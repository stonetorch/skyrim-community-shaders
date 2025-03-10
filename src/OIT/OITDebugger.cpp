#include "OITDebugger.h"
#include "Utils/UI.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>

#include "OITState.h"

void OITDebugger::RenderMenu()
{
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
		}

		{
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
		}

		{
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

// 初始化 ImGui 上下文
bool OITDebugger::InitImGuiContext(IDXGISwapChain *pSwapChain, ID3D11Device *device, ID3D11DeviceContext *context)
{
	// 创建 ImGui 上下文
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& imgui_io = ImGui::GetIO();

	// 配置 ImGui IO
	imgui_io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
	imgui_io.BackendFlags = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasVtxOffset;

	// 配置字体
	ImFontConfig font_config;
	font_config.GlyphExtraSpacing.x = -0.5;
	imgui_io.Fonts->AddFontFromFileTTF("Data\\Interface\\CommunityShaders\\Fonts\\Jost-Regular.ttf", 36, &font_config);

	// 获取交换链描述
	DXGI_SWAP_CHAIN_DESC desc;
	pSwapChain->GetDesc(&desc);

	// 初始化 Win32 和 DX11 实现
	if (!ImGui_ImplWin32_Init(desc.OutputWindow)) {
		return false;
	}

	if (!ImGui_ImplDX11_Init(device, context)) {
		return false;
	}

	return true;
}

// 在渲染循环中调用
void RenderImGui()
{
	// 开始新的 ImGui 帧
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 在这里添加您的 ImGui 界面代码
	ImGui::Begin("Hello, ImGui!");
	ImGui::Text("This is some useful text.");
	ImGui::End();

	// 渲染 ImGui
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// 清理 ImGui
void CleanupImGui()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
