#include "thought_bubbles.h"
#include "render/settler_layout.h"
#include <imgui.h>
#include <algorithm>

void draw_thought_bubbles(const std::vector<ActorView>& actors, const ActorSceneView& view)
{
    if (view.radius <= 0) return;
    auto* draw = ImGui::GetBackgroundDrawList();
    const auto& io = ImGui::GetIO();
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float padding = 5 * scale;
    for (const auto& actor : actors) {
        if (!actor.settler || !actor.settler->thought_ticks || actor.settler->thought.empty()) continue;
        const auto head = settler_layout(actor.position, view).head;
        const auto& text = actor.settler->thought;
        const auto size = ImGui::CalcTextSize(text.c_str());
        const float width = size.x + padding * 2, height = size.y + padding * 2;
        const float x = std::clamp(head.x - width / 2, 2 * scale, std::max(2 * scale, io.DisplaySize.x - width - 2 * scale));
        const float bottom = head.y - 8 * scale;
        if (bottom < 0 || bottom - height > io.DisplaySize.y || head.x < 0 || head.x > io.DisplaySize.x) continue;
        const ImVec2 minimum{x, bottom - height}, maximum{x + width, bottom};
        const auto ink = IM_COL32(44, 45, 52, 255), paper = IM_COL32(250, 247, 231, 248);
        draw->AddRectFilled({minimum.x + scale, minimum.y + 2 * scale},
                            {maximum.x + scale, maximum.y + 2 * scale}, IM_COL32(0, 0, 0, 65), 5 * scale);
        draw->AddRectFilled(minimum, maximum, paper, 5 * scale);
        draw->AddRect(minimum, maximum, ink, 5 * scale);
        const float tail = std::clamp(head.x, x + 6 * scale, x + width - 6 * scale);
        draw->AddTriangleFilled({tail - 4 * scale, bottom - scale}, {tail + 4 * scale, bottom - scale},
                                {head.x, bottom + 5 * scale}, paper);
        draw->AddLine({tail - 4 * scale, bottom}, {head.x, bottom + 5 * scale}, ink);
        draw->AddLine({tail + 4 * scale, bottom}, {head.x, bottom + 5 * scale}, ink);
        draw->AddText({x + padding, bottom - height + padding}, ink, text.c_str());
    }
}
