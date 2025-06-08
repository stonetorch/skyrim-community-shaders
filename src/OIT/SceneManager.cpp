//
// Created by 14982 on 25-3-15.
//

#include "SceneManager.h"

SceneObject::SceneObject(ID3D11DeviceContext* context, ID3D11Device* device, UINT indexCount, UINT startIndexLocation,
	INT baseVertexLocation)
{
	IndexCount = indexCount;
	StartIndexLocation = startIndexLocation;
	BaseVertexLocation = baseVertexLocation;

	// 获取顶点着色器
	context->VSGetShader(&vertexShader, nullptr, nullptr);

	// 获取像素着色器
	context->PSGetShader(&pixelShader, nullptr, nullptr);

	// 获取输入布局
	context->IAGetInputLayout(&inputLayout);

	// 获取图元拓扑结构
	context->IAGetPrimitiveTopology(&topology);

	// 获取深度模板状态
	context->OMGetDepthStencilState(&depthStencilState, &stencilRef);
	// 获取光栅化状态
	context->RSGetState(&rasterizerState);

	// 获取混合状态
	context->OMGetBlendState(&blendState, blendFactor, &sampleMask);

	// 获取常量缓冲区
	numConstantBuffers = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	context->VSGetConstantBuffers(0, numConstantBuffers, constantBuffers);

	// 获取索引缓冲区和格式
	context->IAGetIndexBuffer(&indexBuffer, &indexFormat, nullptr);

	// 获取索引数据
	if (indexBuffer) {
		D3D11_BUFFER_DESC desc;
		indexBuffer->GetDesc(&desc);

		if (desc.Usage != D3D11_USAGE_DYNAMIC && desc.Usage != D3D11_USAGE_STAGING) {
			// 创建一个可以映射的临时缓冲区
			D3D11_BUFFER_DESC stagingDesc = desc;
			stagingDesc.Usage = D3D11_USAGE_STAGING;
			stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			stagingDesc.BindFlags = 0;
			stagingDesc.MiscFlags = 0;

			ID3D11Buffer* stagingBuffer = nullptr;
			context->GetDevice(&device);
			HRESULT hr = device->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
			if (SUCCEEDED(hr)) {
				// 复制数据到临时缓冲区
				context->CopyResource(stagingBuffer, indexBuffer);

				// 映射临时缓冲区以读取数据
				D3D11_MAPPED_SUBRESOURCE mappedResource;
				hr = context->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

				if (SUCCEEDED(hr)) {
					// TODO 还没有拷贝数据
					// indexBuffer.resize(desc.ByteWidth);
					// memcpy(indexBuffer.data(), mappedResource.pData, desc.ByteWidth);
					//
					// context->Unmap(stagingBuffer, 0);
				}

				stagingBuffer->Release();
			}
		}
	}

	// 获取视口状态
	numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	context->RSGetViewports(&numViewports, viewports);

	// 获取着色器资源
	context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, shaderResources);
	numShaderResources = 0;
	while (numShaderResources < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT &&
		   shaderResources[numShaderResources]) {
		shaderResources[numShaderResources]->AddRef();
		numShaderResources++;
	}

	// 获取采样器状态
	context->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, samplers);
	numSamplers = 0;
	while (numSamplers < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT &&
		   samplers[numSamplers]) {
		samplers[numSamplers]->AddRef();
		numSamplers++;
	}

	// 获取顶点缓冲区
	numVertexBuffers = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;  //TODO 不知道从哪里搞过来
	context->IAGetVertexBuffers(0, numVertexBuffers, vertexBuffers, strides, offsets);
	if (numVertexBuffers > 0 && vertexBuffers[0]) {
		D3D11_BUFFER_DESC desc;
		vertexBuffers[0]->GetDesc(&desc);

		if (desc.Usage != D3D11_USAGE_DYNAMIC && desc.Usage != D3D11_USAGE_STAGING) {
			// 创建一个可以映射的临时缓冲区
			D3D11_BUFFER_DESC stagingDesc = desc;
			stagingDesc.Usage = D3D11_USAGE_STAGING;
			stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			stagingDesc.BindFlags = 0;
			stagingDesc.MiscFlags = 0;

			ID3D11Buffer* stagingBuffer = nullptr;
			context->GetDevice(&device);
			HRESULT hr = device->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
			if (SUCCEEDED(hr)) {
				// 复制数据到临时缓冲区
				context->CopyResource(stagingBuffer, vertexBuffers[0]);

				// 映射临时缓冲区以读取数据
				D3D11_MAPPED_SUBRESOURCE mappedResource;
				hr = context->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

				if (SUCCEEDED(hr)) {
					// 注意：这里代码有问题，SceneObject.h中vertexBuffer是指针而不是vector
					// 我们仅保留指针的实现
					// vertexBuffer.resize(desc.ByteWidth);
					// memcpy(vertexBuffer.data(), mappedResource.pData, desc.ByteWidth);

					context->Unmap(stagingBuffer, 0);
				}

				stagingBuffer->Release();
			}
		}
		vertexBuffer = vertexBuffers[0];  // 将指针赋值给成员变量
	}

	// 获取所有顶点缓冲区并增加引用计数
	for (UINT i = 0; i < numVertexBuffers; ++i) {
		if (vertexBuffers[i]) {
			vertexBuffers[i]->AddRef();
		}
	}
}

void SceneObject::Draw(ID3D11DeviceContext* context, ID3D11Device* device)
{
	// 设置顶点着色器
	if (vertexShader) {
		context->VSSetShader(vertexShader, nullptr, 0);
	}

	// 设置像素着色器
	if (pixelShader) {
		context->PSSetShader(pixelShader, nullptr, 0);
	}

	// 设置输入布局
	if (inputLayout) {
		context->IASetInputLayout(inputLayout);
	}

	// 设置图元拓扑结构
	context->IASetPrimitiveTopology(topology);

	// 设置深度模板状态
	if (depthStencilState) {
		context->OMSetDepthStencilState(depthStencilState, stencilRef);
	}

	// 设置光栅化状态
	if (rasterizerState) {
		context->RSSetState(rasterizerState);
	}

	// 设置混合状态
	if (blendState) {
		context->OMSetBlendState(blendState, blendFactor, sampleMask);
	}

	// 设置常量缓冲区
	if (numConstantBuffers > 0) {
		context->VSSetConstantBuffers(0, numConstantBuffers, constantBuffers);
	}

	// 设置索引缓冲区
	if (indexBuffer) {
		context->IASetIndexBuffer(indexBuffer, indexFormat, 0);
	}

	// 设置顶点缓冲区
	if (vertexBuffer) {
		context->IASetVertexBuffers(0, 1, &vertexBuffer, strides, offsets);
	}

	// 执行DrawIndexed调用
	context->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}

void SceneManager::OnDraw(ID3D11DeviceContext* context, ID3D11Device* device)
{
	//1. init context
	//2. render objects
	for (auto& sceneObject : capturedSceneObjects) {
		sceneObject.Draw(context, device);
	}
	//3. (maybe) recover previous context states
	//4. release resources
	for (auto& sceneObject : capturedSceneObjects) {
		if (sceneObject.indexBuffer) {
			sceneObject.indexBuffer->Release();
			sceneObject.indexBuffer = nullptr;
		}
		// 释放图形管线资源
		// 1. 释放顶点缓冲区
		for (UINT i = 0; i < sceneObject.numVertexBuffers; ++i) {
			if (sceneObject.vertexBuffers[i]) {
				sceneObject.vertexBuffers[i]->Release();
				sceneObject.vertexBuffers[i] = nullptr;
			}
		}
		// 2. 释放着色器资源
		for (UINT i = 0; i < sceneObject.numShaderResources; ++i) {
			if (sceneObject.shaderResources[i]) {
				sceneObject.shaderResources[i]->Release();
				sceneObject.shaderResources[i] = nullptr;
			}
		}
		// 3. 释放采样器状态
		for (UINT i = 0; i < sceneObject.numSamplers; ++i) {
			if (sceneObject.samplers[i]) {
				sceneObject.samplers[i]->Release();
				sceneObject.samplers[i] = nullptr;
			}
		}
		// 4. 释放常量缓冲区
		for (UINT i = 0; i < sceneObject.numConstantBuffers; ++i) {
			if (sceneObject.constantBuffers[i]) {
				sceneObject.constantBuffers[i]->Release();
				sceneObject.constantBuffers[i] = nullptr;
			}
		}
		for (auto constant_buffer : sceneObject.constantBuffers) {
			if (constant_buffer) {
				constant_buffer->Release();
			}
		}
	}
	capturedSceneObjects.clear();
}

void SceneObject::DumpVertexBuffer()
{
	if (!vertexBuffer) {
		return;
	}

	// 创建文件名，使用当前时间和地址作为唯一标识符
	char filename[MAX_PATH];
	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf_s(filename, "%s\\vertex_buffer_%04d%02d%02d_%02d%02d%02d_%p.bin",
		VERTEX_BUFFER_DUMP_PATH,
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond,
		vertexBuffer);

	// 获取顶点缓冲区描述
	D3D11_BUFFER_DESC desc;
	vertexBuffer->GetDesc(&desc);

	// 创建目录（如果不存在）
	CreateDirectoryA(VERTEX_BUFFER_DUMP_PATH, NULL);

	// 打开文件准备写入
	FILE* file = nullptr;
	if (fopen_s(&file, filename, "wb") != 0 || !file) {
		return;
	}

	// 创建临时缓冲区用于映射
	ID3D11Device* device = nullptr;
	vertexBuffer->GetDevice(&device);
	if (!device) {
		fclose(file);
		return;
	}

	ID3D11DeviceContext* context = nullptr;
	device->GetImmediateContext(&context);
	if (!context) {
		device->Release();
		fclose(file);
		return;
	}

	// 创建一个可以映射的临时缓冲区
	D3D11_BUFFER_DESC stagingDesc = desc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.BindFlags = 0;
	stagingDesc.MiscFlags = 0;

	ID3D11Buffer* stagingBuffer = nullptr;
	HRESULT hr = device->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
	if (SUCCEEDED(hr) && stagingBuffer) {
		// 复制数据到临时缓冲区
		context->CopyResource(stagingBuffer, vertexBuffer);

		// 映射临时缓冲区以读取数据
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		hr = context->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

		if (SUCCEEDED(hr)) {
			// 写入文件头部信息
			fprintf(file, "Vertex Buffer Dump\n");
			fprintf(file, "ByteWidth: %u\n", desc.ByteWidth);
			fprintf(file, "BindFlags: %u\n", desc.BindFlags);
			fprintf(file, "CPUAccessFlags: %u\n", desc.CPUAccessFlags);
			fprintf(file, "MiscFlags: %u\n", desc.MiscFlags);
			fprintf(file, "StructureByteStride: %u\n", desc.StructureByteStride);
			fprintf(file, "Usage: %u\n\n", desc.Usage);

			// 写入原始顶点数据
			fwrite(mappedResource.pData, 1, desc.ByteWidth, file);

			// 取消映射
			context->Unmap(stagingBuffer, 0);
		}

		stagingBuffer->Release();
	}

	// 释放资源
	context->Release();
	device->Release();
	fclose(file);
}

void SceneManager::AddSceneObject(const SceneObject& sceneObject)
{
	// TODO 这里不知道要怎么实现
	// TODO 看之前别人写的东西，好像可以通过std::move优化性能？好吧，我觉得其实不会提升性能，再看看
	// 其实根本没用到，毕竟可以直接在SceneManager的列表中创建，内存还是连一块的，根本不用考虑拷贝的问题
	capturedSceneObjects.push_back(std::move(sceneObject));
}

void SceneManager::AddSceneObject(ID3D11DeviceContext* context, ID3D11Device* device, UINT IndexCount,
	UINT StartIndexLocation, INT BaseVertexLocation)
{
	capturedSceneObjects.push_back(SceneObject(context, device, IndexCount, StartIndexLocation, BaseVertexLocation));
}
