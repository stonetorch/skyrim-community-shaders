#include "OITDebugger.h"
#include "Utils/UI.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>
#include <winrt/base.h>

#include "Menu.h"
#include "OITState.h"

// 移除全局变量，避免在OITState初始化前使用
// static ID3D11Device* g_pd3dDevice = OITState::GetSingleton()->GetDevice();
// static ID3D11DeviceContext* g_pd3dDeviceContext = OITState::GetSingleton()->GetContext();

// ImGui初始化流程说明：
// 1. 必须在以下顺序进行初始化：
//    a) 创建ID3D11Device和ID3D11DeviceContext（由游戏引擎完成）
//    b) 创建IDXGISwapChain（由游戏引擎完成）
//    c) 调用OITState::Setup()进行初始设置
//    d) 调用InitImGuiContext初始化ImGui
// 2. 每帧渲染顺序：
//    a) 调用ImGui_ImplDX11_NewFrame()和ImGui_ImplWin32_NewFrame()
//    b) 调用ImGui::NewFrame()
//    c) 绘制ImGui界面内容
//    d) 调用ImGui::Render()
//    e) 调用ImGui_ImplDX11_RenderDrawData()

void OITDebugger::RenderMenu() {
    if (ImGui::Begin("OIT Debugger Menu")) {
        // 使用百分比滑动条控制透明度
        float alpha = 1.0f;
        Util::PercentageSlider("Menu Opacity", &alpha, 0.0f, 100.0f, "%.0f%%");
        // ImGui::SetWindowOpacity(alpha);

        // 使用DisableGuard来禁用某些选项
        {
            Util::DisableGuard guard(!debugger0);
            if (ImGui::Checkbox("Debugger 0", &debugger0)) {
                // 当debugger0启用时，禁用其他选项
                if (debugger0) {
                    debugger1 = false;
                    debugger2 = false;
                    debugger3 = false;
                }
            }
        }

        // 为每个选项添加悬停提示
        {
            if (ImGui::Checkbox("Debugger 1", &debugger1)) {
                if (debugger1) {
                    debugger0 = false;
                    debugger2 = false;
                    debugger3 = false;
                }
            }
            if (auto _tt = Util::HoverTooltipWrapper()) {
                ImGui::Text("Enable debugger 1 for detailed logging");
            }
        } {
            if (ImGui::Checkbox("Debugger 2", &debugger2)) {
                if (debugger2) {
                    debugger0 = false;
                    debugger1 = false;
                    debugger3 = false;
                }
            }
            if (auto _tt = Util::HoverTooltipWrapper()) {
                ImGui::Text("Enable debugger 2 for performance metrics");
            }
        } {
            if (ImGui::Checkbox("Debugger 3", &debugger3)) {
                if (debugger3) {
                    debugger0 = false;
                    debugger1 = false;
                    debugger2 = false;
                }
            }
            if (auto _tt = Util::HoverTooltipWrapper()) {
                ImGui::Text("Enable debugger 3 for memory tracking");
            }
        }

        // 添加一个状态显示区域
        ImGui::Separator();
        ImGui::Text("Debug Status:");
        ImGui::Indent(10);
        ImGui::Text("Debugger 0: %s", debugger0 ? "Active" : "Inactive");
        ImGui::Text("Debugger 1: %s", debugger1 ? "Active" : "Inactive");
        ImGui::Text("Debugger 2: %s", debugger2 ? "Active" : "Inactive");
        ImGui::Text("Debugger 3: %s", debugger3 ? "Active" : "Inactive");
        ImGui::Unindent(10);
    }
    ImGui::End();
}


// 初始化 ImGui 上下文 - 必须在获取到Direct3D设备后调用
bool OITDebugger::InitImGuiContext(IDXGISwapChain *pSwapChain, ID3D11Device *device, ID3D11DeviceContext *context) {
    if (bGuiInitialized) {
        logger::warn("OITDebugger: Duplicate ImGui initialization detected");
        return false;
    }
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imgui_io = ImGui::GetIO();

    imgui_io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;

    ImFontConfig font_config;
    font_config.GlyphExtraSpacing.x = -0.5;

    imgui_io.Fonts->AddFontFromFileTTF("Data\\Interface\\CommunityShaders\\Fonts\\Jost-Regular.ttf", 36, &font_config);

    DXGI_SWAP_CHAIN_DESC desc;
    pSwapChain->GetDesc(&desc);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(desc.OutputWindow);
    ImGui_ImplDX11_Init(device, context);

    // auto &io = ImGui::GetIO(); {
    //     winrt::com_ptr<IDXGIDevice> dxgiDevice;
    //     if (!FAILED(device->QueryInterface(dxgiDevice.put()))) {
    //         winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
    //         if (!FAILED(dxgiDevice->GetAdapter(dxgiAdapter.put()))) {
    //             dxgiAdapter->QueryInterface(dxgiAdapter3.put());
    //         }
    //     }
    // }

    bGuiInitialized = true;
    return true;
}

// 在渲染循环中调用 - 必须在每帧的开始调用
void OITDebugger::Render() {
    // 开始新的 ImGui 帧
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (bShowMenu) {
        // 在这里添加您的 ImGui 界面代码
        ImGui::Begin("OIT调试器");
        ImGui::Text("欢迎使用OIT调试器");
        RenderMenu();
        ImGui::End();
    }
    // 渲染 ImGui
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// 清理 ImGui - 必须在程序退出前调用
void OITDebugger::CleanupImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    logger::info("ImGui上下文已清理");
}

void OITDebugger::ProcessInputEvents(RE::InputEvent *const*input_event) {
    // inputQueue.push(*input_event);
    // handle in place
    for (auto it = *input_event; it; it = it->next) {
        if (it->device == RE::INPUT_DEVICE::kKeyboard) {
            auto event = static_cast<RE::CharEvent *>(it);
            auto keycode = event->keyCode;
            if (keycode == 117) {
                bShowMenu = !bShowMenu;
                debugger0 = !debugger0;
            } else if (keycode == KEY_F2) {
                debugger1 = !debugger1;
            } else if (keycode == KEY_F3) {
                debugger2 = !debugger2;
            }
        }
        if (it->eventType == RE::INPUT_EVENT_TYPE::kChar) {
            auto event = static_cast<RE::CharEvent *>(it);
            imgui_io.AddInputCharacter(event->keyCode);
            continue;
        }
        if (it->device == RE::INPUT_DEVICE::kMouse) {
            auto event = static_cast<RE::ButtonEvent *>(it);
            logger::trace("Detect mouse scan code {} value {} pressed: {}", event->GetIDCode(), event->value,
                          event->Value() > 0.0f);
            if (event->GetIDCode() > 7) {
                // middle scroll
                imgui_io.AddMouseWheelEvent(0, event->value * (event->GetIDCode() == 8 ? 1 : -1));
            } else {
                // regular mouse buttons
                imgui_io.AddMouseButtonEvent(event->GetIDCode() > 5 ? 5 : event->GetIDCode(), event->Value() > 0.0f);
            }
        }
    }
}

bool OITDebugger::ShouldSwallowInput() {
    return bShowMenu;
}
