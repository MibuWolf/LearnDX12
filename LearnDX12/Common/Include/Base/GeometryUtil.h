#pragma once
#include "Dx12.h"
#include "DX12Util.h"
#include "MathHelper.h"


struct ObjectConstants
{
    XMFLOAT4X4 World = MathHelper::Identity4x4();   // ÿ�����ζ��������ռ����
};

// Ϊ���ڹ��ռ���������Ч����������һЩ��������
struct PassConstants
{
    XMFLOAT4X4 View = MathHelper::Identity4x4();              // ����ռ�任����
    XMFLOAT4X4 InvView = MathHelper::Identity4x4();           // ����ռ�任�����         
    XMFLOAT4X4 Proj = MathHelper::Identity4x4();             // ͶӰ�ռ�任����
    XMFLOAT4X4 InvProj = MathHelper::Identity4x4();          // ͶӰ�ռ������
    XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();        // ��ͼ�ռ䵽ͶӰ�ռ����
    XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();     // ��ͼ�ռ䵽ͶӰ�ռ������
    XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };                // ���/�۾�λ��
    float cbPerObjectPad1 = 0.0f;                          
    XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };             // ��ȾĿ������С
    XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };          
    float NearZ = 0.0f;                                  // ������
    float FarZ = 0.0f;                                   // Զ����
    float TotalTime = 0.0f;                              // ����Ϸʱ��
    float DeltaTime = 0.0f;                              // ��ǰ֡���ʱ��
};

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};






// �򵥵���Ⱦ��ʾ������ʵ��Ŀ�е�ģ�����ݽṹ��������������ܸ�Ϊ����
// �������Ӧ����Ⱦ�����ݽṹҲ���Ϊ����
struct RenderItem
{
    RenderItem() = default;

    // Ҫ��Ⱦģ�ͼ����������ռ������Ϣ
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    // Dirty��ǣ�������ʾ�ڴ�(NumFramesDirty)֡��Դ��������������ѷ����ı䣬Ҳ����˵��ʱ
    // ��Ҫ���´�FrameResource�еĳ�������������
    int NumFramesDirty = gNumFrameResources;

    // ������ָ��ǰ��Ⱦ�������Ӧ��GPU��������������
    UINT ObjCBIndex = -1;
    // ��ǰ��Ⱦ�����ģ����Ϣ
    MeshGeometry* Geo = nullptr;

    // ��ǰģ��Ҫ���Ƶ�ͼԪ����
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // ���Ƹ�ģ��ʱ��DrawIndexedInstanced����.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};