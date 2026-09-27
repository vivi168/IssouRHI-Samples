#include "Helper.h"

#include <fstream>
#include <stdexcept>

std::vector<std::byte> ReadData(const std::filesystem::path& file)
{
  std::ifstream in(file, std::ios::binary | std::ios::ate);
  if (!in) {
    throw std::runtime_error("Failed to open file: " + file.string());
  }

  const std::streamsize size = in.tellg();
  if (size < 0) {
    throw std::runtime_error("Failed to determine file size: " + file.string());
  }

  std::vector<std::byte> buffer(static_cast<size_t>(size));
  in.seekg(0, std::ios::beg);
  if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) {
    throw std::runtime_error("Failed to read file: " + file.string());
  }

  return buffer;
}
