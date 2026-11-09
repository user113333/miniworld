#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>

struct AssetTestDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("miniworld-assets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    AssetTestDirectory() { std::filesystem::create_directories(path); }
    ~AssetTestDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    void copy_resources(const std::filesystem::path& root)
    { std::filesystem::copy(root, path, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing); }
    void write(const std::string& key, const std::string& data)
    {
        std::filesystem::create_directories((path / key).parent_path());
        std::ofstream file(path / key, std::ios::binary); file << data;
    }
};
