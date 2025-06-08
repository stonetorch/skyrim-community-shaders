#pragma once
#include <intsafe.h>
#include <vector>

#include "REX/W32/D3D11.h"
#include "TruePBR/BSLightingShaderMaterialPBR.h"

// 定义顶点缓冲区转储文件的路径
#define VERTEX_BUFFER_DUMP_PATH "Data\\SKSE\\Plugins\\SkyrimCommunityShaders\\VertexBufferDump\\"

/**
 * @brief 用于存储DrawIndexed调用的相关信息
 * 生命周期：在捕获DrawIndexed调用时创建，然后在OnDraw调用时绘制，最后在**OnDraw**调用中释放资源
 */
struct SceneObject
{
	SceneObject(ID3D11DeviceContext* context, ID3D11Device* device, UINT indexCount, UINT startIndexLocation,
		INT baseVertexLocation);

	SceneObject(const SceneObject&) = default;
	SceneObject& operator=(const SceneObject&) = default;

	SceneObject(SceneObject&& other) noexcept
		:
		IndexCount(other.IndexCount),
		StartIndexLocation(other.StartIndexLocation),
		BaseVertexLocation(other.BaseVertexLocation),
		indexBuffer(other.indexBuffer),
		numVertexBuffers(other.numVertexBuffers),
		indexFormat(other.indexFormat),
		vertexShader(other.vertexShader),
		pixelShader(other.pixelShader),
		inputLayout(other.inputLayout),
		numConstantBuffers(other.numConstantBuffers),
		numShaderResources(other.numShaderResources),
		numSamplers(other.numSamplers),
		topology(other.topology),
		numViewports(other.numViewports),
		depthStencilState(other.depthStencilState),
		stencilRef(other.stencilRef),
		rasterizerState(other.rasterizerState),
		blendState(other.blendState),
		sampleMask(other.sampleMask),
		vertexBuffer(other.vertexBuffer)
	{
		// 复制数组成员
		for (UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i) {
			vertexBuffers[i] = other.vertexBuffers[i];
			strides[i] = other.strides[i];
			offsets[i] = other.offsets[i];
			other.vertexBuffers[i] = nullptr;
		}

		for (UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i) {
			constantBuffers[i] = other.constantBuffers[i];
			other.constantBuffers[i] = nullptr;
		}

		for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i) {
			shaderResources[i] = other.shaderResources[i];
			other.shaderResources[i] = nullptr;
		}

		for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i) {
			samplers[i] = other.samplers[i];
			other.samplers[i] = nullptr;
		}

		for (UINT i = 0; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; ++i) {
			viewports[i] = other.viewports[i];
		}

		for (UINT i = 0; i < 4; ++i) {
			blendFactor[i] = other.blendFactor[i];
		}

		// 将源对象的指针置空，避免双重释放
		other.indexBuffer = nullptr;
		other.vertexShader = nullptr;
		other.pixelShader = nullptr;
		other.inputLayout = nullptr;
		other.depthStencilState = nullptr;
		other.rasterizerState = nullptr;
		other.blendState = nullptr;
		other.vertexBuffer = nullptr;
	}

	~SceneObject() = default;

	void Draw(ID3D11DeviceContext* context, ID3D11Device* device);

	/**
	 * 将顶点缓冲区数据转储到独立的文件中
	 */
	void DumpVertexBuffer();

	// 基本参数
	UINT IndexCount;
	UINT StartIndexLocation;
	INT BaseVertexLocation;

	// 顶点和索引数据
	ID3D11Buffer* indexBuffer;
	ID3D11Buffer* vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];  // 支持多个顶点缓冲区
	UINT numVertexBuffers;                                                   // 实际使用的顶点缓冲区数量
	UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];                 // 每个顶点缓冲区的步长
	UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];                 // 每个顶点缓冲区的偏移
	DXGI_FORMAT indexFormat;

	// 渲染状态
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* inputLayout;
	ID3D11Buffer* constantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	UINT numConstantBuffers;

	// 着色器资源视图和采样器
	ID3D11ShaderResourceView* shaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	ID3D11SamplerState* samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	UINT numShaderResources;
	UINT numSamplers;

	// 其他状态
	D3D11_PRIMITIVE_TOPOLOGY topology;
	D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	UINT numViewports;

	// 深度模板状态
	ID3D11DepthStencilState* depthStencilState;
	UINT stencilRef;

	// 光栅化状态
	ID3D11RasterizerState* rasterizerState;

	// 混合状态
	ID3D11BlendState* blendState;
	FLOAT blendFactor[4];
	UINT sampleMask;
	ID3D11Buffer* vertexBuffer;
};

class SceneManager
{
public:
	SceneManager() = default;

	/**
     * 在管线的合适时机调用
     * 这会绘制所有被捕获的场景对象，然后释放它们的资源
     */
	void OnDraw(ID3D11DeviceContext* context, ID3D11Device* device);

	void AddSceneObject(const SceneObject& sceneObject);

	/**
     * @brief 在管线的合适时机调用，将当前准备渲染的对象添加到场景管理器中，这是一个更加快速的方法。
     */
	void AddSceneObject(ID3D11DeviceContext* context, ID3D11Device* device, UINT IndexCount,
		UINT StartIndexLocation, INT BaseVertexLocation);

private:
	std::vector<SceneObject> capturedSceneObjects;
};
