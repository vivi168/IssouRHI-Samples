#include "Heightfield.h"
#include "Helper.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

using namespace IssouRHI;

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

static constexpr StageAccessLayout UndefinedState{
    PipelineStage::None,
    Access::None,
    TextureLayout::Undefined,
};
static constexpr StageAccessLayout WriteState{
    PipelineStage::ComputeShader,
    Access::ShaderResourceStorage,
    TextureLayout::ShaderResourceStorage,
};
static constexpr StageAccessLayout ReadState{
    PipelineStage::ComputeShader | PipelineStage::VertexShader | PipelineStage::MeshShader | PipelineStage::FragmentShader,
    Access::ShaderResource,
    TextureLayout::ShaderResource,
};
static constexpr StageAccessLayout UploadedState{
    PipelineStage::None,
    Access::None,
    TextureLayout::General,
};

static std::shared_ptr<ComputePipeline> CreateComputePipeline(Device& device, const std::filesystem::path& path)
{
  auto bytes = ReadData(path);
  auto library = device.CreateShaderLibrary(bytes);
  return device.CreateComputePipeline({
      .label = path.stem().string(),
      .shader = {.library = library.get(), .stage = ShaderStage::Compute, .entryPointName = "CSMain"},
  });
}

void Heightfield::Init(Device& device, const TerrainInformation& information)
{
  const auto path = std::filesystem::path(TERRAIN_ASSET_DIRECTORY) / information.imageName;
  auto bytes = ReadData(path);
  constexpr uint64_t rowPitch = uint64_t(TerrainHeightfieldSize) * sizeof(uint16_t);
  assert(bytes.size() == rowPitch * TerrainHeightfieldSize); // expect 4096x4096 R16Unorm

  m_InformationBuffer = device.CreateBuffer({
      .label = "Terrain information",
      .size = sizeof(TerrainMeasurements),
      .usage = BufferUsage::MapWrite,
  });
  m_InformationBuffer->Write(FullBufferRange, &information.measurements);
  m_InformationId = m_InformationBuffer->DescriptorIndex({
      .access = BufferAccess::Read,
      .elementStride = sizeof(TerrainMeasurements),
  });

  auto source = device.CreateTexture({
      .label = "Terrain source heights",
      .size = {TerrainHeightfieldSize, TerrainHeightfieldSize},
      .format = TextureFormat::R16Unorm,
      .usage = TextureUsage::CopyDst | TextureUsage::TextureBinding,
  });
  std::array subresources{TextureSubresource{rowPitch, rowPitch * TerrainHeightfieldSize, bytes.data()}};
  source->Write(subresources);
  const uint32_t imageSourceId = source->CreateView()->DescriptorIndex(TextureAccess::Read);

  const std::filesystem::path shaders = SAMPLE_SHADER_DIRECTORY;
  auto generateHeightfieldPipeline = CreateComputePipeline(device, shaders / "TerrainHeightfield.cs.cso");
  auto generateMipPipeline = CreateComputePipeline(device, shaders / "TerrainMip.cs.cso");

  auto queue = device.GetQueue();
  auto encoder = queue->CreateCommandEncoder();

  std::array sourceBefore{TextureBarrierDesc{source.get(), UploadedState, ReadState}};
  encoder->Barrier({.textures = sourceBefore});

  constexpr uint32_t mipCount = TerrainHeightfieldMipCount;

  m_Texture = device.CreateTexture({
      .label = "Terrain heightfield",
      .size = {TerrainHeightfieldSize, TerrainHeightfieldSize},
      .mipLevelCount = mipCount,
      .format = TextureFormat::RGBA16Float, // XYZ: normal, W: metres above sea level
      .usage = TextureUsage::TextureBinding | TextureUsage::StorageBinding,
  });

  const SubresourceRange baseRange{
      .baseMipLevel = 0,
      .mipLevelCount = 1,
  };
  auto baseView = m_Texture->CreateView({
      .format = TextureFormat::RGBA16Float,
      .dimension = TextureViewDimension::Texture2D,
      .range = baseRange,
  });
  uint32_t mipSourceId = baseView->DescriptorIndex(TextureAccess::Read);

  // Generate terrain heightfield
  {
    std::array baseBefore{TextureBarrierDesc{m_Texture.get(), UndefinedState, WriteState, baseRange}};
    encoder->Barrier({.textures = baseBefore});

    auto basePass = encoder->BeginComputePass({.label = "Generate terrain heightfield"});
    basePass->SetPipeline(generateHeightfieldPipeline.get());
    const TerrainGenerateArgs baseDispatch{
        .sourceId = imageSourceId,
        .destinationId = baseView->DescriptorIndex(TextureAccess::ReadWrite),
        .terrainInformationId = m_InformationId,
    };
    basePass->Dispatch(baseDispatch, (TerrainHeightfieldSize + 7) / 8, (TerrainHeightfieldSize + 7) / 8);
    basePass->End();

    std::array baseAfter{TextureBarrierDesc{m_Texture.get(), WriteState, ReadState, baseRange}};
    encoder->Barrier({.textures = baseAfter});
  }

  // Generate mip
  for (uint32_t level = 1; level < mipCount; ++level) {
    const uint32_t width = std::max(1u, TerrainHeightfieldSize >> level);
    const uint32_t height = std::max(1u, TerrainHeightfieldSize >> level);
    const SubresourceRange range{
        .baseMipLevel = level,
        .mipLevelCount = 1,
    };

    auto view = m_Texture->CreateView({
        .format = TextureFormat::RGBA16Float,
        .dimension = TextureViewDimension::Texture2D,
        .range = range,
    });

    {
      std::array before{TextureBarrierDesc{m_Texture.get(), UndefinedState, WriteState, range}};
      encoder->Barrier({.textures = before});

      auto pass = encoder->BeginComputePass({.label = "Generate terrain mip"});
      pass->SetPipeline(generateMipPipeline.get());
      const TerrainMipArgs dispatch{
          .sourceId = mipSourceId,
          .destinationId = view->DescriptorIndex(TextureAccess::ReadWrite),
          .width = width,
          .height = height,
      };
      pass->Dispatch(dispatch, (width + 7) / 8, (height + 7) / 8);
      pass->End();

      std::array after{TextureBarrierDesc{m_Texture.get(), WriteState, ReadState, range}};
      encoder->Barrier({.textures = after});
    }

    mipSourceId = view->DescriptorIndex(TextureAccess::Read);
  }

  m_TextureId = m_Texture->CreateView()->DescriptorIndex(TextureAccess::Read);

  CommandBuffer* commands[] = {encoder->Finish()};
  queue->Submit(commands);
  queue->WaitForAll();
}
