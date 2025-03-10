#pragma once

#include <d3d11.h>

namespace OIT
{
	namespace Hooks
	{
		void Install();
		void InstallD3DHooks();
	}  // namespace Hooks
}  // namespace OIT

class OITProcess
{
public:
protected:
	static ID3D11ComputeShader* GetAccumulationShader();
	static ID3D11ComputeShader* GetRevealageShader();
	static void OnPrepass();
};
