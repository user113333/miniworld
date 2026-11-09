#include "application_ui.h"
#include "island_controls.h"
#include "simulation_controls.h"
#include "thought_bubbles.h"
#include "simulation/simulation.h"
#include "render/map_renderer.h"
#include "render/camera.h"

#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <misc/freetype/imgui_freetype.h>

#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

ApplicationUi::~ApplicationUi()
{
    shutdown();
}

bool ApplicationUi::initialize(SDL_Window* window, SDL_Renderer* renderer,
                               content::Catalog& content, TextureCache& textures)
{
    content_ = &content; textures_ = &textures;
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    base_style_ = ImGui::GetStyle();

    if (!load_font()) {
        return false;
    }
    platform_ready_ = ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    renderer_ready_ = platform_ready_ && ImGui_ImplSDLRenderer3_Init(renderer);
    if (!renderer_ready_) {
        SDL_Log("ImGui backend initialization failed");
        return false;
    }
    return true;
}

void ApplicationUi::shutdown()
{
    if (renderer_ready_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        renderer_ready_ = false;
    }
    if (platform_ready_) {
        ImGui_ImplSDL3_Shutdown();
        platform_ready_ = false;
    }
    if (context_) {
        ImGui::DestroyContext(context_);
        context_ = nullptr;
    }
    font_data_.clear();
    content_ = nullptr; textures_ = nullptr;
}

bool ApplicationUi::load_font()
{
    constexpr const char* key = "fonts/CozetteVector.ttf";
    auto bytes = content_->font_data(key);
    if (!bytes) { SDL_Log("Cannot load required font: %s", content_->error().c_str()); return false; }
    font_data_ = std::move(*bytes);
    // Validate before ImGui's loader, whose failure path can assert in Debug.
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    const bool valid = FT_Init_FreeType(&library) == 0 &&
        FT_New_Memory_Face(library, font_data_.data(), static_cast<FT_Long>(font_data_.size()), 0, &face) == 0;
    if (face) FT_Done_Face(face);
    if (library) FT_Done_FreeType(library);
    if (!valid) {
        content_->report(key, {}, "Invalid font data");
        SDL_Log("Cannot load required font %s: invalid font data", key); return false;
    }

    // Cozette's native grid is 6x13 pixels. Preserve its hard pixel edges.
    ImFontConfig font_config;
    font_config.PixelSnapH = true;
    font_config.FontDataOwnedByAtlas = false; // Retained until after context destruction.
    font_config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_MonoHinting |
                                  ImGuiFreeTypeLoaderFlags_Monochrome;
    if (!ImGui::GetIO().Fonts->AddFontFromMemoryTTF(font_data_.data(), static_cast<int>(font_data_.size()), 13.0f, &font_config)) {
        content_->report(key, {}, "ImGui font loading failed");
        SDL_Log("Cannot load required font %s", key);
        return false;
    }
    return true;
}

void ApplicationUi::process_event(const SDL_Event& event)
{
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void ApplicationUi::begin_frame(SDL_Window* window)
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    update_scale(window);
    ImGui::NewFrame();
}

GuiFrameState ApplicationUi::frame_state() const
{
    const ImGuiIO& io = ImGui::GetIO();
    return {{io.DisplaySize.x, io.DisplaySize.y},
            {io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y},
            ImGui::GetStyle().FontScaleDpi, io.WantCaptureMouse};
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

void ApplicationUi::draw(bool& running, Simulation& simulation)
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
            ImGui::MenuItem("Simulation", nullptr, &show_simulation_window_);
            if (ImGui::MenuItem("Assets", nullptr, asset_browser_.is_open())) asset_browser_.open();
            const bool explorer_open = entity_explorer_.is_open();
            if (ImGui::MenuItem("Entity Explorer", nullptr, explorer_open)) entity_explorer_.open();
            ImGui::Separator();
            ImGui::MenuItem("ImGui demo", nullptr, &show_demo_window_);
            ImGui::EndMenu();
        }
        if (ImGui::Button("Entities")) entity_explorer_.open();
        if (ImGui::Button("Assets")) asset_browser_.open();
        ImGui::EndMainMenuBar();
    }

    if (show_island_window_) {
        draw_island_controls(simulation, show_island_window_);
    }
    if (world_generation_ != simulation.world_generation()) {
        cancel_tool();
        world_generation_ = simulation.world_generation();
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        tool_ = tool_ == EntityTool::camera ? EntityTool::none : EntityTool::camera;
    }
    if (show_simulation_window_) {
        draw_simulation_controls(simulation, tool_, show_simulation_window_);
    }
    entity_explorer_.draw(simulation);
    asset_browser_.draw(*content_, *textures_);
    if (show_demo_window_) {
        ImGui::ShowDemoWindow(&show_demo_window_);
    }
}

void ApplicationUi::select_actor(ActorHandle actor)
{
    entity_explorer_.select(actor);
}

void ApplicationUi::draw_overlays(const Simulation& simulation, const MapView& view)
{
    if (tool_ == EntityTool::select && !ImGui::GetIO().WantCaptureMouse) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    draw_origin_hint(view);
    const auto frame = frame_state();
    draw_thought_bubbles(simulation.world().actors(), {view.origin, view.radius, frame.framebuffer_scale});
}

void ApplicationUi::update_camera(Camera& camera, const MapView& centered)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (tool_ != EntityTool::camera || io.WantCaptureMouse || io.AppFocusLost) {
        camera_dragging_ = false;
        return;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) camera_dragging_ = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) camera_dragging_ = false;
    if (camera_dragging_) {
        camera.pan({io.MouseDelta.x, io.MouseDelta.y});
    }
    if (io.MouseWheel != 0.0f) {
        camera.zoom(io.MouseWheel, {io.MousePos.x, io.MousePos.y}, centered);
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
}

void ApplicationUi::finish_frame()
{
    ImGui::Render();
}

void ApplicationUi::render(SDL_Renderer* renderer) const
{
    // The backend applies DisplayFramebufferScale itself.
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void ApplicationUi::draw_origin_hint(const MapView& view) const
{
    if (view.radius <= 0.0f) {
        return;
    }
    ImDrawList* overlay = ImGui::GetBackgroundDrawList();
    const ImVec2 origin(view.origin.x, view.origin.y);
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float arm = 4.0f * scale;
    const ImU32 color = IM_COL32(227, 230, 255, 255);
    overlay->AddLine(ImVec2(origin.x - arm, origin.y), ImVec2(origin.x + arm, origin.y), color);
    overlay->AddLine(ImVec2(origin.x, origin.y - arm), ImVec2(origin.x, origin.y + arm), color);
    overlay->AddText(ImVec2(origin.x + arm * 2, origin.y), color, "(0, 0)");
}
