#include "App.h"
#include "Helper.h"

#include <array>

class HelloTriangle final : public App
{
public:
  HelloTriangle() : App("HelloTriangle") {}

private:
  void OnInit() override
  {
    m_Config.width = Width();
    m_Config.height = Height();
    Surface().Configure(m_Config);

    const std::filesystem::path shaders = SAMPLE_SHADER_DIRECTORY;
    auto vertexData = ReadData(shaders / "HelloTriangle.vs.cso");
    auto pixelData = ReadData(shaders / "HelloTriangle.ps.cso");
    auto vertexShader = Device().CreateShaderLibrary(vertexData);
    auto pixelShader = Device().CreateShaderLibrary(pixelData);
    IssouRHI::ShaderModule modules[] = {
        {.library = vertexShader.get(), .stage = IssouRHI::ShaderStage::Vertex, .entryPointName = "VSMain"},
        {.library = pixelShader.get(), .stage = IssouRHI::ShaderStage::Fragment, .entryPointName = "PSMain"},
    };
    IssouRHI::ColorTargetState targets[] = {{.format = m_Config.format}};
    m_Pipeline = Device().CreateRenderPipeline({
        .label = "Triangle pipeline",
        .shaders = modules,
        .targets = targets,
    });
  }

  void OnResize() override
  {
    Device().GetQueue()->WaitForAll();
    m_Config.width = Width();
    m_Config.height = Height();
    Surface().Configure(m_Config);
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
    std::array before{IssouRHI::TextureBarrierDesc{target.get(), present, attachment}};
    encoder->Barrier({.textures = before});

    std::array targets{
        IssouRHI::ColorAttachment{
            .view = view.get(),
            .clearValue = {0.0f, 0.2f, 0.4f, 1.0f},
        },
    };
    auto pass = encoder->BeginRenderPass({
        .label = "Triangle pass",
        .colorAttachments = targets,
    });
    pass->SetPipeline(m_Pipeline.get());

    const auto& config = m_Config;
    const float extent = static_cast<float>(std::min(config.width, config.height));
    const std::array<float, 2> scale{
        extent / static_cast<float>(config.width),
        extent / static_cast<float>(config.height),
    };
    pass->Draw(scale, 3);
    pass->End();

    std::array after{IssouRHI::TextureBarrierDesc{target.get(), attachment, present}};
    encoder->Barrier({.textures = after});
    IssouRHI::CommandBuffer* commands[] = {encoder->Finish()};
    queue->Submit(commands);
    Surface().Present();
  }

  IssouRHI::SurfaceConfiguration m_Config{
      .format = IssouRHI::TextureFormat::RGBA8Unorm,
      .width = 800,
      .height = 600,
      .bufferCount = 3,
  };
  std::shared_ptr<IssouRHI::RenderPipeline> m_Pipeline;
};

int main()
{
  HelloTriangle triangle;
  return triangle.Run();
}
