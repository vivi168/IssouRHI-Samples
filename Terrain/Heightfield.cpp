#include "Heightfield.h"
#include "Helper.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

TerrainInformation TerrainInformation::ReadFromFile(const std::filesystem::path& path)
{
  std::ifstream file(path);
  TerrainInformation information{};
  std::string label;

  file >> label >> information.imageName;
  file >> label >> information.measurements.metersPerTexel;
  file >> label >> information.measurements.minElevation;
  file >> label >> information.measurements.maxElevation;

  information.measurements.heightfieldTexelsPerMeter = 1.0f / information.measurements.metersPerTexel;
  float invHeightfieldSize = 1.0f / static_cast<float>(TerrainHeightfieldSize);
  information.measurements.heightfield_invSize = {invHeightfieldSize, invHeightfieldSize};

  return information;
}

static constexpr IssouRHI::StageAccessLayout UndefinedState{
    IssouRHI::PipelineStage::None,
    IssouRHI::Access::None,
    IssouRHI::TextureLayout::Undefined,
};
static constexpr IssouRHI::StageAccessLayout WriteState{
    IssouRHI::PipelineStage::ComputeShader,
    IssouRHI::Access::ShaderResourceStorage,
    IssouRHI::TextureLayout::ShaderResourceStorage,
};
static constexpr IssouRHI::StageAccessLayout ReadState{
    IssouRHI::PipelineStage::ComputeShader | IssouRHI::PipelineStage::VertexShader | IssouRHI::PipelineStage::MeshShader | IssouRHI::PipelineStage::FragmentShader,
    IssouRHI::Access::ShaderResource,
    IssouRHI::TextureLayout::ShaderResource,
};
static constexpr IssouRHI::StageAccessLayout UploadedState{
    IssouRHI::PipelineStage::None,
    IssouRHI::Access::None,
    IssouRHI::TextureLayout::General,
};

static std::shared_ptr<IssouRHI::ComputePipeline> CreateComputePipeline(IssouRHI::Device& device, const std::filesystem::path& path)
{
  auto bytes = ReadData(path);
  auto library = device.CreateShaderLibrary(bytes);
  return device.CreateComputePipeline({
      .label = path.stem().string(),
      .shader = {.library = library.get(), .stage = IssouRHI::ShaderStage::Compute, .entryPointName = "CSMain"},
  });
}

void Heightfield::Init(IssouRHI::Device& device, const TerrainInformation& information)
{
  const auto path = std::filesystem::path(TERRAIN_ASSET_DIRECTORY) / information.imageName;
  auto bytes = ReadData(path);
  constexpr uint64_t rowPitch = uint64_t(TerrainHeightfieldSize) * sizeof(uint16_t);
  assert(bytes.size() == rowPitch * TerrainHeightfieldSize); // expect 4096x4096 R16Unorm

  m_InformationBuffer = device.CreateBuffer({
      .label = "Terrain information",
      .size = sizeof(TerrainMeasurements),
      .usage = IssouRHI::BufferUsage::MapWrite,
  });
  m_InformationBuffer->Write(IssouRHI::FullBufferRange, &information.measurements);
  m_InformationId = m_InformationBuffer->DescriptorIndex({
      .access = IssouRHI::BufferAccess::Read,
      .elementStride = sizeof(TerrainMeasurements),
  });

  auto source = device.CreateTexture({
      .label = "Terrain source heights",
      .size = {TerrainHeightfieldSize, TerrainHeightfieldSize},
      .format = IssouRHI::TextureFormat::R16Unorm,
      .usage = IssouRHI::TextureUsage::CopyDst | IssouRHI::TextureUsage::TextureBinding,
  });
  std::array subresources{IssouRHI::TextureSubresource{rowPitch, rowPitch * TerrainHeightfieldSize, bytes.data()}};
  source->Write(subresources);
  const uint32_t imageSourceId = source->CreateView()->DescriptorIndex(IssouRHI::TextureAccess::Read);

  const std::filesystem::path shaders = SAMPLE_SHADER_DIRECTORY;
  auto generateHeightfieldPipeline = CreateComputePipeline(device, shaders / "TerrainHeightfield.cs.cso");
  auto generateMipPipeline = CreateComputePipeline(device, shaders / "TerrainMip.cs.cso");

  auto queue = device.GetQueue();
  auto encoder = queue->CreateCommandEncoder();

  std::array sourceBefore{IssouRHI::TextureBarrierDesc{source.get(), UploadedState, ReadState}};
  encoder->Barrier({.textures = sourceBefore});

  constexpr uint32_t mipCount = TerrainHeightfieldMipCount;

  m_Texture = device.CreateTexture({
      .label = "Terrain heightfield",
      .size = {TerrainHeightfieldSize, TerrainHeightfieldSize},
      .mipLevelCount = mipCount,
      .format = IssouRHI::TextureFormat::RGBA16Float, // XYZ: normal, W: metres above sea level
      .usage = IssouRHI::TextureUsage::TextureBinding | IssouRHI::TextureUsage::StorageBinding,
  });

  const IssouRHI::SubresourceRange baseRange{
      .baseMipLevel = 0,
      .mipLevelCount = 1,
  };
  auto baseView = m_Texture->CreateView({
      .format = IssouRHI::TextureFormat::RGBA16Float,
      .dimension = IssouRHI::TextureViewDimension::Texture2D,
      .range = baseRange,
  });
  uint32_t mipSourceId = baseView->DescriptorIndex(IssouRHI::TextureAccess::Read);

  // Generate terrain heightfield
  {
    std::array baseBefore{IssouRHI::TextureBarrierDesc{m_Texture.get(), UndefinedState, WriteState, baseRange}};
    encoder->Barrier({.textures = baseBefore});

    auto basePass = encoder->BeginComputePass({.label = "Generate terrain heightfield"});
    basePass->SetPipeline(generateHeightfieldPipeline.get());
    const TerrainGenerateArgs baseDispatch{
        .sourceId = imageSourceId,
        .destinationId = baseView->DescriptorIndex(IssouRHI::TextureAccess::ReadWrite),
        .terrainInformationId = m_InformationId,
    };
    basePass->Dispatch(baseDispatch, (TerrainHeightfieldSize + 7) / 8, (TerrainHeightfieldSize + 7) / 8);
    basePass->End();

    std::array baseAfter{IssouRHI::TextureBarrierDesc{m_Texture.get(), WriteState, ReadState, baseRange}};
    encoder->Barrier({.textures = baseAfter});
  }

  // Generate mip
  for (uint32_t level = 1; level < mipCount; ++level) {
    const uint32_t width = std::max(1u, TerrainHeightfieldSize >> level);
    const uint32_t height = std::max(1u, TerrainHeightfieldSize >> level);
    const IssouRHI::SubresourceRange range{
        .baseMipLevel = level,
        .mipLevelCount = 1,
    };

    auto view = m_Texture->CreateView({
        .format = IssouRHI::TextureFormat::RGBA16Float,
        .dimension = IssouRHI::TextureViewDimension::Texture2D,
        .range = range,
    });

    {
      std::array before{IssouRHI::TextureBarrierDesc{m_Texture.get(), UndefinedState, WriteState, range}};
      encoder->Barrier({.textures = before});

      auto pass = encoder->BeginComputePass({.label = "Generate terrain mip"});
      pass->SetPipeline(generateMipPipeline.get());
      const TerrainMipArgs dispatch{
          .sourceId = mipSourceId,
          .destinationId = view->DescriptorIndex(IssouRHI::TextureAccess::ReadWrite),
          .width = width,
          .height = height,
      };
      pass->Dispatch(dispatch, (width + 7) / 8, (height + 7) / 8);
      pass->End();

      std::array after{IssouRHI::TextureBarrierDesc{m_Texture.get(), WriteState, ReadState, range}};
      encoder->Barrier({.textures = after});
    }

    mipSourceId = view->DescriptorIndex(IssouRHI::TextureAccess::Read);
  }

  m_TextureId = m_Texture->CreateView()->DescriptorIndex(IssouRHI::TextureAccess::Read);

  IssouRHI::CommandBuffer* commands[] = {encoder->Finish()};
  queue->Submit(commands);
  queue->WaitForAll();
}
