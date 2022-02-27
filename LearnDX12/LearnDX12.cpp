// LearnDX12.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "LearnDX12.h"
#include "MathHelper.h"
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

float mTheta = 1.5f * XM_PI;
float mPhi = XM_PIDIV4;
float mRadius = 5.0f;
PassConstants MainPassCB;
// 所有几何体的渲染项
std::vector<std::unique_ptr<RenderItem>> AllRenderItems;
XMFLOAT3 EyePos = { 0.0f, 0.0f, 0.0f };
XMFLOAT4X4	View = MathHelper::Identity4x4();
XMFLOAT4X4	Proj = MathHelper::Identity4x4();

void UpdateGeometry();

void		UpdateCamera(SystemTimer& Timer);
void		UpdateRenderPassCB(SystemTimer& Timer);
void		UpdateObjectCB(SystemTimer& Timer);

void		Tick(SystemTimer& Timer);
void		Draw(SystemTimer& Timer);

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


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
				Tick(systemTimer);
				Draw(systemTimer);
			}
		}
	}

	return (int)msg.wParam;
}





void		UpdateCamera(SystemTimer& Timer)
{
	// Convert Spherical to Cartesian coordinates.
	EyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	EyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	EyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(EyePos.x, EyePos.y, EyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&View, view);
}

// 跟新RenderPass参数并将数据上传到常量缓冲区
void		UpdateRenderPassCB(SystemTimer& Timer)
{
	XMMATRIX view = XMLoadFloat4x4(&View);
	XMMATRIX proj = XMLoadFloat4x4(&Proj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&MainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&MainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&MainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&MainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&MainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&MainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	MainPassCB.EyePosW = EyePos;
	MainPassCB.RenderTargetSize = XMFLOAT2(1280.0f, 768.0f);
	MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / 1280.0f, 1.0f / 768.0f);
	MainPassCB.NearZ = 1.0f;
	MainPassCB.FarZ = 1000.0f;
	MainPassCB.TotalTime = Timer.TotalTime();
	MainPassCB.DeltaTime = Timer.DeltaTime();

	// 对上传到常量缓冲区的代码接口封装，具体方法请参照顶点缓冲区的数据上传方式
	DXRenderDeviceManager::GetInstance().UploadRenderPassConstantBuffer(MainPassCB);
}

void		UpdateObjectCB(SystemTimer& Timer)
{
	for (auto& Item : AllRenderItems)
	{
		// 仅仅当当前帧此对象常量缓冲区发生变化时才需要更新
		if (Item->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&Item->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			DXRenderDeviceManager::GetInstance().UploadObjectConstantBuffer(objConstants, Item->ObjCBIndex);
		}
	}
}

void		Tick(SystemTimer& Timer)
{
	DXRenderDeviceManager::GetInstance().Tick(Timer);
}


void		Draw(SystemTimer& Timer)
{
	DXRenderDeviceManager::GetInstance().Clear(systemTimer, mBoxGeo ? mBoxGeo->PSO.Get() : nullptr);

	if (mBoxGeo)
		mBoxGeo->Draw(systemTimer);

	DXRenderDeviceManager::GetInstance().Present(systemTimer);
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
