#pragma once

namespace Hooks
{
	struct BSShader_BeginTechnique
	{
		static bool thunk(RE::BSShader* shader, uint32_t vertexDescriptor, uint32_t pixelDescriptor, bool skipPixelShader);
		static inline REL::Relocation<decltype(thunk)> func;
	};
	struct BSGraphics_SetDirtyStates
	{
		static void thunk(bool isCompute);
		static inline REL::Relocation<decltype(thunk)> func;
	};
	void Install();

	/**
	 * @brief 自己实现的低侵入hook，只包含当前测试中需要的，用于测试，之后需要删除L
	 */
	void MyInstall();
	void InstallD3DHooks();
}
