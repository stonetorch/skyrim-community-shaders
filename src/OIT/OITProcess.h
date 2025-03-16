#pragma once

#include <d3d11.h>
#include <memory>
#include <vector>

#include "SceneManager.h"

namespace OIT
{
	namespace Hooks
	{
		void Install();
		void InstallD3DHooks();

		// 捕获的DrawIndexed调用信息
		// extern std::vector<DrawIndexedCallInfo> capturedDrawCalls;

		// 捕获DrawIndexed调用信息
		void CaptureDrawIndexedCall(ID3D11DeviceContext* context, ID3D11Device* device, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
	}  // namespace Hooks
}  // namespace OIT

class OITProcess
{
public:
	// 处理捕获的DrawCall
	static void OnDraw();

protected:
	static ID3D11ComputeShader* GetAccumulationShader();
	static ID3D11ComputeShader* GetRevealageShader();
	static void OnPrepass();
};
