//***************************************************************************************
// Shadows.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	MaterialData matData = gMaterialData[gMaterialIndex];
	
    // 将顶点经过灯光视角下的MVP变换转换到灯光视角的投影空间
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
	
	// 对纹理坐标进行变换
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;
	
    return vout;
}

// 由于ShadowMap仅仅需要的是深度信息且并未设置任何RenderTarget，因此PS并不需要输出任何颜色值信息
void PS(VertexOut pin) 
{
	// 其实这些计算也都可以省略，因为本质上ShadowMap只需要深度信息，根本无需任何颜色值数据计算。
	// 获取材质信息
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseMapIndex = matData.DiffuseMapIndex;
	
	// 根据纹理进行采样
	diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
    // AlphaTest 对alpha值小于0.1的像素进行clip
    clip(diffuseAlbedo.a - 0.1f);
#endif
}


