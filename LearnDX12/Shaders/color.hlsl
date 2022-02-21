//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************
// cbuffer��ʾ�Ǵ���ڳ����������е����ݣ�register(b0)��ʾ�����0�żĴ�����b��ʾ����Ϊ����������������
// register(b0)��ʾ�������Ǵ����0�żĴ����ĳ���������������������b��ʾ������������t��ʾ��ɫ����Դ������
// s��ʾ��������i��ʾ���������ͼ/��������
cbuffer cbPerObject : register(b0)		// ���ⲿ����ĳ��������ڳ����������У�������ϸ���ܳ���������
{
	float4x4 gWorldViewProj;			// �ڱ�ʾ���У�����ֵΪMVP����
};


// ����������ݽṹ����
struct VertexIn
{
	// POSITION �� COLORΪ����ǩ��������ǰ��������ǽ����㻺�����е�������Shader�е����������ӳ��
	// ȷ���洢�ڶ��㻺�����еĶ����������ݶ���Shader�е�PosL������� ��ɫ��Ϣ��Ӧ��Shader�е�Color
	// �����������������ǩ�������÷�ʽ��������
	float3 PosL  : POSITION;		// ���������������
    float4 Color : COLOR;		// �������������ɫ
};
// ������ɫ��������ݽṹ����
struct VertexOut
{
	// SV_POSITION �� COLORͬ��Ϊ����ǩ�������Ǽ��Ƕ�����ɫ�����������˵����Ҳ�Ǻ���
	// �׶�(eg:������ɫ��/������ɫ��)�����������SV_POSITION�е�SV��ʾ��SystemValue����д
	// ���ʾ�ĺ�����λ����οռ��λ����Ϣ��
	float4 PosH  : SV_POSITION;	// ��οռ��λ����Ϣ
    float4 Color : COLOR;		// ��ɫֵ
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	// ����MVP�任������ϵ��������οռ䣬ע�⾭���˱任�󲢷ǽ���NDC�ռ�
	// Ҳ����˵��ʱ��ά�����е�w����Ϊ1.0f, ����οռ�����ת����NDC�ռ�(Ҳ����
	// ��ά���������������w)����Ӳ��ִ�еģ��˴�Ҫ�ر���Ҫ
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	// �򵥽�������ɫ������Ϊ�����������դ����ֱ�ӳ�䵽ÿ������
    vout.Color = vin.Color;
    
    return vout;
}

// ��Ȼ����/�������Ҳ���Բ��������ݽṹ��װ��ֱ��ʹ�ö����Ĳ�����ʾ�����������Ч��Ҳ��һ����������ʾ��
/**
*	����VS�����Ķ��������д������ȫ�ȼ۵�
*	void VS(float4 PosH  : SV_POSITION, float4 Color : COLOR,
				out float4 PosH  : SV_POSITION, out float4 Color : COLOR)
*/

// ������ɫ�������������Ϊ������ɫ��������VertexOut
float4 PS(VertexOut pin) : SV_Target   // SV_Target ��ʾ�ú����������ɫfloat4Ҫ��Ŀ�껺�����ĸ�ʽһ��
{
    return pin.Color;	// ֱ�ӽ�������դ���Ķ�����ɫֵ��Ϊ���
}


