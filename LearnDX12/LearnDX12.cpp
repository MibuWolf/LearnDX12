// LearnDX12.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "LearnDX12.h"
#include "Base/Geometry.h"
#include "SystemTimer.h"
#include "DXRenderDeviceManager.h"

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
HWND hWnd;
SystemTimer systemTimer;
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
std::unique_ptr<Geometry> mBoxGeo = nullptr;
ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


using namespace DirectX;

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

void				BuildBoxGemetry();
void				DrawBoxGemetry();
void				BuildRootSignature();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: 在此处放置代码。

	// 初始化全局字符串
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_LEARNDX12, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// 执行应用程序初始化:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	// 初始化Direct3D
	if (!DXRenderDeviceManager::GetInstance().InitD3DDevice(hWnd))
	{
		return FALSE;
	}
	DXRenderDeviceManager::GetInstance().ResetCommandList();
	BuildBoxGemetry();
	BuildRootSignature();
	DXRenderDeviceManager::GetInstance().ExecuteCommandQueue();

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LEARNDX12));

	MSG msg;

	// 主消息循环:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message != WM_QUIT)
			{
				systemTimer.Tick();
				DXRenderDeviceManager::GetInstance().Tick(systemTimer);
				DXRenderDeviceManager::GetInstance().Clear(systemTimer);

				DrawBoxGemetry();

				DXRenderDeviceManager::GetInstance().Present(systemTimer);
			}
		}
	}

	return (int)msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LEARNDX12));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_LEARNDX12);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // 将实例句柄存储在全局变量中

	hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// 分析菜单选择:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_ACTIVATE:
	{
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			systemTimer.Stop();
		}
		else
		{
			systemTimer.Start();
		}
	}break;
	case WM_EXITSIZEMOVE:
	{
		systemTimer.Start();
		DXRenderDeviceManager::GetInstance().OnResize();
	}break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void BuildBoxGemetry()
{
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<Geometry>();
	mBoxGeo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
	CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();
	mBoxGeo->UploadVertexData(pD3DDevice, pCommandList, vertices.data(), sizeof(Vertex), vbByteSize);
	mBoxGeo->UploadVertexIndexData(pD3DDevice, pCommandList, indices.data(), ibByteSize);

}

void DrawBoxGemetry()
{
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();
	ID3D12DescriptorHeap* CBVHeap = DXRenderDeviceManager::GetInstance().GetConstBufferDesc();

	if (pCommandList == nullptr)
		return;

	//ID3D12DescriptorHeap* descriptorHeaps[] = { CBVHeap };
	//pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//pCommandList->SetGraphicsRootSignature(mRootSignature.Get());


	// 向命令列表中设置顶点缓冲区描述符
	pCommandList->IASetVertexBuffers(0,	// 该接口支持设置多个缓冲区，此参数表示起始输入缓冲区的索引 
		1,								// 缓冲区的数量
		&mBoxGeo->VertexBufferView);	// 指向一个缓冲区描述符数组
	// 向命令列表中设置索引缓冲区描述符的数组指针
	pCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView);
	// 指定将要绘制的图元类型
	pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 以索引方式开始绘制(支持多实例渲染)
	pCommandList->DrawIndexedInstanced(
		36,		// 每个绘制实例需要绘制的索引数量	
		1,		// 每次绘制1个实例
		0,		// 从索引下标为0的位置开始读取索引
		0,		// BaseVertexLocation 根据索引查找顶点时的基础顶点偏移(eg: 多个模型顶点索引数据合并后，可用此偏移表示绘制第几个模型)
		0);		// 用于在多实例渲染时使用
}

void BuildRootSignature()
{
	ID3D12Device* pD3DDevice = DXRenderDeviceManager::GetInstance().GetD3DDevice();
	ID3D12GraphicsCommandList* pCommandList = DXRenderDeviceManager::GetInstance().GetCommandList();

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(pD3DDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}
