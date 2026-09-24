#include <IssouRHI.h>
#include "Camera.h"

#include <algorithm>

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

static std::vector<std::byte> ReadData(const std::filesystem::path& file)
{
  std::ifstream in(file, std::ios::binary | std::ios::ate);
  if (!in) {
    throw std::runtime_error("Failed to open file");
  }

  std::streamsize size = in.tellg();
  if (size < 0) {
    throw std::runtime_error("Failed to determine file size");
  }

  std::vector<std::byte> buffer(static_cast<size_t>(size));

  in.seekg(0, std::ios::beg);
  if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) {
    throw std::runtime_error("Failed to read file");
  }

  return buffer;
}

static void ErrorCallback(int, const char* description)
{
  fprintf(stderr, "Error: %s\n", description);
}

static void KeyCallback(GLFWwindow* window, int key, int, int action, int)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

int main()
{
  // Init window
  GLFWwindow* window;
  glfwSetErrorCallback(ErrorCallback);

  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window = glfwCreateWindow(800, 600, "HelloCube", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    return -1;
  }

  glfwSetKeyCallback(window, KeyCallback);

  using namespace DirectX;

  // Keep GPU resources scoped inside the window lifetime.
  {
    // Create Device
    auto g_Device = IssouRHI::Device::CreateDevice(IssouRHI::Backend::D3D12, {});

    // Create Surface
    auto g_Surface = g_Device->CreateSurface(glfwGetWin32Window(window));
    IssouRHI::SurfaceConfiguration config{
        .format = IssouRHI::TextureFormat::RGBA8Unorm,
        .width = 800,
        .height = 600,
        .bufferCount = 3,
    };
    g_Surface->Configure(config);

    // Create Pipeline
    const std::filesystem::path shaderDirectory = SAMPLE_SHADER_DIRECTORY;
    auto vertexShaderBlob = ReadData(shaderDirectory / "HelloCube.vs.cso");
    auto pixelShaderBlob = ReadData(shaderDirectory / "HelloCube.ps.cso");

    auto vertexShaderLib = g_Device->CreateShaderLibrary(vertexShaderBlob);
    auto pixelShaderLib = g_Device->CreateShaderLibrary(pixelShaderBlob);

    IssouRHI::ShaderModule shaderModules[] = {
        {.library = vertexShaderLib.get(), .stage = IssouRHI::ShaderStage::Vertex, .entryPointName = "VSMain"},
        {.library = pixelShaderLib.get(), .stage = IssouRHI::ShaderStage::Fragment, .entryPointName = "PSMain"},
    };

    IssouRHI::ColorTargetState targets[] = {{
        .format = IssouRHI::TextureFormat::RGBA8Unorm,
    }};
    auto g_RenderPipeline = g_Device->CreateRenderPipeline({
        .label = "Cube pipeline",
        .shaders = shaderModules,
        .targets = targets,
        .depthStencil = {
            .format = IssouRHI::TextureFormat::Depth32Float,
            .depthCompare = IssouRHI::CompareFunction::Less,
            .depthWriteEnabled = true,
        },
    });

    std::vector<std::shared_ptr<IssouRHI::Texture>> depthTextures;
    auto createDepthTextures = [&] {
      depthTextures.clear();
      for (uint32_t i = 0; i < config.bufferCount; ++i) {
        auto texture = g_Device->CreateTexture({
            .label = "Cube depth",
            .size = {.width = config.width, .height = config.height},
            .format = IssouRHI::TextureFormat::Depth32Float,
            .usage = IssouRHI::TextureUsage::RenderAttachment,
        });
        depthTextures.push_back(texture);
      }
    };
    createDepthTextures();

    Camera camera;
    camera.Translate(0.0f, 0.0f, 5.0f);
    camera.Target(0.0f, 0.0f, 0.0f);

    double previousTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      if (glfwWindowShouldClose(window)) break;
      const double now = glfwGetTime();
      const float dt = static_cast<float>(std::min(now - previousTime, 0.1));
      previousTime = now;

      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      if (width == 0 || height == 0) {
        glfwWaitEvents();
        continue;
      }

      auto queue = g_Device->GetQueue();
      if (config.width != static_cast<uint32_t>(width) || config.height != static_cast<uint32_t>(height)) {
        queue->WaitForAll();
        config.width = static_cast<uint32_t>(width);
        config.height = static_cast<uint32_t>(height);
        g_Surface->Configure(config);
        createDepthTextures();
      }

      camera.ProcessKeyboard(window, dt);
      const float aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
      const XMMATRIX model = XMMatrixRotationY(static_cast<float>(now) * 0.5f) * XMMatrixRotationX(0.3f);
      const XMMATRIX projection = XMMatrixPerspectiveFovRH(XM_PIDIV4, aspect, 0.1f, 100.0f);
      XMFLOAT4X4 modelViewProjection;
      XMStoreFloat4x4(&modelViewProjection, XMMatrixTranspose(model * camera.LookAt() * projection));

      auto renderTarget = g_Surface->GetCurrentTexture();
      auto renderTargetView = renderTarget->CreateView();
      auto depthView = depthTextures[g_Surface->CurrentFrameIndex()]->CreateView();

      auto encoder = queue->CreateCommandEncoder();

      constexpr IssouRHI::StageAccessLayout presentState{
          IssouRHI::PipelineStage::None,
          IssouRHI::Access::None,
          IssouRHI::TextureLayout::Present,
      };
      constexpr IssouRHI::StageAccessLayout colorAttachmentState{
          IssouRHI::PipelineStage::ColorAttachment,
          IssouRHI::Access::ColorAttachmentWrite,
          IssouRHI::TextureLayout::ColorAttachment,
      };

      {
        std::array transitions{
            IssouRHI::TextureBarrierDesc{renderTarget.get(), presentState, colorAttachmentState},
        };
        encoder->Barrier({.textures = transitions});
      }

      // Record render pass
      {
        std::array targets{
            IssouRHI::ColorAttachment{
                .view = renderTargetView.get(),
                .clearValue = {0.0f, 0.2f, 0.4f, 1.0f},
            },
        };
        auto passEncoder = encoder->BeginRenderPass({
            .label = "Cube pass",
            .colorAttachments = targets,
            .depthStencilAttachment = {
                .view = depthView.get(),
                .depthClearValue = 1.0f,
                .stencilLoadOp = IssouRHI::LoadOp::DontCare,
            },
        });

        passEncoder->SetPipeline(g_RenderPipeline.get());

        passEncoder->Draw(modelViewProjection, 36);
        passEncoder->End();
      }

      {
        std::array transitions{
            IssouRHI::TextureBarrierDesc{renderTarget.get(), colorAttachmentState, presentState},
        };
        encoder->Barrier({.textures = transitions});
      }

      // Submit work
      IssouRHI::CommandBuffer* cb[] = {encoder->Finish()};
      queue->Submit(cb);

      g_Surface->Present();
    }

    g_Device->GetQueue()->WaitForAll();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}
