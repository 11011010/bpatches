#include "http_client.h"
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <iostream>

#pragma comment(lib, "wininet.lib")

namespace net {

namespace {

class ScopedHInternet {
    HINTERNET h_ = nullptr;
public:
    ScopedHInternet(HINTERNET h) : h_(h) {}
    ~ScopedHInternet() {
        if (h_) InternetCloseHandle(h_);
    }
    HINTERNET get() const { return h_; }
    operator HINTERNET() const { return h_; }
};

void set_timeouts(HINTERNET h, int timeout_seconds) {
    DWORD timeout_ms = static_cast<DWORD>(timeout_seconds * 1000);
    InternetSetOptionA(h, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionA(h, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionA(h, INTERNET_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
}

} // anonymous namespace

HttpResponse HttpClient::get(const std::string& url, int timeout_seconds, const std::string& user_agent) {
    HttpResponse res;
    ScopedHInternet h_session = InternetOpenA(user_agent.c_str(), INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!h_session) {
        res.error = "InternetOpen failed: " + std::to_string(GetLastError());
        return res;
    }

    set_timeouts(h_session, timeout_seconds);

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE |
                  INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;

    std::string headers = "User-Agent: " + user_agent + "\r\nAccept: application/vnd.github+json, application/json\r\n";

    ScopedHInternet h_url = InternetOpenUrlA(h_session, url.c_str(), headers.c_str(), -1, flags, 0);
    if (!h_url) {
        res.error = "InternetOpenUrl failed: " + std::to_string(GetLastError());
        return res;
    }

    DWORD status_code = 0;
    DWORD status_len = sizeof(status_code);
    if (HttpQueryInfoA(h_url, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status_code, &status_len, nullptr)) {
        res.status_code = static_cast<int>(status_code);
    }

    char buffer[8192];
    DWORD bytes_read = 0;
    while (InternetReadFile(h_url, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        res.body.append(buffer, bytes_read);
    }

    res.success = (res.status_code >= 200 && res.status_code < 300);
    return res;
}

bool HttpClient::download_file(const std::string& url, const std::string& destination_path, int timeout_seconds,
                              ProgressCallback progress_callback, std::string* error_out,
                              const std::string& user_agent) {
    ScopedHInternet h_session = InternetOpenA(user_agent.c_str(), INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!h_session) {
        if (error_out) *error_out = "InternetOpen failed: " + std::to_string(GetLastError());
        return false;
    }

    set_timeouts(h_session, timeout_seconds);

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE |
                  INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;

    std::string headers = "User-Agent: " + user_agent + "\r\nAccept: application/octet-stream, */*\r\n";

    ScopedHInternet h_url = InternetOpenUrlA(h_session, url.c_str(), headers.c_str(), -1, flags, 0);
    if (!h_url) {
        if (error_out) *error_out = "InternetOpenUrl failed for " + url + " error: " + std::to_string(GetLastError());
        return false;
    }

    DWORD status_code = 0;
    DWORD status_len = sizeof(status_code);
    if (HttpQueryInfoA(h_url, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status_code, &status_len, nullptr)) {
        if (status_code < 200 || status_code >= 300) {
            if (error_out) *error_out = "HTTP status code " + std::to_string(status_code) + " while downloading " + url;
            return false;
        }
    }

    int64_t content_length = -1;
    DWORD clen = 0;
    DWORD clen_len = sizeof(clen);
    if (HttpQueryInfoA(h_url, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &clen, &clen_len, nullptr)) {
        content_length = static_cast<int64_t>(clen);
    }

    std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        if (error_out) *error_out = "Failed to open destination file: " + destination_path;
        return false;
    }

    char buffer[65536];
    DWORD bytes_read = 0;
    int64_t downloaded_total = 0;

    while (InternetReadFile(h_url, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        out.write(buffer, bytes_read);
        downloaded_total += bytes_read;

        if (progress_callback) {
            bool keep_going = progress_callback(downloaded_total, content_length);
            if (!keep_going) {
                out.close();
                DeleteFileA(destination_path.c_str());
                if (error_out) *error_out = "Download cancelled by user";
                return false;
            }
        }
    }

    out.close();
    return true;
}

} // namespace net
