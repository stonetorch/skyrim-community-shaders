#include "Hooks.h"
#include "ShaderCache.h"
#include "State.h"
#include <ShaderTools/BSShader.h>
#include <ShaderTools/BSShaderHooks.h>

std::unordered_map<void*, std::pair<std::unique_ptr<uint8_t[]>, size_t>> ShaderBytecodeMap;

void RegisterShaderBytecode(void* Shader, const void* Bytecode, size_t BytecodeLength)
{
	auto codeCopy = std::make_unique<uint8_t[]>(BytecodeLength);
	memcpy(codeCopy.get(), Bytecode, BytecodeLength);
	logger::debug(fmt::runtime("Saving shader at index {:x} with {} bytes:\t{:x}"), (std::uintptr_t)Shader, BytecodeLength, (std::uintptr_t)Bytecode);
	ShaderBytecodeMap.emplace(Shader, std::make_pair(std::move(codeCopy), BytecodeLength));
}

const std::pair<std::unique_ptr<uint8_t[]>, size_t>& GetShaderBytecode(void* Shader)
{
	logger::debug(fmt::runtime("Loading shader at index {:x}"), (std::uintptr_t)Shader);
	return ShaderBytecodeMap.at(Shader);
}

struct ID3D11Device_CreateVertexShader
{
	static HRESULT thunk(ID3D11Device* This, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)
	{
		HRESULT hr = func(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);

		if (SUCCEEDED(hr))
			RegisterShaderBytecode(*ppVertexShader, pShaderBytecode, BytecodeLength);

		return hr;
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

struct ID3D11Device_CreatePixelShader
{
	static HRESULT STDMETHODCALLTYPE thunk(ID3D11Device* This, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)
	{
		HRESULT hr = func(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);

		if (SUCCEEDED(hr))
			RegisterShaderBytecode(*ppPixelShader, pShaderBytecode, BytecodeLength);

		return hr;
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

struct BSShader_LoadShaders
{
	static void thunk(RE::BSShader* shader, std::uintptr_t stream)
	{
		func(shader, stream);
		BSShaderHooks::hk_LoadShaders((REX::BSShader*)shader, stream);
	};
	static inline REL::Relocation<decltype(thunk)> func;
};

struct BSGraphics_Renderer_Init_InitD3D
{
	static void thunk()
	{
		logger::info("Calling original Init3D");

		func();

		logger::info("Accessing render device information");

		auto manager = RE::BSGraphics::Renderer::GetSingleton();

		auto context = reinterpret_cast<ID3D11DeviceContext*>(manager->GetRuntimeData().context);
		auto swapchain = reinterpret_cast<IDXGISwapChain*>(manager->GetRuntimeData().renderWindows->swapChain);
		auto device = reinterpret_cast<ID3D11Device*>(manager->GetRuntimeData().forwarder);

		auto& shaderCache = SIE::ShaderCache::Instance();
		if (shaderCache.IsDump()) {
			logger::info("Hooking ID3D11Device::CreateVertexShader and ID3D11Device::CreatePixelShader");
			stl::detour_vfunc<12, ID3D11Device_CreateVertexShader>(device);
			stl::detour_vfunc<15, ID3D11Device_CreatePixelShader>(device);
		}
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

void OIT::Hooks::Install()
{
	logger::info("Hooking BSShader::LoadShaders");
	stl::detour_thunk<BSShader_LoadShaders>(REL::RelocationID(101339, 108326));
	
	logger::info("Hooking BSGraphics::Renderer::InitD3D");
	stl::write_thunk_call<BSGraphics_Renderer_Init_InitD3D>(REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC));
}
