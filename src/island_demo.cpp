#include "island_demo.h"
#include "island_generator.h"
#include "map_renderer.h"

#include <imgui.h>

#include <algorithm>

IslandDemo::IslandDemo()
    : seed_(random_()), map_(generate_island(width_, height_, seed_))
{
}

void IslandDemo::draw_controls()
{
    ImGui::SetNextWindowPos(ImVec2(12, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Island", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Current seed: %u", map_.seed);
        ImGui::InputScalar("Seed", ImGuiDataType_U32, &seed_);
        ImGui::InputInt("Width", &width_);
        ImGui::InputInt("Height", &height_);
        width_ = std::clamp(width_, 12, 96);
        height_ = std::clamp(height_, 12, 72);
        if (ImGui::Button("Generate")) {
            seed_ = random_();
            map_ = generate_island(width_, height_, seed_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Regenerate")) {
            map_ = generate_island(width_, height_, seed_);
        }
        ImGui::TextUnformatted("Edit seed, then Regenerate.");
    }
    ImGui::End();
}

void IslandDemo::draw(SDL_Renderer* renderer) const
{
    const ImGuiIO& io = ImGui::GetIO();
    const float sidebar = 280.0f * ImGui::GetStyle().FontScaleDpi;
    const float top = ImGui::GetFrameHeight() + 16.0f;
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    draw_map(renderer, map_, {sidebar, top, io.DisplaySize.x - sidebar - 16.0f,
                             io.DisplaySize.y - top - 16.0f});
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}
