#include "App.h"
#include "Camera.h"
#include "Heightfield.h"
#include "Helper.h"
#include "Shared.h"
#include "TerrainMesh.h"

#include <imgui.h>

#include <algorithm>
#include <array>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace DirectX;

static constexpr IssouRHI::StageAccessLayout PresentState{
    IssouRHI::PipelineStage::None,
    IssouRHI::Access::None,
    IssouRHI::TextureLayout::Present,
};
static constexpr IssouRHI::StageAccessLayout AttachmentState{
    IssouRHI::PipelineStage::ColorAttachment,
    IssouRHI::Access::ColorAttachmentWrite,
    IssouRHI::TextureLayout::ColorAttachment,
};

class Terrain final : public App
{
public:
  Terrain() : App("Terrain", 1920, 1080) {}

private:
  static constexpr uint32_t FrameCount = 3;

  enum PipelineMode {
    Vertex,
    Mesh,
    Task,
  };

  static constexpr const char* PipelineNames[] = {"Vertex", "Mesh", "Task + Mesh"};

  struct FrameResources {
    std::shared_ptr<IssouRHI::Buffer> timestampReadback;
    int pipeline = -1;
  };

  void OnInit() override
  {
    m_SurfaceConfig = {
        .format = IssouRHI::TextureFormat::RGBA8Unorm,
        .width = Width(),
        .height = Height(),
        .bufferCount = FrameCount,
    };
    Surface().Configure(m_SurfaceConfig);

    InitImGui(m_SurfaceConfig);
    InitQueries();
    BuildPipelines();

    CreateDepthTexture();

    m_LinearClampSampler = Device().CreateSampler({
        .label = "Terrain linear clamp",
        .minFilter = IssouRHI::FilterMode::Linear,
        .magFilter = IssouRHI::FilterMode::Linear,
        .mipmapFilter = IssouRHI::FilterMode::Linear,
    });
    m_DrawArgs.samplerId = m_LinearClampSampler->DescriptorIndex();

    {
      auto mesh = TerrainMesh::BuildMesh();
      m_VertexBuffer = Device().CreateBuffer({
          .label = "Terrain vertices",
          .size = mesh.vertices.size() * sizeof(mesh.vertices[0]),
          .usage = IssouRHI::BufferUsage::MapWrite,
      });
      m_VertexBuffer->Write(IssouRHI::FullBufferRange, mesh.vertices.data());

      m_IndexBuffer = Device().CreateBuffer({
          .label = "Terrain indices",
          .size = mesh.indices.size() * sizeof(mesh.indices[0]),
          .usage = IssouRHI::BufferUsage::MapWrite,
      });
      m_IndexBuffer->Write(IssouRHI::FullBufferRange, mesh.indices.data());

      m_DrawArgs.verticesId = m_VertexBuffer->DescriptorIndex({.access = IssouRHI::BufferAccess::Read, .elementStride = sizeof(Point)});
      m_DrawArgs.indicesId = m_IndexBuffer->DescriptorIndex({.access = IssouRHI::BufferAccess::Read, .elementStride = sizeof(uint32_t)});
      m_IndexCount = static_cast<uint32_t>(mesh.indices.size());
    }

    m_TerrainInformation = TerrainInformation::ReadFromFile(std::filesystem::path(TERRAIN_ASSET_DIRECTORY) / "grandballon.txt");

    // Heightfield creation
    {
      m_Heightfield.Init(Device(), m_TerrainInformation);
      m_DrawArgs.heightfieldId = m_Heightfield.TextureId();
      m_DrawArgs.terrainInformationId = m_Heightfield.InformationId();
    }

    // Camera setup
    {
      const float centerX = 0.5f * (TerrainHeightfieldSize - 1) * m_TerrainInformation.measurements.metersPerTexel;
      const float centerZ = 0.5f * (TerrainHeightfieldSize - 1) * m_TerrainInformation.measurements.metersPerTexel;
      m_WorldExtent = 2.0f * std::max(centerX, centerZ);
      m_Camera.Translate(m_WorldPosition.x + centerX, m_WorldPosition.y + m_TerrainInformation.measurements.maxElevation + m_WorldExtent * 0.1f, m_WorldPosition.z + centerZ + m_WorldExtent * 0.3f);
      m_Camera.Target(m_WorldPosition.x + centerX, m_WorldPosition.y + (m_TerrainInformation.measurements.minElevation + m_TerrainInformation.measurements.maxElevation) * 0.5f, m_WorldPosition.z + centerZ);
    }
  }

  void InitQueries()
  {
    m_TimestampQueries = Device().CreateQuerySet({
        .label = "Terrain timestamps",
        .type = IssouRHI::QueryType::Timestamp,
        .count = 2,
    });
    for (auto& frameResources : m_FrameResources) {
      frameResources.timestampReadback = Device().CreateBuffer({
          .label = "Terrain timestamp readback",
          .size = 2 * sizeof(uint64_t),
          .usage = IssouRHI::BufferUsage::MapRead | IssouRHI::BufferUsage::QueryResolve,
      });
    }
  }

  void DrawGui(uint32_t frameIndex)
  {
    auto& frameResources = m_FrameResources[frameIndex];
    if (ImGui::Begin("Terrain")) {

      ImGui::TextUnformatted("Pipeline");

      for (int pipeline = Vertex; pipeline <= Task; ++pipeline) {
        ImGui::RadioButton(PipelineNames[pipeline], &m_Pipeline, pipeline);
      }

      ImGui::Separator();
      ImGui::TextUnformatted("GPU timestamp query (terrain pass)");

      if (frameResources.pipeline >= 0) {
        std::array<uint64_t, 2> timestamps{};
        frameResources.timestampReadback->Read(IssouRHI::FullBufferRange, timestamps.data());
        const uint64_t ticks = timestamps[1] - timestamps[0];
        const double milliseconds = static_cast<double>(ticks) / static_cast<double>(Device().TimestampFrequencyHz()) * 1000.0;
        ImGui::Text("Measured pipeline: %s", PipelineNames[frameResources.pipeline]);
        ImGui::Text("GPU time: %.4f ms", milliseconds);
      } else {
        ImGui::TextUnformatted("Waiting for GPU results...");
      }

      ImGui::Text("Frame rate: %.1f FPS", ImGui::GetIO().Framerate);
    }
    ImGui::End();
  }

  void BuildPipelines()
  {
    const std::filesystem::path shaders = SAMPLE_SHADER_DIRECTORY;
    auto pixelBlob = ReadData(shaders / "Terrain.ps.cso");
    auto pixelShader = Device().CreateShaderLibrary(pixelBlob);
    const IssouRHI::ShaderModule pixelModule{.library = pixelShader.get(), .stage = IssouRHI::ShaderStage::Fragment, .entryPointName = "PSMain"};

    IssouRHI::ColorTargetState targets[] = {{.format = m_SurfaceConfig.format}};

    IssouRHI::RenderPipelineDesc pipelineDesc{};
    pipelineDesc.targets = targets,
    pipelineDesc.depthStencil = {
        .format = IssouRHI::TextureFormat::Depth32Float,
        .depthCompare = IssouRHI::CompareFunction::Greater,
        .depthWriteEnabled = true,
    },
    pipelineDesc.primitive = {
        .frontFace = IssouRHI::FrontFace::CCW,
        .cullMode = IssouRHI::CullMode::Back,
        // .polygonMode = IssouRHI::PolygonMode::Line,
    };

    {
      auto vertexBlob = ReadData(shaders / "Terrain.vs.cso");
      auto vertexShader = Device().CreateShaderLibrary(vertexBlob);

      IssouRHI::ShaderModule shaderModules[] = {
          {.library = vertexShader.get(), .stage = IssouRHI::ShaderStage::Vertex, .entryPointName = "VSMain"},
          pixelModule,
      };

      pipelineDesc.label = "Terrain vertex pipeline";
      pipelineDesc.shaders = shaderModules;
      m_VertexPipeline = Device().CreateRenderPipeline(pipelineDesc);
    }

    {
      auto meshBlob = ReadData(shaders / "Terrain.ms.cso");
      auto meshShader = Device().CreateShaderLibrary(meshBlob);

      IssouRHI::ShaderModule shaderModules[] = {
          {.library = meshShader.get(), .stage = IssouRHI::ShaderStage::Mesh, .entryPointName = "MSMain"},
          pixelModule,
      };

      pipelineDesc.label = "Terrain mesh pipeline";
      pipelineDesc.shaders = shaderModules;
      m_MeshPipeline = Device().CreateMeshPipeline(pipelineDesc);
    }

    {
      auto meshBlob = ReadData(shaders / "TerrainTasked.ms.cso");
      auto meshShader = Device().CreateShaderLibrary(meshBlob);

      auto taskBlob = ReadData(shaders / "Terrain.as.cso");
      auto taskShader = Device().CreateShaderLibrary(taskBlob);

      IssouRHI::ShaderModule shaderModules[] = {
          {.library = taskShader.get(), .stage = IssouRHI::ShaderStage::Task, .entryPointName = "ASMain"},
          {.library = meshShader.get(), .stage = IssouRHI::ShaderStage::Mesh, .entryPointName = "MSMain"},
          pixelModule,
      };

      pipelineDesc.label = "Terrain task+mesh pipeline";
      pipelineDesc.shaders = shaderModules;
      m_AmpPipeline = Device().CreateMeshPipeline(pipelineDesc);
    }
  }

  void CreateDepthTexture()
  {
    m_DepthTexture = Device().CreateTexture({
        .label = "Terrain depth",
        .size = {.width = m_SurfaceConfig.width, .height = m_SurfaceConfig.height},
        .format = IssouRHI::TextureFormat::Depth32Float,
        .usage = IssouRHI::TextureUsage::RenderAttachment,
    });
  }

  void OnUpdate(float dt, uint32_t frameIndex) override
  {
    DrawGui(frameIndex);
    if (!ImGui::GetIO().WantCaptureKeyboard) {
      const auto pressed = [this](int key) {
        return glfwGetKey(Window(), key) == GLFW_PRESS ? 1.0f : 0.0f;
      };
      const float moveStep = 800.0f * dt;
      const float turnStep = 2.0f * dt;

      const float pitch = pressed(GLFW_KEY_UP) - pressed(GLFW_KEY_DOWN);
      const float yaw = pressed(GLFW_KEY_LEFT) - pressed(GLFW_KEY_RIGHT);
      const float right = pressed(GLFW_KEY_D) - pressed(GLFW_KEY_A);
      const float up = pressed(GLFW_KEY_E) - pressed(GLFW_KEY_Q);
      const float forward = pressed(GLFW_KEY_W) - pressed(GLFW_KEY_S);

      m_Camera.RotateAndMove(pitch * turnStep, yaw * turnStep, right * moveStep, up * moveStep, forward * moveStep);
    }

    const auto relative = m_Camera.WorldToLocal(m_WorldPosition);
    m_DrawArgs.terrainOrigin = {relative.x, relative.y, relative.z, 0.0f};
    // reversed Z: near maps to 1, far to 0
    const XMMATRIX projection = XMMatrixPerspectiveFovRH(XM_PIDIV4, AspectRatio(), m_WorldExtent * 3.0f, 0.1f);
    XMStoreFloat4x4(&m_DrawArgs.viewProjection, XMMatrixTranspose(m_Camera.LookAt() * projection));
  }

  void OnResize() override
  {
    Device().GetQueue()->WaitForAll();

    m_SurfaceConfig.width = Width();
    m_SurfaceConfig.height = Height();
    Surface().Configure(m_SurfaceConfig);

    CreateDepthTexture();
  }

  void OnRender(IssouRHI::Texture* target) override
  {
    auto queue = Device().GetQueue();
    auto view = target->CreateView();
    auto& frameResources = m_FrameResources[Surface().CurrentFrameIndex()];
    auto encoder = queue->CreateCommandEncoder();

    {
      std::array before{IssouRHI::TextureBarrierDesc{target, PresentState, AttachmentState}};
      encoder->Barrier({.textures = before});

      std::array targets{
          IssouRHI::ColorAttachment{
              .view = view.get(),
              .clearValue = {0.0f, 0.2f, 0.4f, 1.0f},
          },
      };
      auto pass = encoder->BeginRenderPass({
          .label = "Terrain forward pass",
          .colorAttachments = targets,
          .depthStencilAttachment = {
              .view = m_DepthTexture->CreateView().get(),
              .depthClearValue = 0.0f,  // Reverse-Z.
              .stencilLoadOp = IssouRHI::LoadOp::DontCare,
          },
          .timestampWrites = IssouRHI::TimestampWrites{
              .beginningOfPassWriteIndex = 0,
              .endOfPassWriteIndex = 1,
              .querySet = m_TimestampQueries.get(),
          },
      });
      if (m_Pipeline != Vertex) {
        if (m_Pipeline == Task) {
          pass->SetPipeline(m_AmpPipeline.get());
          constexpr uint32_t groupCount = (TerrainMeshPatchCount + TerrainTaskGroupSize - 1) / TerrainTaskGroupSize;
          pass->DrawMesh(m_DrawArgs, groupCount);
        } else {
          pass->SetPipeline(m_MeshPipeline.get());
          pass->DrawMesh(m_DrawArgs, TerrainMeshPatchCountX, TerrainMeshPatchCountZ, TerrainMeshLODLevels);
        }
      } else {
        pass->SetPipeline(m_VertexPipeline.get());
        pass->Draw(m_DrawArgs, m_IndexCount);
      }
      pass->End();

      encoder->ResolveQuerySet(m_TimestampQueries.get(), 0, 2, frameResources.timestampReadback.get(), 0);

      RenderImGui(*encoder, target);

      std::array after{IssouRHI::TextureBarrierDesc{target, AttachmentState, PresentState}};
      encoder->Barrier({.textures = after});
    }

    IssouRHI::CommandBuffer* commands[] = {encoder->Finish()};
    queue->Submit(commands);

    frameResources.pipeline = m_Pipeline;
    Surface().Present();
  }

  IssouRHI::SurfaceConfiguration m_SurfaceConfig;

  std::shared_ptr<IssouRHI::RenderPipeline> m_VertexPipeline;
  std::shared_ptr<IssouRHI::RenderPipeline> m_MeshPipeline;
  std::shared_ptr<IssouRHI::RenderPipeline> m_AmpPipeline;
  std::shared_ptr<IssouRHI::Sampler> m_LinearClampSampler;
  std::shared_ptr<IssouRHI::Texture> m_DepthTexture;
  std::shared_ptr<IssouRHI::Buffer> m_VertexBuffer;
  std::shared_ptr<IssouRHI::Buffer> m_IndexBuffer;

  std::shared_ptr<IssouRHI::QuerySet> m_TimestampQueries;
  std::array<FrameResources, FrameCount> m_FrameResources;
  int m_Pipeline = Task;
  uint32_t m_IndexCount = 0;
  TerrainDrawArgs m_DrawArgs{};

  Camera m_Camera;
  Heightfield m_Heightfield;
  WorldPosition m_WorldPosition{};
  TerrainInformation m_TerrainInformation;
  float m_WorldExtent = 1.0f;
};

int main()
{
  Terrain terrain;
  return terrain.Run();
}
