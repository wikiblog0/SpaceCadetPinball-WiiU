#pragma once

class menu {
    public:
    static void RenderMenuWindow();
    static bool ShowWindow;
};

void BeginGroupPanel(const char* name, const ImVec2& size = ImVec2(0.0f, 0.0f));
void EndGroupPanel();