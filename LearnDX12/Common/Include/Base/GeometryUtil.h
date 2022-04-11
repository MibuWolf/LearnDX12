#pragma once
#include "Dx12.h"
#include "DX12Util.h"
#include "MathHelper.h"


struct ObjectConstants
{
    XMFLOAT4X4 World = MathHelper::Identity4x4();   // ÿ�����ζ��������ռ����
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT     MaterialIndex;     // ��ǰ��Ⱦ����ʹ�õĲ�������(��Ӧ����������е�����)
    UINT     ObjPad0;
    UINT     ObjPad1;
    UINT     ObjPad2;
};


// �ڳ����������д洢�Ĳ�������
struct MaterialConstants
{
    // �����䷴����
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    // 0�㷴����
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    // �ֲڶ�
    float Roughness = 0.25f;

    // ����������������
    UINT DiffuseMapIndex = 0;
    // Used in texture mapping.
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

    UINT MaterialPad0;
    UINT MaterialPad1;
    UINT MaterialPad2;
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

    // ��������ɫ
    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    // ���֧��MaxLightsյ�ƣ�ǰNUM_DIR_LIGHTS��Ϊƽ�й�
    // [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS)��Ϊ���Դ
    // [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)Ϊ̽�յ�
    // �������ÿ�ֵƹ�ʱ�����������Ӧλ�ã�����ɫ���ж�ÿյ�ƹⰴ������������λ��ȡ��ͬ�ƹ����ͼ���
    Light Lights[MaxLights];
};

struct Vertex
{
    XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
};






// �򵥵���Ⱦ��ʾ������ʵ��Ŀ�е�ģ�����ݽṹ��������������ܸ�Ϊ����
// �������Ӧ����Ⱦ�����ݽṹҲ���Ϊ����
struct RenderItem
{
    RenderItem() = default;

    // Ҫ��Ⱦģ�ͼ����������ռ������Ϣ
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};