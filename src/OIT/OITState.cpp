#include "OITState.h"

#include "OITDebugger.h"
#include "RE/R/Renderer.h"
#include "State.h"

OITState::OITState()
{
}

OITState::~OITState()
{
	Reset();
	delete oitDebugger;
}

void OITState::Setup()
{
	if (initialized) {
		return;
	}

	this->oitDebugger = new OITDebugger();
	// 获取D3D11设备和上下文
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	device = reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder);
	context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
	auto swapchain = reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows->swapChain);

	if (!device || !context || !swapchain) {
		logger::error("OITState::Setup: 无法获取D3D11设备或上下文");
		return;
	}

	// 初始化调试器ImGui上下文
	if (!oitDebugger->InitImGuiContext(swapchain, device, context)) {
		logger::error("OITState::Setup: 无法初始化ImGui上下文");
		return;
	}

	initialized = true;
	logger::info("OITState初始化成功");
}

void OITState::Reset()
{
	if (!initialized) {
		return;
	}

	// 清理ImGui资源
	oitDebugger->CleanupImGui();

	device = nullptr;
	context = nullptr;
	initialized = false;
	logger::info("OITState已重置");
}

void OITState::RenderMenu() const
{
	if (!initialized) {
		return;
	}

	// 渲染ImGui界面
	oitDebugger->Render();
}

OITDebugger* OITState::GetOITDebugger() const
{
	return oitDebugger;
}
