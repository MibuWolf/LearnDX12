//***************************************************************************************
// DrawNormals.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 0
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
	float3 TangentU : TANGENT;
};

struct VertexOut
{
	float4 PosH     : SV_POSITION;
    float3 NormalW  : NORMAL;
	float3 TangentW : TANGENT;
	float2 TexC     : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	
    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
	vout.TangentW = mul(vin.TangentU, (float3x3)gWorld);

    // Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
	
	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	// 对与光栅化后的法线进行归一化处理
    pin.NormalW = normalize(pin.NormalW);
	
    // NOTE: We use interpolated vertex normal for SSAO.

    // 绘制每个像素的法向量比较简单，将法向量转化到视图空间并输出以方便后续SSAO后处理阶段使用
    float3 normalV = mul(pin.NormalW, (float3x3)gView);
    return float4(normalV, 0.0f);
}


//  当前播放动作的动作索引
int m_CurrentAnimIndex;
//  当前动作播放时间
float m_CurrentAnimTime;


// 轨迹点
class TrajectoryPoint
{
	// 当前轨迹点在世界空间内的坐标位置
	Vector3 m_Poition;
	// 多久时间后到达此位置
	float m_TimeDelay;
};

// 目标信息，由Gameplay层计算出轨迹信息
class Goal
{
	// 目标轨迹
	Array<TrajectoryPoint>	m_DesiredTrajectory;

	// 目标姿态
	Stance m_DesiredStance;
};


float  ComputCost(Pose currentPose, Pose candidatePose, Goal goal)
{
	float cost = 0.0f;

	// 计算从当前Pose跳转到候选Pose的动作匹配开销
	cost += ComputePoseMatchCost(currentPose, candidatePose);

	// 额外控制响应度的参数，用于调整手感和轨迹相应，该值为0表示不考虑轨迹匹配仅考虑动作姿势匹配
	// 该值越大，受轨迹影响越大操作越灵敏操作相应越及时
	static float responsivity = 1.0f;

	// 计算候选Pose轨迹与目标轨迹的差异开销
	cost += ComputeTrajectoryCost(candidatePose, goal);

	return cost;
}


// 每帧更新动作，Goal为根据玩家操作和角色当前状态
// 等逻辑判断出来的目标信息(eg: 向右侧45°奔跑)
// dt则是帧间隔时间
void	AnimUpdate(Goal goal, float dt)
{
	m_CurrentAnimTime += dt;		// 更新动作播放时间
	// 根据当前播放的动作id,播放时间评估出当前所属的姿势(Pose)信息
	Pose currentPose = EvaluateLerpedPoseFormData(m_CurrentAnimIndex, m_CurrentAnimTime);

	// 定义当前Pose要跳转到的最佳Pose及其跳转代价
	float bestCost = 10000000;
	Pose bestPose;

	// 遍历动捕数据中所有的姿势(Pose)信息，找出当前Pose下跳转开销最小的最佳目标Pose
	for (int i = 0; i < m_Poses.size(); ++i)
	{
		Pose candidatePose = Pose[i];   // 候选Pose信息
		// 根据当前Pose，目标goal计算出跳转到当前候选Pose(candidatePose)的代价开销
		float candidateCost = ComputCost(currentPose, candidatePose, goal);
		// 记录跳转开销最小的候选Pose
		if (candidateCost < bestCost)
		{
			bestCost = candidateCost;
			bestPose = candidatePose;
		}
	}

	// 如果当前动作索引和目标Pose是同一动作，并且当前动作播放时间与目标Pose所属的动作播放时间基本相同(误差在0.2s内)，
	// 则认定最佳目标跳转目标仍然是当前Pose，也就是说应该继续播放当前动作。
	bool IsAtTheSameLocation = (m_CurrentAnimIndex == bestPose.m_AnimIndex && fabs(m_CurrentAnimTime - bestPose.m_AnimTime) < 0.2f);

	// 如果最佳候选Pose并非当前动作Pose则直接融合到最佳候选Pose进行播放
	if (!IsAtTheSameLocation)
	{
		m_CurrentAnimIndex = bestPose.m_AnimIndex;
		m_CurrentAnimTime = bestPose.m_AnimTime;
		PlayAnimStartingAtTime(m_CurrentAnimIndex, m_CurrentAnimTime, m_BlendTime);
	}
}