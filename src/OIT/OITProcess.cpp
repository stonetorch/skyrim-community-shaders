#include "OITProcess.h"

#include <d3dcompiler.h>
// 引入调用栈捕获工具
#include "Utils/CallstackCapture.h"

#include "OITState.h"
#include "SceneManager.h"
#include "ShaderCache.h"
#include "ShaderTools/BSShaderHooks.h"
#include "State.h"
#include "Utils/D3D.h"

#define debugger OITState::GetSingleton()->GetOITDebugger()

namespace OIT {
    namespace Hooks {
        std::unordered_map<void *, std::pair<std::unique_ptr<uint8_t[]>, size_t> > ShaderBytecodeMap;
        bool isCapturing = false;
        // 定义DrawCall调用地址数据结构
        Utils::CallstackCapture::CallSitesMap drawCallSites;

        void RegisterShaderBytecode(void *Shader, const void *Bytecode, size_t BytecodeLength) {
            auto codeCopy = std::make_unique<uint8_t[]>(BytecodeLength);
            memcpy(codeCopy.get(), Bytecode, BytecodeLength);
            logger::debug(fmt::runtime("Saving shader at index {:x} with {} bytes:\t{:x}"), (std::uintptr_t) Shader,
                          BytecodeLength, (std::uintptr_t) Bytecode);
            ShaderBytecodeMap.emplace(Shader, std::make_pair(std::move(codeCopy), BytecodeLength));
        }

        const std::pair<std::unique_ptr<uint8_t[]>, size_t> &GetShaderBytecode(void *Shader) {
            logger::debug(fmt::runtime("Loading shader at index {:x}"), (std::uintptr_t) Shader);
            return ShaderBytecodeMap.at(Shader);
        }

        // 开始捕获DrawCall
        void StartCapturing() {
            isCapturing = true;
            logger::info("开始捕获DrawIndexed调用");
        }

        // 停止捕获DrawCall
        void StopCapturing() {
            isCapturing = false;
            logger::info("停止捕获DrawIndexed调用");
        }

        // 捕获DrawIndexed调用信息
        void CaptureDrawIndexedCall(ID3D11DeviceContext *context, ID3D11Device *device, UINT IndexCount,
                                    UINT StartIndexLocation, INT BaseVertexLocation) {
            if (!OIT::Hooks::isCapturing)
                return;

            logger::debug("捕获DrawIndexed调用: IndexCount={}, StartIndexLocation={}, BaseVertexLocation={}",
                          IndexCount, StartIndexLocation, BaseVertexLocation);
            OITState::GetSingleton()->GetSceneManager()->AddSceneObject(context, device, IndexCount, StartIndexLocation,
                                                                        BaseVertexLocation);
        }

        struct ID3D11Device_CreateVertexShader {
            static HRESULT thunk(ID3D11Device *This, const void *pShaderBytecode, SIZE_T BytecodeLength,
                                 ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader) {
                HRESULT hr = func(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);

                if (SUCCEEDED(hr))
                    RegisterShaderBytecode(*ppVertexShader, pShaderBytecode, BytecodeLength);

                return hr;
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct ID3D11Device_CreatePixelShader {
            static HRESULT STDMETHODCALLTYPE thunk(ID3D11Device *This, const void *pShaderBytecode,
                                                   SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage,
                                                   ID3D11PixelShader **ppPixelShader) {
                HRESULT hr = func(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);

                if (SUCCEEDED(hr))
                    RegisterShaderBytecode(*ppPixelShader, pShaderBytecode, BytecodeLength);

                return hr;
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct BSShader_LoadShaders {
            static void thunk(RE::BSShader *shader, std::uintptr_t stream) {
                func(shader, stream);
                BSShaderHooks::hk_LoadShaders((REX::BSShader *) shader, stream);
            };
            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct IDXGISwapChain_Present {
            static HRESULT WINAPI thunk(IDXGISwapChain *This, UINT SyncInterval, UINT Flags) {
                //TODO 可能需要使用Skyrim的按键池

                // 检查是否按下了指定快捷键
                static bool f11KeyWasDown = false;
                bool f11KeyIsDown = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;

                if (f11KeyIsDown && !f11KeyWasDown) {
                    // 导出并分析DrawCall调用地址数据
                    Utils::CallstackCapture::DumpCallSites(drawCallSites, "DrawCallAddresses.log");
                    Utils::CallstackCapture::AnalyzeCallSites(drawCallSites, "DrawCallAnalysis.log");
                }
                f11KeyWasDown = f11KeyIsDown;

                // 添加清除数据的快捷键 (F12)
                static bool f12KeyWasDown = false;
                bool f12KeyIsDown = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;

                if (f12KeyIsDown && !f12KeyWasDown) {
                    Utils::CallstackCapture::ClearCallSites(drawCallSites);
                }
                f12KeyWasDown = f12KeyIsDown;

                OITState::GetSingleton()->RenderMenu();
                auto retval = func(This, SyncInterval, Flags);
                return retval;
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct BSGraphics_Renderer_Init_InitD3D {
            static void thunk() {
                logger::info("Calling original Init3D");

                func();

                logger::info("Accessing render device information");

                auto manager = RE::BSGraphics::Renderer::GetSingleton();

                auto context = reinterpret_cast<ID3D11DeviceContext *>(manager->GetRuntimeData().context);
                auto swapchain = reinterpret_cast<IDXGISwapChain *>(manager->GetRuntimeData().renderWindows->swapChain);
                auto device = reinterpret_cast<ID3D11Device *>(manager->GetRuntimeData().forwarder);

                auto &shaderCache = SIE::ShaderCache::Instance();
                if (shaderCache.IsDump()) {
                    logger::info("Hooking ID3D11Device::CreateVertexShader and ID3D11Device::CreatePixelShader");
                    stl::detour_vfunc<12, ID3D11Device_CreateVertexShader>(device);
                    stl::detour_vfunc<15, ID3D11Device_CreatePixelShader>(device);
                }

                OITState::GetSingleton()->Setup();
                InstallD3DHooks();
                OITState::GetSingleton()->GetOITDebugger()->InitImGuiContext(swapchain, device, context);
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        // DrawCall钩子结构
#define LOG_DRAWCALLS OITState::GetSingleton()->GetOITDebugger()->debugger0

        struct ID3D11DeviceContext_DrawIndexed {
            static void STDMETHODCALLTYPE thunk(ID3D11DeviceContext *This, UINT IndexCount, UINT StartIndexLocation,
                                                INT BaseVertexLocation) {
                // 捕获调用地址
                uintptr_t returnAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
                Utils::CallstackCapture::RecordCallSite(drawCallSites, returnAddr, "DrawIndexed");

                // 钩子前处理
                if (LOG_DRAWCALLS)
                    logger::debug(
                        fmt::runtime("DrawIndexed: IndexCount={}, StartIndexLocation={}, BaseVertexLocation={}"),
                        IndexCount, StartIndexLocation, BaseVertexLocation);

                // 捕获DrawIndexed调用信息
                if (isCapturing) {
                    ID3D11Device *pDevice;
                    This->GetDevice(&pDevice);
                    CaptureDrawIndexedCall(This, pDevice, IndexCount, StartIndexLocation, BaseVertexLocation);
                }

                auto context = OITState::GetSingleton()->GetContext();

                ID3D11DepthStencilState *stencilState;
                UINT stencilRef = 0;
                context->OMGetDepthStencilState(&stencilState, &stencilRef);
                D3D11_DEPTH_STENCIL_DESC desc;
                stencilState->GetDesc(&desc);
                // 调用原始函数
                if (!debugger->debugger1)
                    func(This, IndexCount, StartIndexLocation, BaseVertexLocation);
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct ID3D11DeviceContext_Draw {
            static void STDMETHODCALLTYPE thunk(ID3D11DeviceContext *This, UINT VertexCount, UINT StartVertexLocation) {
                // 捕获调用地址
                uintptr_t returnAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
                Utils::CallstackCapture::RecordCallSite(drawCallSites, returnAddr, "Draw");

                // 钩子前处理
                if (LOG_DRAWCALLS)
                    logger::debug(fmt::runtime("Draw: VertexCount={}, StartVertexLocation={}"),
                                  VertexCount, StartVertexLocation);

                // 调用原始函数
                if (!debugger->debugger2)
                    func(This, VertexCount, StartVertexLocation);

                // 钩子后处理
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct ID3D11DeviceContext_DrawIndexedInstanced {
            static void STDMETHODCALLTYPE thunk(ID3D11DeviceContext *This, UINT IndexCountPerInstance,
                                                UINT InstanceCount,
                                                UINT StartIndexLocation, INT BaseVertexLocation,
                                                UINT StartInstanceLocation) {
                // 捕获调用地址
                uintptr_t returnAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
                Utils::CallstackCapture::RecordCallSite(drawCallSites, returnAddr, "DrawIndexedInstanced");

                // 钩子前处理
                if (LOG_DRAWCALLS)
                    logger::debug(
                        fmt::runtime(
                            "DrawIndexedInstanced: IndexCountPerInstance={}, InstanceCount={}, StartIndexLocation={}, BaseVertexLocation={}, StartInstanceLocation={}"),
                        IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation,
                        StartInstanceLocation);

                // 调用原始函数
                func(This, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation,
                     StartInstanceLocation);

                // 钩子后处理
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct ID3D11DeviceContext_DrawInstanced {
            static void STDMETHODCALLTYPE thunk(ID3D11DeviceContext *This, UINT VertexCountPerInstance,
                                                UINT InstanceCount,
                                                UINT StartVertexLocation, UINT StartInstanceLocation) {
                // 捕获调用地址
                uintptr_t returnAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
                Utils::CallstackCapture::RecordCallSite(drawCallSites, returnAddr, "DrawInstanced");

                // 钩子前处理
                if (LOG_DRAWCALLS)
                    logger::debug(
                        fmt::runtime(
                            "DrawInstanced: VertexCountPerInstance={}, InstanceCount={}, StartVertexLocation={}, StartInstanceLocation={}"),
                        VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

                // 调用原始函数
                func(This, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

                // 钩子后处理
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct ID3D11DeviceContext_DrawAuto {
            static void STDMETHODCALLTYPE thunk(ID3D11DeviceContext *This) {
                // 捕获调用地址
                uintptr_t returnAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
                Utils::CallstackCapture::RecordCallSite(drawCallSites, returnAddr, "DrawAuto");

                // 钩子前处理
                if (LOG_DRAWCALLS)
                    logger::debug(fmt::runtime("DrawAuto"));

                // 调用原始函数
                func(This);

                // 钩子后处理
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct ID3D11DeviceContext_Dispatch {
            static HRESULT STDMETHODCALLTYPE thunk(ID3D11DeviceContext *This, UINT ThreadGroupCountX,
                                                   UINT ThreadGroupCountY, UINT ThreadGroupCountZ) {
                // 捕获调用地址
                uintptr_t returnAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
                Utils::CallstackCapture::RecordCallSite(drawCallSites, returnAddr, "Dispatch");

                // 钩子前处理
                if (LOG_DRAWCALLS)
                    logger::debug(
                        fmt::runtime("Dispatch: ThreadGroupCountX={}, ThreadGroupCountY={}, ThreadGroupCountZ={}"),
                        ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

                // 调用原始函数
                HRESULT hr = func(This, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

                // 钩子后处理
                return hr;
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        struct BSInputDeviceManager_PollInputDevices {
            static void thunk(RE::BSTEventSource<RE::InputEvent *> *a_dispatcher, RE::InputEvent *const*a_events) {
                bool allowBlockDevice = true;
                auto menu = OITState::GetSingleton()->GetOITDebugger();

                if (a_events) {
                    menu->ProcessInputEvents(a_events);
                }

                if (allowBlockDevice && menu->ShouldSwallowInput()) {
                    //the menu is open, eat all keypresses
                    constexpr RE::InputEvent *const dummy[] = {nullptr};
                    func(a_dispatcher, dummy);
                    return;
                }

                func(a_dispatcher, a_events);
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        void Install() {
            logger::info("Hooking BSInputDeviceManager::PollInputDevices");
            stl::write_thunk_call<BSInputDeviceManager_PollInputDevices>(
                REL::RelocationID(67315, 68617).address() + REL::Relocate(0x7B, 0x7B, 0x81));

            logger::info("Hooking BSShader::LoadShaders");
            stl::detour_thunk<BSShader_LoadShaders>(REL::RelocationID(101339, 108326));

            logger::info("Hooking BSGraphics::Renderer::InitD3D");
            stl::write_thunk_call<BSGraphics_Renderer_Init_InitD3D>(
                REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC));
        }

        void InstallD3DHooks() {
            // Hook DrawCall方法
            logger::info("Hooking ID3D11DeviceContext drawcall methods");
            auto renderer = RE::BSGraphics::Renderer::GetSingleton();
            auto context = reinterpret_cast<ID3D11DeviceContext *>(renderer->GetRuntimeData().context);
            if (context) {
                // 安装各种DrawCall钩子
                // vtable中相对于ID3D11DeviceContext的偏移是7
                stl::detour_vfunc<12, ID3D11DeviceContext_DrawIndexed>(context);
                stl::detour_vfunc<13, ID3D11DeviceContext_Draw>(context);
                stl::detour_vfunc<20, ID3D11DeviceContext_DrawIndexedInstanced>(context);
                stl::detour_vfunc<21, ID3D11DeviceContext_DrawInstanced>(context);
                // stl::detour_vfunc<16, ID3D11DeviceContext_DrawAuto>(context);
                // stl::detour_vfunc<27, ID3D11DeviceContext_Dispatch>(context);
            } else {
                logger::error("Cannot get ID3D11DeviceContext when hooking");
            }

            logger::info("Hooking IDXGISwapChain::Present");
            auto swapchain = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().renderWindows->swapChain;
            stl::detour_vfunc<8, IDXGISwapChain_Present>(swapchain);
        }
    } // namespace Hooks
} // namespace OIT

ID3D11ComputeShader *OITProcess::GetAccumulationShader() {
    static ID3D11ComputeShader *accumulationShader = nullptr;
    if (!accumulationShader) {
        auto &device = State::GetSingleton()->device;
        auto &shaderCache = SIE::ShaderCache::Instance();
        auto &context = State::GetSingleton()->context;
        accumulationShader = static_cast<ID3D11ComputeShader *>(Util::CompileShader(
            L"Data\\Shaders\\OIT\\Accumulation.hlsl", {}, "cs", "Accumulation"));
    }

    return nullptr;
}

ID3D11ComputeShader *OITProcess::GetRevealageShader() {
    static ID3D11ComputeShader *revealageShader = nullptr;
    if (!revealageShader) {
        auto &device = State::GetSingleton()->device;
        auto &shaderCache = SIE::ShaderCache::Instance();
        auto &context = State::GetSingleton()->context;
        revealageShader = static_cast<ID3D11ComputeShader *>(Util::CompileShader(
            L"Data\\Shaders\\OIT\\Revealage.hlsl", {}, "cs", "Revealage"));
    }
    return revealageShader;
}

void OITProcess::OnPrepass() {
    auto &context = State::GetSingleton()->context;
    context->CSSetShader(GetAccumulationShader(), nullptr, 0);
    Buffer *buffer;
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    ZeroMemory(&desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    // TODO set format
    buffer->CreateSRV(desc);

    ID3D11ShaderResourceView *srvs[1] = {
        buffer->srv.get()
    };
    context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
    context->Dispatch(8, 1, 1);
}

// 实现OnDraw函数，用于重现DrawIndexed调用
void OITProcess::OnDraw() {
    auto context = OITState::GetSingleton()->GetContext();
    auto device = OITState::GetSingleton()->GetDevice();

    OITState::GetSingleton()->GetSceneManager()->OnDraw(context, device);
}
