#pragma once

#include <PCH.h>
#include <d3d11.h>

#include "OITDebugger.h"

class OITState
{
public:
	static OITState* GetSingleton()
	{
		static OITState singleton;
		return &singleton;
	}

	void Setup();

	void Reset();

	void RenderMenu() const;

	OITDebugger* GetOITDebugger() const;

	ID3D11Device* GetDevice() const
	{
		return device;
	}

	ID3D11DeviceContext* GetContext() const
	{
		return context;
	}

private:
	OITState();

	~OITState();

	OITState(const OITState&) = delete;

	OITState& operator=(const OITState&) = delete;

	bool initialized = false;
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;

	OITDebugger* oitDebugger;
};
