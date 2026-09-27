#include "App.h"
#include "Camera.h"
#include "Helper.h"

#include <array>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace DirectX;

class HelloCube final : public App
{
public:
  HelloCube() : App("HelloCube") {}

private:
  void OnInit() override
  {
    m_SurfaceConfig.width = Width();
    m_SurfaceConfig.height = Height();
    Surface().Configure(m_SurfaceConfig);

    const std::filesystem::path shaders = SAMPLE_SHADER_DIRECTORY;
    auto vertexData = ReadData(shaders / "HelloCube.vs.cso");
    auto pixelData = ReadData(shaders / "HelloCube.ps.cso");
    auto vertexShader = Device().CreateShaderLibrary(vertexData);
    auto pixelShader = Device().CreateShaderLibrary(pixelData);
    IssouRHI::ShaderModule modules[] = {
        {.library = vertexShader.get(), .stage = IssouRHI::ShaderStage::Vertex, .entryPointName = "VSMain"},
        {.library = pixelShader.get(), .stage = IssouRHI::ShaderStage::Fragment, .entryPointName = "PSMain"},
    };

    IssouRHI::ColorTargetState targets[] = {{.format = m_SurfaceConfig.format}};
    m_Pipeline = Device().CreateRenderPipeline({
        .label = "Cube pipeline",
        .shaders = modules,
        .targets = targets,
        .depthStencil = {
            .format = IssouRHI::TextureFormat::Depth32Float,
            .depthCompare = IssouRHI::CompareFunction::Less,
            .depthWriteEnabled = true,
        },
        .primitive = {
              .frontFace = IssouRHI::FrontFace::CCW,
              .cullMode = IssouRHI::CullMode::Back,
          },
    });

    CreateDepthTexture();

    m_Camera.Translate(0.0f, 0.0f, 5.0f);
    m_Camera.Target(0.0f, 0.0f, 0.0f);
  }

  void CreateDepthTexture()
  {
    m_DepthTexture = Device().CreateTexture({
        .label = "Cube depth",
        .size = {.width = m_SurfaceConfig.width, .height = m_SurfaceConfig.height},
        .format = IssouRHI::TextureFormat::Depth32Float,
        .usage = IssouRHI::TextureUsage::RenderAttachment,
    });
  }

  void OnUpdate(float dt) override
  {
    {
      const auto pressed = [this](int key) {
        return glfwGetKey(Window(), key) == GLFW_PRESS ? 1.0f : 0.0f;
      };
      const float moveStep = 3.0f * dt;
      const float turnStep = 2.0f * dt;

      const float pitch = pressed(GLFW_KEY_UP) - pressed(GLFW_KEY_DOWN);
      const float yaw = pressed(GLFW_KEY_LEFT) - pressed(GLFW_KEY_RIGHT);
      const float right = pressed(GLFW_KEY_D) - pressed(GLFW_KEY_A);
      const float up = pressed(GLFW_KEY_E) - pressed(GLFW_KEY_Q);
      const float forward = pressed(GLFW_KEY_W) - pressed(GLFW_KEY_S);

      m_Camera.RotateAndMove(pitch * turnStep, yaw * turnStep, right * moveStep, up * moveStep, forward * moveStep);
    }

    const XMMATRIX model = XMMatrixRotationY(static_cast<float>(glfwGetTime()) * 0.5f) * XMMatrixRotationX(0.3f);
    const XMMATRIX projection = XMMatrixPerspectiveFovRH(XM_PIDIV4, AspectRatio(), 0.1f, 100.0f);
    XMStoreFloat4x4(&m_ModelViewProjection, XMMatrixTranspose(model * m_Camera.LookAt() * projection));
  }

  void OnResize() override
  {
    Device().GetQueue()->WaitForAll();

    m_SurfaceConfig.width = Width();
    m_SurfaceConfig.height = Height();
    Surface().Configure(m_SurfaceConfig);

    CreateDepthTexture();
  }

  void OnRender() override
  {
    auto queue = Device().GetQueue();
    auto target = Surface().GetCurrentTexture();
    auto view = target->CreateView();
    auto encoder = queue->CreateCommandEncoder();

    constexpr IssouRHI::StageAccessLayout present{
        IssouRHI::PipelineStage::None,
        IssouRHI::Access::None,
        IssouRHI::TextureLayout::Present,
    };
    constexpr IssouRHI::StageAccessLayout attachment{
        IssouRHI::PipelineStage::ColorAttachment,
        IssouRHI::Access::ColorAttachmentWrite,
        IssouRHI::TextureLayout::ColorAttachment,
    };

    {
      std::array before{IssouRHI::TextureBarrierDesc{target.get(), present, attachment}};
      encoder->Barrier({.textures = before});

      std::array targets{
          IssouRHI::ColorAttachment{
              .view = view.get(),
              .clearValue = {0.0f, 0.2f, 0.4f, 1.0f},
          },
      };
      auto pass = encoder->BeginRenderPass({
          .label = "Cube pass",
          .colorAttachments = targets,
          .depthStencilAttachment = {
              .view = m_DepthTexture->CreateView().get(),
              .depthClearValue = 1.0f,
              .stencilLoadOp = IssouRHI::LoadOp::DontCare,
          },
      });
      pass->SetPipeline(m_Pipeline.get());
      pass->Draw(m_ModelViewProjection, 36);
      pass->End();

      std::array after{IssouRHI::TextureBarrierDesc{target.get(), attachment, present}};
      encoder->Barrier({.textures = after});
    }

    IssouRHI::CommandBuffer* commands[] = {encoder->Finish()};
    queue->Submit(commands);

    Surface().Present();
  }

  IssouRHI::SurfaceConfiguration m_SurfaceConfig{
      .format = IssouRHI::TextureFormat::RGBA8Unorm,
      .width = 800,
      .height = 600,
      .bufferCount = 3,
  };
  std::shared_ptr<IssouRHI::RenderPipeline> m_Pipeline;
  std::shared_ptr<IssouRHI::Texture> m_DepthTexture;
  Camera m_Camera;
  DirectX::XMFLOAT4X4 m_ModelViewProjection{};
};

int main()
{
  HelloCube cube;
  return cube.Run();
}
