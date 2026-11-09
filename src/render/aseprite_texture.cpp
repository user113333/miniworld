#include "aseprite_texture.h"
#define CUTE_ASEPRITE_IMPLEMENTATION
#include <cute_aseprite.h>
#include <algorithm>

std::size_t CachedAnimation::frame_at(const AnimationClip& clip, std::uint64_t elapsed_ms) const
{
    elapsed_ms %= clip.duration_ms;
    for (auto frame : clip.frames) {
        if (elapsed_ms < durations_ms[frame]) return frame;
        elapsed_ms -= durations_ms[frame];
    }
    return clip.frames.back();
}

const CachedAnimation* TextureCache::load_animation(const std::string& key)
{
    if (!catalog_ || !renderer_) { error_ = key + ": texture cache is detached"; return nullptr; }
    const auto found = animations_.find(key);
    if (found != animations_.end()) {
        error_ = found->second ? "" : catalog_->find(key)->diagnostic;
        return found->second.get();
    }
    const auto* asset = catalog_->find(key);
    if (!asset || asset->type != content::AssetType::source) { error_ = key + ": missing Aseprite asset"; return nullptr; }
    std::unique_ptr<ase_t, decltype(&cute_aseprite_free)> source(
        cute_aseprite_load_from_file(catalog_->path(key)->string().c_str(), nullptr), cute_aseprite_free);
    auto animation = std::make_unique<CachedAnimation>();
    if (!source || source->w <= 0 || source->h <= 0 || source->frame_count <= 0) error_ = "Cannot decode Aseprite";
    else {
        for (int i = 0; i < source->frame_count; ++i) {
            const auto& frame = source->frames[i];
            auto image = std::make_unique<CachedImage>();
            image->mask.size = {source->w, source->h};
            image->mask.alpha.reserve(source->w * source->h);
            for (int pixel = 0; pixel < source->w * source->h; ++pixel) image->mask.alpha.push_back(frame.pixels[pixel].a);
            SDL_Surface* surface = SDL_CreateSurfaceFrom(source->w, source->h, SDL_PIXELFORMAT_RGBA32,
                                                        frame.pixels, source->w * sizeof(ase_color_t));
            if (surface) { image->texture = SDL_CreateTextureFromSurface(renderer_, surface); SDL_DestroySurface(surface); }
            if (!image->texture || !SDL_SetTextureScaleMode(image->texture, SDL_SCALEMODE_NEAREST) ||
                !SDL_SetTextureBlendMode(image->texture, SDL_BLENDMODE_BLEND)) { error_ = SDL_GetError(); break; }
            animation->frames.push_back(std::move(image));
            animation->durations_ms.push_back(std::max(1, frame.duration_milliseconds));
        }
        if (animation->frames.size() == static_cast<std::size_t>(source->frame_count)) {
            for (int i = 0; i < source->tag_count; ++i) {
                const auto& tag = source->tags[i];
                if (tag.from_frame < 0 || tag.to_frame < tag.from_frame || tag.to_frame >= source->frame_count) {
                    error_ = "Invalid Aseprite animation tag"; break;
                }
                AnimationClip clip;
                for (int frame = tag.from_frame; frame <= tag.to_frame; ++frame) clip.frames.push_back(frame);
                if (tag.loop_animation_direction == ASE_ANIMATION_DIRECTION_BACKWORDS) std::reverse(clip.frames.begin(), clip.frames.end());
                if (tag.loop_animation_direction == ASE_ANIMATION_DIRECTION_PINGPONG)
                    for (int frame = tag.to_frame - 1; frame > tag.from_frame; --frame) clip.frames.push_back(frame);
                for (auto frame : clip.frames) clip.duration_ms += animation->durations_ms[frame];
                animation->clips.emplace(tag.name, std::move(clip));
            }
            if (animation->clips.size() == static_cast<std::size_t>(source->tag_count)) {
                catalog_->report(key, {source->w, source->h});
                error_.clear();
                const auto* result = animation.get();
                animations_.emplace(key, std::move(animation));
                return result;
            }
        }
    }
    catalog_->report(key, {}, error_.empty() ? "Cannot upload Aseprite frames" : error_);
    error_ = catalog_->find(key)->diagnostic;
    animations_.emplace(key, nullptr);
    return nullptr;
}
