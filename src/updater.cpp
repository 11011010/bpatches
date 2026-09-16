#include "updater.h"
#include "http_client.h"
#include "json_mini.h"
#include "sha256.h"
#include "ui.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    return to_lower(str.substr(str.size() - suffix.size())) == to_lower(suffix);
}

int64_t get_file_size(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) {
        return -1;
    }
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}

std::string format_bytes(int64_t bytes) {
    char buf[64];
    if (bytes >= 1024 * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "%lld B", static_cast<long long>(bytes));
    }
    return std::string(buf);
}

void log_message(const std::string& root, const std::string& msg) {
    std::string log_path = root + "\\bpatches.log";
    std::ofstream out(log_path, std::ios::app);
    if (out.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char time_buf[64];
        snprintf(time_buf, sizeof(time_buf), "[%04d-%02d-%02d %02d:%02d:%02d] ",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        out << time_buf << msg << "\n";
    }
}

} // anonymous namespace

PatchUpdater::PatchUpdater(HINSTANCE hInstance) : hInstance_(hInstance) {
    root_dir_ = get_game_root();
    std::string ini_path = root_dir_ + "\\bpatches.ini";
    config_ = AppConfig::load(ini_path);
}

std::string PatchUpdater::get_game_root() {
    char path[MAX_PATH] = { 0 };
    if (hInstance_) {
        GetModuleFileNameA(hInstance_, path, MAX_PATH);
    } else {
        GetModuleFileNameA(nullptr, path, MAX_PATH);
    }
    std::string full(path);
    size_t pos = full.find_last_of("\\/");
    if (pos != std::string::npos) {
        return full.substr(0, pos);
    }
    return ".";
}

std::string PatchUpdater::resolve_patch_target(const std::string& patch_name) {
    static const std::vector<std::string> locales = {
        "deDE", "enUS", "enGB", "frFR", "ruRU", "esES", "esMX", "zhCN", "zhTW", "koKR"
    };

    std::string lower_name = to_lower(patch_name);
    for (const auto& loc : locales) {
        if (lower_name.find(to_lower(loc)) != std::string::npos) {
            std::string loc_dir = root_dir_ + "\\Data\\" + loc;
            DWORD attrib = GetFileAttributesA(loc_dir.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                return loc_dir + "\\" + patch_name;
            }
        }
    }

    return root_dir_ + "\\Data\\" + patch_name;
}

std::vector<PatchItem> PatchUpdater::check_releases(const std::string& repo) {
    std::vector<PatchItem> items;
    std::string api_url = config_.custom_api_url;
    if (api_url.empty()) {
        api_url = "https://api.github.com/repos/" + repo + "/releases";
    }

    log_message(root_dir_, "Querying releases from: " + api_url);

    net::HttpResponse res = net::HttpClient::get(api_url, config_.timeout_seconds);
    if (!res.success) {
        log_message(root_dir_, "Failed to query GitHub releases: " + res.error + " (HTTP " + std::to_string(res.status_code) + ")");
        return items;
    }

    std::string parse_err;
    json::Value root = json::parse(res.body, &parse_err);
    if (!parse_err.empty() || !root.is_array() || root.size() == 0) {
        log_message(root_dir_, "No releases found or parse error: " + parse_err);
        return items;
    }

    // Load local cache if available
    std::string cache_path = root_dir_ + "\\Data\\bpatches.json";
    json::Value cache;
    std::ifstream cache_file(cache_path);
    if (cache_file.is_open()) {
        std::stringstream ss;
        ss << cache_file.rdbuf();
        cache = json::parse(ss.str());
    }

    // Process releases (latest first)
    const json::Value& latest_rel = root[0];
    std::string rel_tag = latest_rel["tag_name"].as_string();
    const json::Value& assets = latest_rel["assets"];

    // Check if manifest.json is one of the assets
    std::string manifest_content;
    for (size_t i = 0; i < assets.size(); i++) {
        const json::Value& asset = assets[i];
        if (asset["name"].as_string() == "manifest.json") {
            std::string manifest_url = asset["browser_download_url"].as_string();
            net::HttpResponse mf_res = net::HttpClient::get(manifest_url, config_.timeout_seconds);
            if (mf_res.success) {
                manifest_content = mf_res.body;
            }
            break;
        }
    }

    json::Value manifest;
    if (!manifest_content.empty()) {
        manifest = json::parse(manifest_content);
    }

    for (size_t i = 0; i < assets.size(); i++) {
        const json::Value& asset = assets[i];
        std::string asset_name = asset["name"].as_string();

        if (asset_name == "manifest.json") continue;
        if (config_.only_mpq && !ends_with(asset_name, ".mpq")) continue;

        PatchItem item;
        item.name = asset_name;
        item.download_url = asset["browser_download_url"].as_string();
        item.size = asset["size"].as_int64();
        item.updated_at = asset["updated_at"].as_string();
        item.target_path = resolve_patch_target(asset_name);

        if (manifest.has("patches") && manifest["patches"].is_array()) {
            for (size_t p = 0; p < manifest["patches"].size(); p++) {
                const auto& m_patch = manifest["patches"][p];
                if (m_patch["name"].as_string() == asset_name) {
                    if (m_patch.has("sha256")) item.sha256 = m_patch["sha256"].as_string();
                    if (m_patch.has("rel_path")) {
                        item.target_path = root_dir_ + "\\" + m_patch["rel_path"].as_string();
                    }
                    break;
                }
            }
        }

        int64_t local_sz = get_file_size(item.target_path);
        if (local_sz < 0) {
            // File does not exist
            item.needs_download = true;
            log_message(root_dir_, "Patch missing: " + item.name);
        } else if (local_sz != item.size) {
            // Size mismatch
            item.needs_download = true;
            log_message(root_dir_, "Patch size mismatch: " + item.name + " (local " + std::to_string(local_sz) + ", remote " + std::to_string(item.size) + ")");
        } else if (!item.sha256.empty()) {
            // Compare SHA256 if available
            std::string local_hash = crypto::sha256_file(item.target_path);
            if (local_hash != item.sha256) {
                item.needs_download = true;
                log_message(root_dir_, "Patch hash mismatch for " + item.name);
            }
        } else if (cache.has(item.name)) {
            // Check cache
            std::string cached_updated = cache[item.name]["updated_at"].as_string();
            if (!cached_updated.empty() && cached_updated != item.updated_at) {
                item.needs_download = true;
                log_message(root_dir_, "Patch updated_at differs: " + item.name);
            }
        }

        items.push_back(item);
    }

    return items;
}

bool PatchUpdater::download_patch(const PatchItem& patch) {
    std::string tmp_path = patch.target_path + ".tmp";
    PatchUI* ui = PatchUI::get();

    log_message(root_dir_, "Downloading patch " + patch.name + " to " + tmp_path);

    auto progress_cb = [ui, &patch](int64_t downloaded, int64_t total) -> bool {
        if (ui->is_closed()) return false;

        int percent = 0;
        if (total > 0) {
            percent = static_cast<int>((downloaded * 100) / total);
        }

        std::string detail = patch.name + " (" + format_bytes(downloaded) + " / " + format_bytes(total > 0 ? total : patch.size) + ")";
        ui->set_status("Downloading custom patches...", detail);
        ui->set_progress(percent);
        return true;
    };

    std::string err;
    bool ok = net::HttpClient::download_file(patch.download_url, tmp_path, 30, progress_cb, &err);
    if (!ok) {
        log_message(root_dir_, "Failed to download " + patch.name + ": " + err);
        return false;
    }

    if (!patch.sha256.empty()) {
        std::string downloaded_hash = crypto::sha256_file(tmp_path);
        if (downloaded_hash != patch.sha256) {
            log_message(root_dir_, "Downloaded patch checksum failed for " + patch.name);
            DeleteFileA(tmp_path.c_str());
            return false;
        }
    }

    // Replace original file
    if (!MoveFileExA(tmp_path.c_str(), patch.target_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        log_message(root_dir_, "Failed to move temp patch to " + patch.target_path + " error: " + std::to_string(GetLastError()));
        return false;
    }

    log_message(root_dir_, "Successfully installed " + patch.name);
    return true;
}

void PatchUpdater::update_local_cache(const std::vector<PatchItem>& patches, const std::string& release_tag) {
    std::string cache_path = root_dir_ + "\\Data\\bpatches.json";
    json::Value root(json::Type::Object);

    std::ifstream in(cache_path);
    if (in.is_open()) {
        std::stringstream ss;

    }
    // Read existing cache if present
    std::ifstream in2(cache_path);
    if (in2.is_open()) {
        std::stringstream ss;
        ss << in2.rdbuf();
        root = json::parse(ss.str());
    }

    for (const auto& item : patches) {
        json::Value obj(json::Type::Object);
        obj["size"] = json::Value(item.size);
        obj["updated_at"] = json::Value(item.updated_at);
        obj["tag"] = json::Value(release_tag);
        if (!item.sha256.empty()) {
            obj["sha256"] = json::Value(item.sha256);
        }
        root[item.name] = obj;
    }

    std::ofstream out(cache_path, std::ios::trunc);
    if (out.is_open()) {
        out << json::serialize(root, 2);
    }
}

bool PatchUpdater::run() {
    log_message(root_dir_, "bpatches check started for repo: " + config_.repo);

    PatchUI* ui = PatchUI::get();
    if (config_.show_ui) {
        ui->init(hInstance_, "BalanceWoW Patch Updater");
        ui->set_status("Checking for custom patches...", "Connecting to GitHub Releases...");
        ui->set_progress(0);
        ui->show();
    }

    std::vector<PatchItem> patches = check_releases(config_.repo);

    std::vector<PatchItem> to_download;
    for (const auto& p : patches) {
        if (p.needs_download) {
            to_download.push_back(p);
        }
    }

    if (to_download.empty()) {
        log_message(root_dir_, "All custom patches are up to date.");
        if (config_.show_ui) {
            ui->set_status("BalanceWoW Patches", "All custom patches are up to date.");
            ui->set_progress(100);
            Sleep(250);
            ui->close();
        }
        return true;
    }

    log_message(root_dir_, "Found " + std::to_string(to_download.size()) + " patches to download.");

    bool all_ok = true;
    for (size_t i = 0; i < to_download.size(); i++) {
        if (ui->is_closed()) break;

        const auto& p = to_download[i];
        if (!download_patch(p)) {
            all_ok = false;
        }
    }

    update_local_cache(patches, "");

    if (config_.show_ui) {
        if (all_ok) {
            ui->set_status("BalanceWoW Patches", "All patches updated successfully!");
            ui->set_progress(100);
            Sleep(500);
        } else {
            ui->set_status("BalanceWoW Patches", "Some patches could not be updated. Continuing...");
            Sleep(1000);
        }
        ui->close();
    }

    return all_ok;
}
