#pragma once

#include <string>
#include <cstdint>

namespace crypto {

std::string sha256_file(const std::string& filepath);
std::string sha256_buffer(const void* data, size_t len);

} // namespace crypto
