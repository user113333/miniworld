#include "texture_cache.h"
#include "aseprite_texture.h"

TextureCache::TextureCache() = default;

CachedImage::~CachedImage() { if (texture) SDL_DestroyTexture(texture); }
TextureCache::~TextureCache() { shutdown(); }
void TextureCache::initialize(SDL_Renderer* renderer, content::Catalog& catalog)
{
    shutdown(); renderer_ = renderer; catalog_ = &catalog;
}
void TextureCache::shutdown() { animations_.clear(); images_.clear(); catalog_ = nullptr; renderer_ = nullptr; error_.clear(); }
std::size_t TextureCache::loaded_count() const
{
    std::size_t count = 0;
    for (const auto& image : images_) count += image.second != nullptr;
    for (const auto& animation : animations_) count += animation.second != nullptr;
    return count;
}
const CachedImage* TextureCache::load(const std::string& key)
{
    if (!catalog_ || !renderer_) { error_ = key + ": texture cache is detached"; return nullptr; }
    const auto* asset = catalog_->find(key);
    if (!asset || asset->type != content::AssetType::image) {
        error_ = key + ": missing PNG asset or unsupported loader"; return nullptr;
    }
    const auto found = images_.find(key);
    if (found != images_.end()) {
        error_ = found->second ? "" : asset->diagnostic;
        return found->second.get();
    }
    auto image = std::make_unique<CachedImage>();
    SDL_Surface* surface = SDL_LoadPNG(catalog_->path(key)->string().c_str());
    bool loaded = false;
    if (!surface) error_ = SDL_GetError();
    else {
        image->mask.size = {surface->w, surface->h};
        image->mask.alpha.resize(static_cast<std::size_t>(surface->w) * surface->h);
        bool readable = true;
        for (int y = 0; y < surface->h && readable; ++y) for (int x = 0; x < surface->w; ++x) {
            Uint8 r, g, b, a;
            if (!SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a)) { readable = false; break; }
            image->mask.alpha[static_cast<std::size_t>(y) * surface->w + x] = a;
        }
        if (readable) image->texture = SDL_CreateTextureFromSurface(renderer_, surface);
        if (!image->texture || !SDL_SetTextureScaleMode(image->texture, SDL_SCALEMODE_NEAREST) ||
            !SDL_SetTextureBlendMode(image->texture, SDL_BLENDMODE_BLEND)) error_ = SDL_GetError();
        else { error_.clear(); loaded = true; }
        SDL_DestroySurface(surface);
    }
    if (!loaded) {
        if (error_.empty()) error_ = "Cannot decode PNG or create its texture/alpha mask";
        catalog_->report(key, {}, error_);
        error_ = catalog_->find(key)->diagnostic;
        images_.emplace(key, nullptr); return nullptr;
    }
    catalog_->report(key, image->mask.size);
    const auto* result = image.get(); images_.emplace(key, std::move(image)); return result;
}
