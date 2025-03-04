// Deferred.cpp - 延迟渲染实现
// 该文件实现了基于D3D11的延迟渲染系统，包括G-Buffer的创建、渲染目标管理和着色器处理

#include "Deferred.h"

#include "ShaderCache.h"
#include "State.h"
#include "TruePBR.h"
#include "Util.h"

#include "Features/DynamicCubemaps.h"
#include "Features/ScreenSpaceGI.h"
#include "Features/Skylighting.h"
#include "Features/SubsurfaceScattering.h"
#include "Features/TerrainBlending.h"

// 深度状态结构体 - 用于管理不同深度测试/写入配置
struct DepthStates
{
	ID3D11DepthStencilState* a[6][40];
};

// 混合状态结构体 - 用于管理不同的alpha混合配置
struct BlendStates
{
	ID3D11BlendState* a[7][2][13][2];

	static BlendStates* GetSingleton()
	{
		static auto blendStates = reinterpret_cast<BlendStates*>(REL::RelocationID(524749, 411364).address());
		return blendStates;
	}
};

// 设置单个渲染目标的辅助函数
// 为G-Buffer中的每个组件(albedo, normal等)创建必要的D3D11资源
void SetupRenderTarget(RE::RENDER_TARGET target, D3D11_TEXTURE2D_DESC texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc, D3D11_RENDER_TARGET_VIEW_DESC rtvDesc, D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc, DXGI_FORMAT format)
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	// 设置所有视图使用相同的格式
	texDesc.Format = format;
	srvDesc.Format = format;
	rtvDesc.Format = format;
	uavDesc.Format = format;

	// 创建渲染目标所需的所有D3D11资源:
	// 1. 2D纹理 - 存储实际像素数据
	// 2. SRV - 允许在着色器中读取
	// 3. RTV - 允许渲染到该目标
	// 4. UAV - 允许计算着色器读写访问
	auto& data = renderer->GetRuntimeData().renderTargets[target];
	DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, nullptr, &data.texture));
	DX::ThrowIfFailed(device->CreateShaderResourceView(data.texture, &srvDesc, &data.SRV));
	DX::ThrowIfFailed(device->CreateRenderTargetView(data.texture, &rtvDesc, &data.RTV));
	DX::ThrowIfFailed(device->CreateUnorderedAccessView(data.texture, &uavDesc, &data.UAV));
}

void Deferred::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	// 创建G-Buffer渲染目标
	{
		// 获取主渲染目标的描述符作为模板
		auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		// 初始化各种描述符
		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		main.texture->GetDesc(&texDesc);
		main.SRV->GetDesc(&srvDesc);
		main.RTV->GetDesc(&rtvDesc);
		main.UAV->GetDesc(&uavDesc);

		// 输出纹理描述信息用于调试
		logger::info("Texture Description:");
		logger::info("Width: {}", texDesc.Width);
		logger::info("Height: {}", texDesc.Height);
		logger::info("MipLevels: {}", texDesc.MipLevels);
		logger::info("ArraySize: {}", texDesc.ArraySize);
		logger::info("SampleDesc.Count: {}", texDesc.SampleDesc.Count);
		logger::info("SampleDesc.Quality: {}", texDesc.SampleDesc.Quality);
		logger::info("BindFlags: {}", texDesc.BindFlags);
		logger::info("CPUAccessFlags: {}", texDesc.CPUAccessFlags);
		logger::info("MiscFlags: {}", texDesc.MiscFlags);

		// 创建G-Buffer中的各个组件:
		// Albedo - 基础颜色信息
		SetupRenderTarget(ALBEDO, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Specular - 高光信息
		SetupRenderTarget(SPECULAR, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R11G11B10_FLOAT);
		// Reflectance - 反射信息
		SetupRenderTarget(REFLECTANCE, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Normal + Roughness - 法线和粗糙度
		SetupRenderTarget(NORMALROUGHNESS, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Masks - 各种遮罩信息
		SetupRenderTarget(MASKS, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Additional Masks - 额外遮罩信息
		SetupRenderTarget(MASKS2, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
	}

	// 创建线性采样器状态 - 用于纹理采样
	{
		auto& device = State::GetSingleton()->device;

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // 三线性过滤
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &linearSampler));
	}

	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DEFAULT;
		sbDesc.CPUAccessFlags = 0;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = 0;

		std::uint32_t numElements = 1;

		sbDesc.StructureByteStride = sizeof(PerGeometry);
		sbDesc.ByteWidth = sizeof(PerGeometry) * numElements;
		perShadow = new Buffer(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		perShadow->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		perShadow->CreateUAV(uavDesc);

		copyShadowCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\ShadowTest\\CopyShadowData.hlsl", {}, "cs_5_0"));
	}

	{
		D3D11_TEXTURE2D_DESC texDesc;
		auto mainTex = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];
		mainTex.texture->GetDesc(&texDesc);

		texDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1 }
		};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
			.Texture2D = { .MipSlice = 0 }
		};

		prevDiffuseAmbientTexture = new Texture2D(texDesc);
		prevDiffuseAmbientTexture->CreateSRV(srvDesc);
		prevDiffuseAmbientTexture->CreateUAV(uavDesc);
	}
}

void Deferred::CopyShadowData()
{
	ZoneScoped;
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "CopyShadowData");

	auto& context = State::GetSingleton()->context;

	ID3D11UnorderedAccessView* uavs[1]{ perShadow->uav.get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	ID3D11Buffer* buffers[3];
	context->PSGetConstantBuffers(0, 3, buffers);
	context->PSGetConstantBuffers(12, 1, buffers + 1);

	context->CSSetConstantBuffers(0, 3, buffers);

	context->CSSetShader(copyShadowCS, nullptr, 0);

	// 运行该CS后，perShadow中乱七八糟的的数据被统一拷贝到perGeometry，再保存到copiedData中，通过u0寄存器返回
	// 而u0寄存器绑定的是perShadow->uav.get()
	context->Dispatch(1, 1, 1);

	uavs[0] = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	std::fill(buffers, buffers + ARRAYSIZE(buffers), nullptr);
	context->CSSetConstantBuffers(0, 3, buffers);

	context->CSSetShader(nullptr, nullptr, 0);

	{
		context->PSGetShaderResources(4, 1, &shadowView);

		ID3D11ShaderResourceView* srvs[2]{
			shadowView,
			perShadow->srv.get(),
		};

		context->PSSetShaderResources(25, ARRAYSIZE(srvs), srvs);
	}
}

void Deferred::PrepassPasses()
{
	ZoneScoped;
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Prepass");

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	context->OMSetRenderTargets(0, nullptr, nullptr);  // Unbind all bound render targets

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);  // Run OMSetRenderTargets again

	TruePBR::GetSingleton()->PrePass();
	for (auto* feature : Feature::GetFeatureList()) {
		if (feature->loaded) {
			feature->Prepass();
		}
	}
}

void Deferred::StartDeferred()
{
	// 检查是否在游戏世界中
	if (!inWorld)
		return;

	// 检查着色器缓存是否可用
	auto& shaderCache = SIE::ShaderCache::Instance();
	if (!shaderCache.IsEnabled())
		return;

	// 更新共享数据
	State::GetSingleton()->UpdateSharedData();

	// 获取渲染器状态
	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(renderTargets, shadowState)
	GET_INSTANCE_MEMBER(setRenderTargetMode, shadowState)
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	// 保存原始(前向渲染)的渲染目标配置
	for (uint i = 0; i < 4; i++) {
		forwardRenderTargets[i] = renderTargets[i];
	}

	// 设置G-Buffer渲染目标
	RE::RENDER_TARGET targets[8]{
		RE::RENDER_TARGET::kMAIN,           // 主渲染目标
		RE::RENDER_TARGET::kMOTION_VECTOR,  // 运动矢量
		NORMALROUGHNESS,                    // 法线和粗糙度
		ALBEDO,                             // 基础颜色
		SPECULAR,                           // 高光
		REFLECTANCE,                        // 反射
		MASKS,                              // 遮罩
		MASKS2                              // 额外遮罩
	};

	// 配置新的渲染目标
	for (uint i = 2; i < 8; i++) {
		renderTargets[i] = targets[i];
		// 清除上一帧的数据
		setRenderTargetMode[i] = RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR;
	}

	// 标记需要更新渲染目标
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

	// 标记开始延迟渲染阶段
	deferredPass = true;

	{
		// 获取设备上下文
		auto& context = State::GetSingleton()->context;

		// 获取每帧常量缓冲区
		static REL::Relocation<ID3D11Buffer**> perFrame{ REL::RelocationID(524768, 411384) };
		ID3D11Buffer* buffers[1] = { *perFrame.get() };

		ID3D11Buffer* vrBuffer = nullptr;

		// 如果是VR模式，获取VR常量缓冲区
		if (REL::Module::IsVR()) {
			static REL::Relocation<ID3D11Buffer**> VRValues{ REL::Offset(0x3180688) };
			vrBuffer = *VRValues.get();
		}
		// 设置计算着色器的常量缓冲区
		if (vrBuffer) {
			context->CSSetConstantBuffers(12, 1, buffers);
			context->CSSetConstantBuffers(13, 1, &vrBuffer);
		} else {
			context->CSSetConstantBuffers(12, 1, buffers);
		}
	}

	// 执行预处理阶段
	PrepassPasses();

	// 覆盖混合状态
	OverrideBlendStates();
}

void Deferred::DeferredPasses()
{
	// 性能分析区域标记
	ZoneScoped;
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Deferred");

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& context = State::GetSingleton()->context;

	// 设置常量缓冲区
	{
		static REL::Relocation<ID3D11Buffer**> perFrame{ REL::RelocationID(524768, 411384) };
		ID3D11Buffer* buffers[1] = { *perFrame.get() };
		ID3D11Buffer* vrBuffer = nullptr;

		// VR模式特殊处理
		if (REL::Module::IsVR()) {
			static REL::Relocation<ID3D11Buffer**> VRValues{ REL::Offset(0x3180688) };
			vrBuffer = *VRValues.get();
		}
		if (vrBuffer) {
			context->CSSetConstantBuffers(12, 1, buffers);
			context->CSSetConstantBuffers(13, 1, &vrBuffer);
		} else {
			context->CSSetConstantBuffers(12, 1, buffers);
		}
	}

	// 获取所有渲染目标的引用
	auto specular = renderer->GetRuntimeData().renderTargets[SPECULAR];
	auto albedo = renderer->GetRuntimeData().renderTargets[ALBEDO];
	auto normalRoughness = renderer->GetRuntimeData().renderTargets[NORMALROUGHNESS];
	auto masks = renderer->GetRuntimeData().renderTargets[MASKS];
	auto masks2 = renderer->GetRuntimeData().renderTargets[MASKS2];

	auto main = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[0]];
	auto normals = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[2]];
	auto depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
	auto reflectance = renderer->GetRuntimeData().renderTargets[REFLECTANCE];

	auto motionVectors = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMOTION_VECTOR];

	// 检查是否在室内
	bool interior = true;
	if (auto sky = RE::Sky::GetSingleton())
		interior = sky->mode.get() != RE::Sky::Mode::kFull;

	// 获取各个渲染特性的实例
	auto skylighting = Skylighting::GetSingleton();
	auto ssgi = ScreenSpaceGI::GetSingleton();

	// 计算dispatch大小
	auto dispatchCount = Util::GetScreenDispatchCount();

	// 处理屏幕空间全局光照
	if (ssgi->loaded) {
		ssgi->DrawSSGI(prevDiffuseAmbientTexture);

		// 环境光合成
		{
			TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Ambient Composite");

			// 设置着色器资源
			ID3D11ShaderResourceView* srvs[6]{
				albedo.SRV,
				normalRoughness.SRV,
				skylighting->loaded || REL::Module::IsVR() ? depth.depthSRV : nullptr,
				skylighting->loaded ? skylighting->texProbeArray->srv.get() : nullptr,
				ssgi->settings.Enabled ? ssgi->texGI[ssgi->outputGIIdx]->srv.get() : nullptr,
				masks2.SRV,
			};

			context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

			ID3D11UnorderedAccessView* uavs[2]{ main.UAV, prevDiffuseAmbientTexture->uav.get() };
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			auto shader = interior ? GetComputeAmbientCompositeInterior() : GetComputeAmbientComposite();
			context->CSSetShader(shader, nullptr, 0);

			context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
		}

		// Clear
		{
			ID3D11ShaderResourceView* views[6]{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
			context->CSSetShaderResources(0, ARRAYSIZE(views), views);

			ID3D11UnorderedAccessView* uavs[2]{ nullptr, nullptr };
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			context->CSSetShader(nullptr, nullptr, 0);
		}
	}

	auto sss = SubsurfaceScattering::GetSingleton();
	if (sss->loaded)
		sss->DrawSSS();

	auto dynamicCubemaps = DynamicCubemaps::GetSingleton();
	if (dynamicCubemaps->loaded)
		dynamicCubemaps->UpdateCubemap();

	auto terrainBlending = TerrainBlending::GetSingleton();

	// Deferred Composite
	{
		TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Deferred Composite");

		bool doSSGISpecular = ssgi->loaded && ssgi->settings.Enabled && ssgi->settings.EnableGI && ssgi->settings.EnableSpecularGI;

		ID3D11ShaderResourceView* srvs[11]{
			specular.SRV,
			albedo.SRV,
			normalRoughness.SRV,
			masks.SRV,
			masks2.SRV,
			dynamicCubemaps->loaded || REL::Module::IsVR() ? (terrainBlending->loaded ? terrainBlending->blendedDepthTexture16->srv.get() : depth.depthSRV) : nullptr,
			dynamicCubemaps->loaded ? reflectance.SRV : nullptr,
			dynamicCubemaps->loaded ? dynamicCubemaps->envTexture->srv.get() : nullptr,
			dynamicCubemaps->loaded ? dynamicCubemaps->envReflectionsTexture->srv.get() : nullptr,
			dynamicCubemaps->loaded && skylighting->loaded ? skylighting->texProbeArray->srv.get() : nullptr,
			doSSGISpecular ? ssgi->texGISpecular[ssgi->outputGIIdx]->srv.get() : nullptr,
		};

		if (dynamicCubemaps->loaded)
			context->CSSetSamplers(0, 1, &linearSampler);

		context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

		ID3D11UnorderedAccessView* uavs[3]{ main.UAV, normals.UAV, motionVectors.UAV };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		auto shader = interior ? GetComputeMainCompositeInterior() : GetComputeMainComposite();
		context->CSSetShader(shader, nullptr, 0);

		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	// Clear
	{
		ID3D11ShaderResourceView* views[10]{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[3]{ nullptr, nullptr, nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11Buffer* buffers[1] = { nullptr };
		context->CSSetConstantBuffers(12, 1, buffers);

		context->CSSetShader(nullptr, nullptr, 0);
	}

	if (dynamicCubemaps->loaded)
		dynamicCubemaps->PostDeferred();
}

void Deferred::EndDeferred()
{
	if (!inWorld)
		return;

	auto& shaderCache = SIE::ShaderCache::Instance();
	if (!shaderCache.IsEnabled())
		return;

	// 恢复原始渲染目标配置
	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(renderTargets, shadowState)
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	// 恢复前向渲染的渲染目标
	for (uint i = 0; i < 4; i++) {
		renderTargets[i] = forwardRenderTargets[i];
	}

	// 清除额外的渲染目标
	for (uint i = 4; i < 8; i++) {
		renderTargets[i] = RE::RENDER_TARGET::kNONE;
	}

	// 解绑所有渲染目标
	auto& context = State::GetSingleton()->context;
	context->OMSetRenderTargets(0, nullptr, nullptr);

	// 执行延迟渲染合成
	DeferredPasses();

	// 标记需要更新渲染目标
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

	// 结束延迟渲染阶段
	deferredPass = false;

	// 重置混合状态
	ResetBlendStates();
}

// 处理混合状态的覆盖
void Deferred::OverrideBlendStates()
{
	auto blendStates = BlendStates::GetSingleton();

	// 使用std::once_flag确保初始化代码只执行一次
	static std::once_flag setup;
	std::call_once(setup, [&]() {
		auto& device = State::GetSingleton()->device;

		// 遍历所有可能的混合状态组合
		for (int a = 0; a < 7; a++) {
			for (int b = 0; b < 2; b++) {
				for (int c = 0; c < 13; c++) {
					for (int d = 0; d < 2; d++) {
						// 保存原始混合状态
						forwardBlendStates[a][b][c][d] = blendStates->a[a][b][c][d];

						if (auto blendState = forwardBlendStates[a][b][c][d]) {
							D3D11_BLEND_DESC blendDesc;
							forwardBlendStates[a][b][c][d]->GetDesc(&blendDesc);

							// 启用独立混合以便于每个渲染目标使用不同的混合设置
							blendDesc.IndependentBlendEnable = true;

							// 为每个渲染目标配置混合状态(跳过主渲染目标)
							for (int i = 1; i < 8; i++) {
								blendDesc.RenderTarget[i].BlendEnable = blendDesc.RenderTarget[0].BlendEnable;
								// 设置标准的alpha混合
								blendDesc.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
								blendDesc.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
								blendDesc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
								blendDesc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
								blendDesc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
								blendDesc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
								blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
							}

							// 创建新的混合状态
							DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[a][b][c][d]));
						} else {
							deferredBlendStates[a][b][c][d] = nullptr;
						}
					}
				}
			}
		}
	});

	// 应用修改后的混合状态
	for (int a = 0; a < 7; a++) {
		for (int b = 0; b < 2; b++) {
			for (int c = 0; c < 13; c++) {
				for (int d = 0; d < 2; d++) {
					blendStates->a[a][b][c][d] = deferredBlendStates[a][b][c][d];
				}
			}
		}
	}

	// 标记混合状态需要更新
	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
}

// 恢复原始混合状态
void Deferred::ResetBlendStates()
{
	auto blendStates = BlendStates::GetSingleton();

	// 恢复所有原始混合状态
	for (int a = 0; a < 7; a++) {
		for (int b = 0; b < 2; b++) {
			for (int c = 0; c < 13; c++) {
				for (int d = 0; d < 2; d++) {
					blendStates->a[a][b][c][d] = forwardBlendStates[a][b][c][d];
				}
			}
		}
	}

	// 标记混合状态需要更新
	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)
	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
}

// 清理着色器缓存
void Deferred::ClearShaderCache()
{
	// 释放并清空所有计算着色器
	if (ambientCompositeCS) {
		ambientCompositeCS->Release();
		ambientCompositeCS = nullptr;
	}
	if (ambientCompositeInteriorCS) {
		ambientCompositeInteriorCS->Release();
		ambientCompositeInteriorCS = nullptr;
	}
	if (mainCompositeCS) {
		mainCompositeCS->Release();
		mainCompositeCS = nullptr;
	}
	if (mainCompositeInteriorCS) {
		mainCompositeInteriorCS->Release();
		mainCompositeInteriorCS = nullptr;
	}
}

// 获取环境光合成计算着色器
ID3D11ComputeShader* Deferred::GetComputeAmbientComposite()
{
	if (!ambientCompositeCS) {
		logger::debug("Compiling AmbientCompositeCS");

		// 设置着色器编译选项
		std::vector<std::pair<const char*, const char*>> defines;

		// 根据功能启用情况添加相应的宏定义
		if (Skylighting::GetSingleton()->loaded)
			defines.push_back({ "SKYLIGHTING", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		// 编译着色器
		ambientCompositeCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\AmbientCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return ambientCompositeCS;
}

// 获取室内环境光合成计算着色器
ID3D11ComputeShader* Deferred::GetComputeAmbientCompositeInterior()
{
	if (!ambientCompositeInteriorCS) {
		logger::debug("Compiling AmbientCompositeCS INTERIOR");

		// 设置着色器编译选项
		std::vector<std::pair<const char*, const char*>> defines;
		defines.push_back({ "INTERIOR", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		// 编译着色器
		ambientCompositeInteriorCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\AmbientCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return ambientCompositeInteriorCS;
}

// 获取主要延迟合成计算着色器
ID3D11ComputeShader* Deferred::GetComputeMainComposite()
{
	if (!mainCompositeCS) {
		logger::debug("Compiling DeferredCompositeCS");

		// 设置着色器编译选项
		std::vector<std::pair<const char*, const char*>> defines;

		// 根据功能启用情况添加相应的宏定义
		if (DynamicCubemaps::GetSingleton()->loaded)
			defines.push_back({ "DYNAMIC_CUBEMAPS", nullptr });

		if (Skylighting::GetSingleton()->loaded)
			defines.push_back({ "SKYLIGHTING", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		// 编译着色器
		mainCompositeCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DeferredCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return mainCompositeCS;
}

// 获取室内主要延迟合成计算着色器
ID3D11ComputeShader* Deferred::GetComputeMainCompositeInterior()
{
	if (!mainCompositeInteriorCS) {
		logger::debug("Compiling DeferredCompositeCS INTERIOR");

		// 设置着色器编译选项
		// 根据启用的特性添加相应的宏定义
		std::vector<std::pair<const char*, const char*>> defines;
		defines.push_back({ "INTERIOR", nullptr });

		if (DynamicCubemaps::GetSingleton()->loaded)
			defines.push_back({ "DYNAMIC_CUBEMAPS", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		// 编译着色器
		mainCompositeInteriorCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DeferredCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return mainCompositeInteriorCS;
}