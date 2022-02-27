#pragma once
#include "DX12Util.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryUtil.h"


// ֡��Դ���ڼ�¼�ʹ洢CPUΪ����ÿ֡�����б��������Դ��������Ϣ
struct FrameResource
{
public:

    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // ÿ֡����Ҫ�и�������������������ڼ�¼��ǰ֡CPUҪִ�е�Command��Ϣ
    // ���GPU����ʹ�õ�ǰ������������л��ƣ����ڻ������ǰCPU�����ٶ������û��޸�
    ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // ��Ⱦÿһ֡ʱ��Ҫ���õĳ����������е����ݻ���Դ
    // ͬ�����GPU����ʹ�õ�ǰ������������л��ƣ����ڻ������ǰCPU�����ٶ������û��޸�
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    // ��¼��ǰ֡������Χ������Ϣ�������жϵ�ǰGPU�Ƿ�����ʹ�ø�֡��Դ����
    // �жϷ�ʽ��DXRenderDeviceManager::FlushCommandQueue()��ͬ
    UINT64 Fence = 0;
};