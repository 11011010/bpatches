#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace net {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::string error;
    bool success = false;
};

using ProgressCallback = std::function<bool(int64_t downloaded_bytes, int64_t total_bytes)>;

class HttpClient {
public:
    static HttpResponse get(const std::string& url, int timeout_seconds = 5, const std::string& user_agent = "bpatches-updater/1.0");
    static bool download_file(const std::string& url, const std::string& destination_path, int timeout_seconds = 10,
                             ProgressCallback progress_callback = nullptr, std::string* error_out = nullptr,
                             const std::string& user_agent = "bpatches-updater/1.0");
};

} // namespace net
