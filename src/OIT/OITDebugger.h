#pragma once

#include <PCH.h>
#include <d3d11.h>
#include <dxgi.h>

/**
 * 用于调试，还用于渲染ImGui调试界面
 */
class OITDebugger {
public:
    OITDebugger() = default;

    ~OITDebugger() = default;

    // ImGui初始化和清理
    bool InitImGuiContext(IDXGISwapChain *pSwapChain, ID3D11Device *device, ID3D11DeviceContext *context);

    void Render();

    void CleanupImGui();

    void ProcessInputEvents(RE::InputEvent *const*input_event);

    bool ShouldSwallowInput();

    // 调试标志
    bool debugger0 = false;
    bool debugger1 = false;
    bool debugger2 = false;
    bool debugger3 = false;
    bool bGuiInitialized = false;
    ImGuiIO imgui_io;

private:
    // UI渲染
    void RenderMenu();


    // 按键配置
    static constexpr uint32_t KEY_F1 = 0x3B; // DIK_F1
    static constexpr uint32_t KEY_F2 = 0x3C; // DIK_F2
    static constexpr uint32_t KEY_F3 = 0x3D; // DIK_F3

    // 输入状态
    bool bInputEnabled = false;
    bool bShowMenu = false;

    
    std::queue<RE::InputEvent*> inputQueue;
};
