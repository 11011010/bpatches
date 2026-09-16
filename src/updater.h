#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include "config.h"

struct PatchItem {
    std::string name;             // e.g. "patch-5.rar" or "patch-W.mpq"
    std::string download_url;
    int64_t size = 0;
    std::string sha256;
    std::string target_path;      // target destination for primary mpq
    std::string updated_at;
    bool is_archive = false;      // true if .rar, .zip, .7z, .tar.gz
    std::vector<std::string> extracted_mpqs;
    bool needs_download = false;
};

class PatchUpdater {
public:
    explicit PatchUpdater(HINSTANCE hInstance);

    bool run();

private:
    HINSTANCE hInstance_ = nullptr;
    std::string root_dir_;
    AppConfig config_;

    std::string get_game_root();
    std::string resolve_patch_target(const std::string& patch_name);
    std::vector<PatchItem> check_releases(const std::string& repo);
    bool download_patch(const PatchItem& patch);
    bool extract_archive(const std::string& archive_path, const std::string& dest_dir);
    void update_local_cache(const std::vector<PatchItem>& installed_patches, const std::string& release_tag);
};
