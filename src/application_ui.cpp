#include "application_ui.h"

#include <SDL3/SDL.h>

#include <string>

bool ApplicationUi::initialize()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    base_style_ = ImGui::GetStyle();

    const char* base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_Log("Failed to locate application directory: %s", SDL_GetError());
        return false;
    }
    const std::string font_path = std::string(base_path) + "res/fonts/plex.ttf";
    SDL_PathInfo font_info;
    if (!SDL_GetPathInfo(font_path.c_str(), &font_info)) {
        SDL_Log("Failed to locate font: %s", SDL_GetError());
        return false;
    }

    // FreeType defaults to native hinting with grayscale antialiasing.
    // Keep fractional advances: PixelSnapH would break fractional DPI scaling.
    ImFontConfig font_config;
    font_config.FontLoaderFlags = 0;
    if (!io.Fonts->AddFontFromFileTTF(font_path.c_str(), 15.0f, &font_config)) {
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

void ApplicationUi::draw(bool& running)
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit")) {
                running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui demo", nullptr, &show_demo_window_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Submit after the menu reserves viewport work area and before dockable windows.
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    if (show_demo_window_) {
        ImGui::ShowDemoWindow(&show_demo_window_);
    }
}
