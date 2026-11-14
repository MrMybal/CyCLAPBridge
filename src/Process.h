#pragma once
#include <cyclapbridge/Conversion.h>
namespace cybridge::detail {
int runProcess(const std::filesystem::path&, const std::vector<std::string>&, const LogSink&);
std::string utf8(const std::filesystem::path&);
std::filesystem::path fromUtf8(const std::string&);
}
