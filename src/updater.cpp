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
#include <shellapi.h>

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    return to_lower(str.substr(str.size() - suffix.size())) == to_lower(suffix);
}

std::string strip_extension(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(0, pos);
    }
    return filename;
}

bool is_archive_file(const std::string& name) {
    return ends_with(name, ".rar") || ends_with(name, ".zip") ||
           ends_with(name, ".7z")  || ends_with(name, ".tar.gz") || ends_with(name, ".tgz");
}

bool is_patch_asset(const std::string& name) {
    return ends_with(name, ".mpq") || is_archive_file(name);
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

void delete_directory_recursive(const std::string& dir) {
    char from[MAX_PATH + 2] = { 0 };
    strncpy_s(from, dir.c_str(), MAX_PATH);
    from[strlen(from) + 1] = '\0'; // double null-terminated

    SHFILEOPSTRUCTA op = { 0 };
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    SHFileOperationA(&op);
}

void find_mpq_files(const std::string& dir, std::vector<std::string>& out_files) {
    std::string search_path = dir + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        std::string full_path = dir + "\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            find_mpq_files(full_path, out_files);
        } else {
            if (ends_with(fd.cFileName, ".mpq")) {
                out_files.push_back(full_path);
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
}

std::string get_filename_from_path(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
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

bool PatchUpdater::extract_archive(const std::string& archive_path, const std::string& dest_dir) {
    CreateDirectoryA(dest_dir.c_str(), nullptr);

    char sys_dir[MAX_PATH] = { 0 };
    GetSystemDirectoryA(sys_dir, MAX_PATH);
    std::string tar_exe = std::string(sys_dir) + "\\tar.exe";
    if (GetFileAttributesA(tar_exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        tar_exe = "tar.exe";
    }

    std::string cmd = "\"" + tar_exe + "\" -xf \"" + archive_path + "\" -C \"" + dest_dir + "\"";
    log_message(root_dir_, "Executing extract command: " + cmd);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };

    char cmd_buf[2048];
    strncpy_s(cmd_buf, cmd.c_str(), sizeof(cmd_buf) - 1);

    if (CreateProcessA(nullptr, cmd_buf, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 120000); // Wait up to 2 minutes for extraction
        DWORD exit_code = 1;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exit_code == 0;
    }

    log_message(root_dir_, "CreateProcess failed for tar.exe: " + std::to_string(GetLastError()));
    return false;
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
        if (!is_patch_asset(asset_name)) continue;

        PatchItem item;
        item.name = asset_name;
        item.download_url = asset["browser_download_url"].as_string();
        item.size = asset["size"].as_int64();
        item.updated_at = asset["updated_at"].as_string();
        item.is_archive = is_archive_file(asset_name);

        if (item.is_archive) {
            // e.g. "patch-5.rar" -> primary mpq "patch-5.mpq"
            std::string base_mpq = strip_extension(asset_name) + ".mpq";
            item.target_path = resolve_patch_target(base_mpq);

            bool target_exists = (GetFileAttributesA(item.target_path.c_str()) != INVALID_FILE_ATTRIBUTES);

            if (!target_exists) {
                item.needs_download = true;
                log_message(root_dir_, "Archive patch missing local file: " + item.target_path);
            } else if (cache.has(item.name)) {
                std::string cached_updated = cache[item.name]["updated_at"].as_string();
                int64_t cached_sz = cache[item.name]["size"].as_int64();
                if (cached_updated != item.updated_at || cached_sz != item.size) {
                    item.needs_download = true;
                    log_message(root_dir_, "Archive patch updated remotely: " + item.name);
                }
            } else {
                // Not in cache, download to be safe
                item.needs_download = true;
                log_message(root_dir_, "Archive patch not in cache: " + item.name);
            }
        } else {
            // Direct .mpq file
            item.target_path = resolve_patch_target(asset_name);
            int64_t local_sz = get_file_size(item.target_path);

            if (local_sz < 0) {
                item.needs_download = true;
                log_message(root_dir_, "Patch missing: " + item.name);
            } else if (local_sz != item.size) {
                item.needs_download = true;
                log_message(root_dir_, "Patch size mismatch: " + item.name);
            } else if (cache.has(item.name)) {
                std::string cached_updated = cache[item.name]["updated_at"].as_string();
                if (!cached_updated.empty() && cached_updated != item.updated_at) {
                    item.needs_download = true;
                    log_message(root_dir_, "Patch updated_at differs: " + item.name);
                }
            }
        }

        items.push_back(item);
    }

    return items;
}

bool PatchUpdater::download_patch(const PatchItem& patch) {
    PatchUI* ui = PatchUI::get();

    if (patch.is_archive) {
        // Download archive to a temporary file
        std::string tmp_archive = root_dir_ + "\\Data\\_tmp_" + patch.name;
        log_message(root_dir_, "Downloading archive " + patch.name + " to " + tmp_archive);

        auto progress_cb = [ui, &patch](int64_t downloaded, int64_t total) -> bool {
            if (ui->is_closed()) return false;
            int percent = (total > 0) ? static_cast<int>((downloaded * 100) / total) : 0;
            std::string detail = patch.name + " (" + format_bytes(downloaded) + " / " + format_bytes(total > 0 ? total : patch.size) + ")";
            ui->set_status("Downloading patch archive...", detail);
            ui->set_progress(percent);
            return true;
        };

        std::string err;
        bool ok = net::HttpClient::download_file(patch.download_url, tmp_archive, 60, progress_cb, &err);
        if (!ok) {
            log_message(root_dir_, "Failed to download " + patch.name + ": " + err);
            return false;
        }

        ui->set_status("Extracting patch...", "Extracting " + patch.name + "...");
        ui->set_progress(95);

        // Extract to temporary folder
        std::string extract_dir = root_dir_ + "\\Data\\_extract_tmp";
        delete_directory_recursive(extract_dir);

        bool ext_ok = extract_archive(tmp_archive, extract_dir);
        DeleteFileA(tmp_archive.c_str());

        if (!ext_ok) {
            log_message(root_dir_, "Failed to extract archive " + patch.name);
            delete_directory_recursive(extract_dir);
            return false;
        }

        // Find all extracted .mpq files
        std::vector<std::string> mpqs;
        find_mpq_files(extract_dir, mpqs);

        if (mpqs.empty()) {
            log_message(root_dir_, "Warning: No .mpq files found inside archive " + patch.name);
            delete_directory_recursive(extract_dir);
            return false;
        }

        for (const auto& mpq_file : mpqs) {
            std::string filename = get_filename_from_path(mpq_file);
            std::string final_dest = resolve_patch_target(filename);
            log_message(root_dir_, "Installing extracted MPQ " + filename + " -> " + final_dest);
            MoveFileExA(mpq_file.c_str(), final_dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
        }

        delete_directory_recursive(extract_dir);
        log_message(root_dir_, "Successfully unpacked and installed " + patch.name);
        return true;
    } else {
        // Direct .mpq download
        std::string tmp_path = patch.target_path + ".tmp";
        log_message(root_dir_, "Downloading patch " + patch.name + " to " + tmp_path);

        auto progress_cb = [ui, &patch](int64_t downloaded, int64_t total) -> bool {
            if (ui->is_closed()) return false;
            int percent = (total > 0) ? static_cast<int>((downloaded * 100) / total) : 0;
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

        if (!MoveFileExA(tmp_path.c_str(), patch.target_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            log_message(root_dir_, "Failed to move temp patch to " + patch.target_path + " error: " + std::to_string(GetLastError()));
            return false;
        }

        log_message(root_dir_, "Successfully installed " + patch.name);
        return true;
    }
}

void PatchUpdater::update_local_cache(const std::vector<PatchItem>& patches, const std::string& release_tag) {
    std::string cache_path = root_dir_ + "\\Data\\bpatches.json";
    json::Value root(json::Type::Object);

    std::ifstream in(cache_path);
    if (in.is_open()) {
        std::stringstream ss;
        ss << in.rdbuf();
        root = json::parse(ss.str());
    }

    for (const auto& item : patches) {
        json::Value obj(json::Type::Object);
        obj["size"] = json::Value(item.size);
        obj["updated_at"] = json::Value(item.updated_at);
        obj["tag"] = json::Value(release_tag);
        obj["is_archive"] = json::Value(item.is_archive);
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
