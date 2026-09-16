#include "config.h"
#include <windows.h>

AppConfig AppConfig::load(const std::string& ini_path) {
    AppConfig cfg;
    char buf[512] = { 0 };

    if (GetPrivateProfileStringA("bpatches", "Repo", "11011010/bpatches", buf, sizeof(buf), ini_path.c_str()) > 0) {
        cfg.repo = buf;
    }

    cfg.timeout_seconds = GetPrivateProfileIntA("bpatches", "TimeoutSeconds", 5, ini_path.c_str());
    if (cfg.timeout_seconds <= 0) cfg.timeout_seconds = 5;

    cfg.show_ui = GetPrivateProfileIntA("bpatches", "ShowUI", 1, ini_path.c_str()) != 0;
    cfg.only_mpq = GetPrivateProfileIntA("bpatches", "OnlyMpq", 1, ini_path.c_str()) != 0;

    if (GetPrivateProfileStringA("bpatches", "ApiUrl", "", buf, sizeof(buf), ini_path.c_str()) > 0) {
        cfg.custom_api_url = buf;
    }

    return cfg;
}
