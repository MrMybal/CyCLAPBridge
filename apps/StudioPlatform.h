// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace cybridge::studio {
std::string pathText(const std::filesystem::path&);
std::filesystem::path pathFromText(const std::string&);
std::string fileUrl(const std::filesystem::path&);
bool openUrl(const std::string&);
std::filesystem::path settingsFile();
std::vector<std::string> roots();
}
