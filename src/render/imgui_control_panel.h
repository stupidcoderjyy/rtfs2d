//
// Created by PC on 2026/6/16.
//

#ifndef RTFS2D_IMGUI_CONTROL_PANEL_H
#define RTFS2D_IMGUI_CONTROL_PANEL_H

namespace rtfs2d {

struct VisConfig;
class ComputeContext;
class Window;

class ImGuiControlPanel {
public:
    explicit ImGuiControlPanel(VisConfig& config, ComputeContext& cc, Window& window);
    void Render() const;

private:
    VisConfig* config_;
    ComputeContext* cc_;
    Window* window_;

    void RenderFieldControls() const;
    void RenderDyeControls() const;
};

}
#endif //RTFS2D_IMGUI_CONTROL_PANEL_H
