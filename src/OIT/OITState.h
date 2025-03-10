//
// Created by 14982 on 25-3-10.
//

#ifndef OITSTATE_H
#define OITSTATE_H

#include <d3d11.h>
#include <spdlog/spdlog.h>

#include "OITDebugger.h"

class OITState {
public:
    static OITState *GetSingleton() {
        static OITState singleton;
        return &singleton;
    }

    void Setup();

    void Reset();

    void RenderMenu() const;

    OITDebugger *GetOITDebugger() const;

    ID3D11Device *GetDevice() const {
        return device;
    }

    ID3D11DeviceContext *GetContext() const {
        return context;
    }

private:
    OITState() = default;

    ~OITState();

    OITState(const OITState &) = delete;

    OITState &operator=(const OITState &) = delete;

    bool initialized = false;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;

    OITDebugger *oitDebugger;
};

#endif  //OITSTATE_H
