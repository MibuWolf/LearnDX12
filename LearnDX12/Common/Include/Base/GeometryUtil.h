#pragma once
#include "Dx12.h"
#include "DX12Util.h"
#include "MathHelper.h"


struct ObjectConstants
{
    XMFLOAT4X4 World = MathHelper::Identity4x4();   // 每个几何对象的世界空间矩阵
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT     MaterialIndex;     // 当前渲染对象使用的材质索引(对应与材质数组中的索引)
    UINT     ObjPad0;
    UINT     ObjPad1;
    UINT     ObjPad2;
};


// 在常量缓冲区中存储的材质数据
struct MaterialConstants
{
    // 漫反射反照率
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    // 0°反射率
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    // 粗糙度
    float Roughness = 0.25f;

    // 增加纹理索引数据
    UINT DiffuseMapIndex = 0;
    // Used in texture mapping.
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

    UINT MaterialPad0;
    UINT MaterialPad1;
    UINT MaterialPad2;
};

// 为后期光照计算与其他效果计算增加一些常用数据
struct PassConstants
{
    XMFLOAT4X4 View = MathHelper::Identity4x4();              // 相机空间变换矩阵
    XMFLOAT4X4 InvView = MathHelper::Identity4x4();           // 相机空间变换逆矩阵         
    XMFLOAT4X4 Proj = MathHelper::Identity4x4();             // 投影空间变换矩阵
    XMFLOAT4X4 InvProj = MathHelper::Identity4x4();          // 投影空间逆矩阵
    XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();        // 视图空间到投影空间矩阵
    XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();     // 视图空间到投影空间逆矩阵
    XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };                // 相机/眼睛位置
    float cbPerObjectPad1 = 0.0f;                          
    XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };             // 渲染目标对象大小
    XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };          
    float NearZ = 0.0f;                                  // 近截面
    float FarZ = 0.0f;                                   // 远截面
    float TotalTime = 0.0f;                              // 总游戏时间
    float DeltaTime = 0.0f;                              // 当前帧间隔时间

    // 环境光颜色
    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    // 最多支持MaxLights盏灯，前NUM_DIR_LIGHTS个为平行光
    // [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS)个为点光源
    // [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)为探照灯
    // 因此增加每种灯光时放置在数组对应位置，在着色器中对每盏灯光按其所处的数组位置取不同灯光类型计算
    Light Lights[MaxLights];
};

struct Vertex
{
    XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
};






// 简单的渲染项示例，真实项目中的模型数据结构材质所需参数可能更为复杂
// 因此其相应的渲染项数据结构也会更为复杂
struct RenderItem
{
    RenderItem() = default;

    // 要渲染模型几何体的世界空间矩阵信息
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