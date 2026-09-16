#pragma once

#include <string>

struct AppConfig {
    std::string repo = "11011010/bpatches";
    int timeout_seconds = 5;
    bool show_ui = true;
    bool only_mpq = true;
    std::string custom_api_url;

    static AppConfig load(const std::string& ini_path);
};
