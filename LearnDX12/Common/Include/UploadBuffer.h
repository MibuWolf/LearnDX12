#pragma once

#include "DX12Util.h"

template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) :
		mIsConstantBuffer(isConstantBuffer)
	{
		/**
		*	为了更为通用(支持各种不同定义的着色器参数结构)进行了模板封装
		*	此时着色器参数类型T为
		*	struct ObjectConstants
		*	{		
		*		XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
		*	};
		*/
		mElementByteSize = sizeof(T);

		// 如果是常量缓冲区则缓冲区大小必须为256的倍数
		// 这是因为硬件只能以256的倍数来访问常量缓冲区
		if (isConstantBuffer)
			mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));	// 根据资源参数类型计算出需要创建缓冲区的大小

		CD3DX12_HEAP_PROPERTIES heapPro(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resResc = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
		// 创建缓冲区资源
		ThrowIfFailed(device->CreateCommittedResource(		
			&heapPro,				// 缓冲区类型为上传堆，方便CPU修改上传数据
			D3D12_HEAP_FLAG_NONE,
			&resResc,				// 资源描述(大小 flag等信息)
			D3D12_RESOURCE_STATE_GENERIC_READ,	// 资源默认状态为读取
			nullptr,
			IID_PPV_ARGS(&mUploadBuffer)));	// 返回创建好的缓冲区


		// 将缓冲区映射到一内存块地址
		// 第一个参数0表示映射缓冲区子资源的索引(由于常量内存缓冲区中就只有一个资源因此为0)
		// 第二个参数为描述获取范围信息的参数，nullptr意味着获取整个缓冲
		// 第三个参数为返回内存块首地址
		ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));	//mMappedData为BYTE*型的内存块地址

		// We do not need to unmap until we are done with the resource.  However, we must not write to
		// the resource while it is in use by the GPU (so we must use synchronization techniques).
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
	~UploadBuffer()
	{
		if (mUploadBuffer != nullptr)
			mUploadBuffer->Unmap(0, nullptr);

		mMappedData = nullptr;
	}

	ID3D12Resource* Resource()const
	{
		return mUploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data)
	{
		memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;

	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};
