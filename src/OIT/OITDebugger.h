#ifndef OITDEBUGGER_H
#define OITDEBUGGER_H
#include <d3d11.h>
#include <dxgi.h>


class OITDebugger {
public:
    bool debugger0 = false;
    bool debugger1 = false;
    bool debugger2 = false;
    bool debugger3 = false;

    bool InitImGuiContext(IDXGISwapChain *pSwapChain, ID3D11Device *device, ID3D11DeviceContext *context);

    void RenderMenu();
};

#endif  //OITDEBUGGER_H
