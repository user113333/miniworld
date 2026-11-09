#pragma once
#include "content/catalog.h"
#include "render/texture_cache.h"

class AssetBrowser {
public:
    void open() { open_ = true; focus_ = true; }
    bool is_open() const { return open_; }
    void draw(content::Catalog& catalog, TextureCache& textures);
private:
    void details(content::Catalog& catalog, TextureCache& textures, bool hidden);
    bool open_ = false, focus_ = false;
    std::string folder_, selected_;
    char search_[192]{};
    int type_ = 0, sort_ = 0;
    float thumbnail_size_ = 80;
};
