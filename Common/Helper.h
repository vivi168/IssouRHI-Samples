#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

std::vector<std::byte> ReadData(const std::filesystem::path& file);
