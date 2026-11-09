#pragma once
#include "texture_cache.h"
#include <map>

struct AnimationClip {
    std::vector<std::size_t> frames;
    std::uint64_t duration_ms = 0;
};
struct CachedAnimation {
    std::vector<std::unique_ptr<CachedImage>> frames;
    std::vector<unsigned> durations_ms;
    std::map<std::string, AnimationClip> clips;
    std::size_t frame_at(const AnimationClip& clip, std::uint64_t elapsed_ms) const;
};
