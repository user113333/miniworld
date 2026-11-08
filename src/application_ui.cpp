#include "application_ui.h"
#include "island_controls.h"
#include "map_renderer.h"

#include <SDL3/SDL.h>
#include <misc/freetype/imgui_freetype.h>

#include <string>

bool ApplicationUi::initialize()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    base_style_ = ImGui::GetStyle();

    const char* base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_Log("Failed to locate application directory: %s", SDL_GetError());
        return false;
    }
    const std::string font_path = std::string(base_path) + "res/fonts/CozetteVector.ttf";
    SDL_PathInfo font_info;
    if (!SDL_GetPathInfo(font_path.c_str(), &font_info)) {
        SDL_Log("Failed to locate font: %s", SDL_GetError());
        return false;
    }

    // Cozette's native grid is 6x13 pixels. Preserve its hard pixel edges.
    ImFontConfig font_config;
    font_config.PixelSnapH = true;
    font_config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting |
                                  ImGuiFreeTypeLoaderFlags_Monochrome;
    if (!io.Fonts->AddFontFromFileTTF(font_path.c_str(), 13.0f, &font_config)) {
        SDL_Log("Failed to load font: %s", font_path.c_str());
        return false;
    }
    return true;
}

void ApplicationUi::update_scale(SDL_Window* window)
{
    const float scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
    if (scale <= 0.0f || scale == content_scale_) {
        return;
    }
    content_scale_ = scale;

    // Rebuild from the original style to avoid cumulative rounding errors.
    // Content scale controls UI size; the backends handle framebuffer density
    // and ImGui's dynamic font atlas rasterizes at that density automatically.
    ImGuiStyle& style = ImGui::GetStyle();
    style = base_style_;
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
}

void ApplicationUi::draw(bool& running, IslandDemo& island, const MapView& view)
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit")) {
                running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Island", nullptr, &show_island_window_);
            ImGui::MenuItem("ImGui demo", nullptr, &show_demo_window_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (view.radius > 0.0f) {
        ImDrawList* overlay = ImGui::GetBackgroundDrawList();
        const ImVec2 origin(view.origin.x, view.origin.y);
        const float scale = ImGui::GetStyle().FontScaleDpi;
        const float arm = 4.0f * scale;
        const ImU32 color = IM_COL32(227, 230, 255, 255);
        overlay->AddLine(ImVec2(origin.x - arm, origin.y), ImVec2(origin.x + arm, origin.y), color);
        overlay->AddLine(ImVec2(origin.x, origin.y - arm), ImVec2(origin.x, origin.y + arm), color);
        overlay->AddText(ImVec2(origin.x + arm * 2, origin.y), color, "(0, 0)");
    }
    if (show_island_window_) {
        draw_island_controls(island, show_island_window_);
    }
    if (show_demo_window_) {
        ImGui::ShowDemoWindow(&show_demo_window_);
    }
}
