//=============================================================================
// Ssao.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//=============================================================================

cbuffer cbSsao : register(b0)
{
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gProjTex;
	float4   gOffsetVectors[14];

    // For SsaoBlur.hlsl
    float4 gBlurWeights[3];

    float2 gInvRenderTargetSize;

    // Coordinates given in view space.
    float    gOcclusionRadius;
    float    gOcclusionFadeStart;
    float    gOcclusionFadeEnd;
    float    gSurfaceEpsilon;
};

cbuffer cbRootConstants : register(b1)
{
    bool gHorizontalBlur;
};
 
// Nonnumeric values cannot be added to a cbuffer.
Texture2D gNormalMap    : register(t0);
Texture2D gDepthMap     : register(t1);
Texture2D gRandomVecMap : register(t2);

SamplerState gsamPointClamp : register(s0);
SamplerState gsamLinearClamp : register(s1);
SamplerState gsamDepthMap : register(s2);
SamplerState gsamLinearWrap : register(s3);

static const int gSampleCount = 14;
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

// 对屏幕空间坐标(6个点组成的两个三角形拼成屏幕矩形)
// 屏幕空间坐标的范围是[0,1]
static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};
 

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;

    vout.TexC = gTexCoords[vid];

    // 先将屏幕空间坐标转化到NDC空间，对于DX来说NDC空间x,y轴的范围是[-1,1]
	// z轴取0 表示的是先将屏幕空间坐标转化到相机空间的近截面上，然后由相机原点近截面
	// 坐标以及深度值最终确定相机空间的位置
    vout.PosH = float4(2.0f*vout.TexC.x - 1.0f, 1.0f - 2.0f*vout.TexC.y, 0.0f, 1.0f);
 
    // 将NDC空间坐标转化到相机空间(乘以ProjMatrix的逆矩阵)
    float4 ph = mul(vout.PosH, gInvProj);
	// 除以w是保证w值为1的相机空间坐标
    vout.PosV = ph.xyz / ph.w;

    return vout;
}

// Determines how much the sample point q occludes the point p as a function
// of distZ.
float OcclusionFunction(float distZ)
{
	//
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.
	//
	// We use the following function to determine the occlusion.  
	// 
	//
	//       1.0     -------------\
	//               |           |  \
	//               |           |    \
	//               |           |      \ 
	//               |           |        \
	//               |           |          \
	//               |           |            \
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps          z0            z1        
	//
	
	float occlusion = 0.0f;
	if(distZ > gSurfaceEpsilon)
	{
		float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
		
		// Linearly decrease occlusion from 1 to 0 as distZ goes 
		// from gOcclusionFadeStart to gOcclusionFadeEnd.	
		occlusion = saturate( (gOcclusionFadeEnd-distZ)/fadeLength );
	}
	
	return occlusion;	
}

float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, 而 A = gProj[2,2]  B = gProj[3,2].
	// 因此相机空间深度值Z的计算方式如下图所示：
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}
 
float4 PS(VertexOut pin) : SV_Target
{
	// p -- the point we are computing the ambient occlusion for.
	// n -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.

	// Get viewspace normal and z-coord of this pixel.  
    float3 n = normalize(gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz);
    float pz = gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r;
    pz = NdcDepthToViewDepth(pz);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	float3 p = (pz/pin.PosV.z)*pin.PosV;
	
	// 获取一随机方向向量并将向量范围扩展到[-1,1]范围(颜色值范围是[0,1])
	float3 randVec = 2.0f*gRandomVecMap.SampleLevel(gsamLinearWrap, 4.0f*pin.TexC, 0.0f).rgb - 1.0f;

	float occlusionSum = 0.0f;
	
	// 14个均匀分布的向量(正方体中心点到6个面中心点加8个顶点方向的向量)对随机向量反射以确保其随机性
	for(int i = 0; i < gSampleCount; ++i)
	{
		// 计算出沿随机向量randVec反射后的偏移向量
		float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
	
		// 判断该随机偏移向量与法线的正负管线
		float flip = sign( dot(offset, n) );
		
		// 确保随机偏移测试点q在法线正方向半球内
		float3 q = p + flip * gOcclusionRadius * offset;
		
		// 将q点坐标转化到纹理空间(q点对应的后台缓冲区/深度缓冲区纹理信息)
		float4 projQ = mul(float4(q, 1.0f), gProjTex);
		projQ /= projQ.w;

		// 对q点对应的深度进行采样获取其深度值
		float rz = gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r;
		// 将q点对应的深度值转换到视图空间
        rz = NdcDepthToViewDepth(rz);

		// 在视图空间q点所在的向量也是q(视点为原点)，按照切线公式就一定会存在一个float值t
		// 使得q点对应深度的位置点r = t* q => rz = t * q.z => t = rz / q.z因此可以计算出r处坐标
		float3 r = (rz / q.z) * q;
		
		// 根据法向量n与r-p向量的夹角判定是否对p点产生遮挡
		// 如果r-p 与 n在同一方向则说明 r处的深度值小于p处深度值（因为在视图空间内n一定是朝向原点(相机)方向）
		// r-p与n方向相同说明r-p也朝向相机方向也就说r.z < p.z 因此r处更接近相机也就是更高，因此会对p点产生遮蔽
		float dp = max(dot(n, normalize(r - p)), 0.0f);
		// 根据p r两点的距离为遮蔽增加遮蔽因子，距离越近遮蔽的影响越大
		float distZ = p.z - r.z;
        float occlusion = dp*OcclusionFunction(distZ);

		occlusionSum += occlusion;
	}
	
	// 用遮蔽数除以总采样数计算出遮蔽率
	occlusionSum /= gSampleCount;
	// 1-遮蔽率计算出可达率
	float access = 1.0f - occlusionSum;

	// 锐化 SSAO 地图的对比度，使 SSAO 影响更具有对比度
	return saturate(pow(access, 6.0f));
}
