#pragma once

#include "Shared.h"

#include <IssouRHI.h>

#include <filesystem>
#include <string>

struct TerrainInformation {
  std::string imageName;
  TerrainMeasurements measurements;

  static TerrainInformation ReadFromFile(const std::filesystem::path& path);
};

class Heightfield
{
public:
  void Init(IssouRHI::Device& device, const TerrainInformation& information);

  uint32_t TextureId() const { return m_TextureId; }
  uint32_t InformationId() const { return m_InformationId; }

private:
  std::shared_ptr<IssouRHI::Texture> m_Texture;
  std::shared_ptr<IssouRHI::Buffer> m_InformationBuffer;
  uint32_t m_TextureId = 0;
  uint32_t m_InformationId = 0;
};
