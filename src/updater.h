#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include "config.h"

struct PatchItem {
    std::string name;
    std::string download_url;
    int64_t size = 0;
    std::string sha256;
    std::string target_path;
    std::string updated_at;
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
    void update_local_cache(const std::vector<PatchItem>& installed_patches, const std::string& release_tag);
};
