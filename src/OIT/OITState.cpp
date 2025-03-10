#include "OITState.h"

#include "OITDebugger.h"
#include "State.h"
#include "RE/R/Renderer.h"

OITState::~OITState() {
    if (this->oitDebugger)delete oitDebugger;
}

void OITState::Setup() {
    if (initialized)
        return;


    this->oitDebugger = new OITDebugger();
    device = reinterpret_cast<ID3D11Device *>(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().forwarder);
    context = reinterpret_cast<ID3D11DeviceContext *>(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().
        context);

    initialized = true;
}

void OITState::Reset() {
    initialized = false;
    device = nullptr;
    context = nullptr;
}

void OITState::RenderMenu() const {
    this->oitDebugger->RenderMenu();
}

OITDebugger *OITState::GetOITDebugger() const {
    return oitDebugger;
}
