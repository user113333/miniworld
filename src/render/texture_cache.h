#pragma once
#include "content/catalog.h"
#include "actor_scene.h"
#include <SDL3/SDL.h>
#include <memory>
#include <unordered_map>

struct CachedImage {
    CachedImage() = default;
    CachedImage(const CachedImage&) = delete;
    CachedImage& operator=(const CachedImage&) = delete;
    SDL_Texture* texture = nullptr;
    ActorSpriteMask mask;
    ~CachedImage();
};
// Application owns this cache. Consumers borrow images until shutdown; no eviction.
class TextureCache {
public:
    ~TextureCache();
    TextureCache() = default;
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    void initialize(SDL_Renderer* renderer, content::Catalog& catalog);
    void shutdown(); // Detach GUI/actor consumers before this; renderer must still live.
    const CachedImage* load(const std::string& key);
    std::size_t loaded_count() const;
    const std::string& error() const { return error_; }
private:
    SDL_Renderer* renderer_ = nullptr;
    content::Catalog* catalog_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<CachedImage>> images_;
    std::string error_;
};
