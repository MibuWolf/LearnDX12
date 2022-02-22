//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************
// cbuffer表示是存放在常量缓冲区中的数据，register(b0)表示存放在0号寄存器，b表示类型为常量缓冲区描述符
// register(b0)表示该数据是存放在0号寄存器的常量缓冲区描述符，其中b表示常量缓冲区，t表示着色器资源描述符
// s表示采样器，i表示无序访问视图/描述符。
cbuffer cbPerObject : register(b0)		// 由外部传入的常量，放在常量缓冲区中，后续详细介绍常量缓冲区
{
	float4x4 gWorldViewProj;			// 在本示例中，常量值为MVP矩阵
};


// 输入参数数据结构定义
struct VertexIn
{
	// POSITION 和 COLOR为输入签名，输入前面的作用是将顶点缓冲区中的数据与Shader中的输入参数绑定映射
	// 确保存储在顶点缓冲区中的顶点坐标数据对于Shader中的PosL输入参数 颜色信息对应于Shader中的Color
	// 输入参数。具体输入签名的设置方式后续讨论
	float3 PosL  : POSITION;		// 输入参数顶点坐标
    float4 Color : COLOR;		// 输入参数顶点颜色
};
// 顶点着色器输出数据结构定义
struct VertexOut
{
	// SV_POSITION 和 COLOR同样为输入签名，他们即是顶点着色器的输出参数说明，也是后续
	// 阶段(eg:几何着色器/像素着色器)的输入参数。SV_POSITION中的SV表示是SystemValue的缩写
	// 其表示的含义是位于齐次空间的位置信息。
	float4 PosH  : SV_POSITION;	// 齐次空间的位置信息
    float4 Color : COLOR;		// 颜色值
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	// 经过MVP变换后坐标系来到了齐次空间，注意经过此变换后并非进入NDC空间
	// 也就是说此时四维坐标中的w并不为1.0f, 将齐次空间坐标转化到NDC空间(也就是
	// 四维坐标各个分量除以w)是由硬件执行的，此处要特别主要
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	// 简单将顶点颜色数据作为输出，经过光栅化后分别映射到每个像素
    vout.Color = vin.Color;
    
    return vout;
}

// 当然输入/输出参数也可以不进行数据结构封装，直接使用独立的参数表示输入输出，其效果也是一样的如下所示：
/**
*	以上VS函数的定义与此种写法是完全等价的
*	void VS(float4 PosH  : SV_POSITION, float4 Color : COLOR,
				out float4 PosH  : SV_POSITION, out float4 Color : COLOR)
*/

// 像素着色器，其输入参数为顶点着色其的输出即VertexOut
float4 PS(VertexOut pin) : SV_Target   // SV_Target 表示该函数的输出颜色float4要与目标缓冲区的格式一致
{
    return pin.Color;	// 直接将经过光栅化的顶点颜色值作为输出
}


