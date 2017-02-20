#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment (lib, "windowscodecs.lib")

#include <windows.h>
#include <wincodec.h>
#include "resource.h"

TCHAR szClassName[] = TEXT("Window");

IStream* CreateStreamOnResource(LPCTSTR lpName, LPCTSTR lpType)
{
	IStream * ipStream = NULL;

	HRSRC hrsrc = FindResource(NULL, lpName, lpType);
	if (hrsrc == NULL)
		goto Return;

	DWORD dwResourceSize = SizeofResource(NULL, hrsrc);
	HGLOBAL hglbImage = LoadResource(NULL, hrsrc);
	if (hglbImage == NULL)
		goto Return;

	LPVOID pvSourceResourceData = LockResource(hglbImage);
	if (pvSourceResourceData == NULL)
		goto Return;

	HGLOBAL hgblResourceData = GlobalAlloc(GMEM_MOVEABLE, dwResourceSize);
	if (hgblResourceData == NULL)
		goto Return;

	LPVOID pvResourceData = GlobalLock(hgblResourceData);

	if (pvResourceData == NULL)
		goto FreeData;

	CopyMemory(pvResourceData, pvSourceResourceData, dwResourceSize);

	GlobalUnlock(hgblResourceData);

	if (SUCCEEDED(CreateStreamOnHGlobal(hgblResourceData, TRUE, &ipStream)))
		goto Return;

FreeData:
	GlobalFree(hgblResourceData);

Return:
	return ipStream;
}

IWICBitmapSource* LoadBitmapFromStream(IStream * ipImageStream)
{
	IWICBitmapSource* ipBitmap = NULL;

	IWICBitmapDecoder* ipDecoder = NULL;
	if (FAILED(CoCreateInstance(CLSID_WICPngDecoder, NULL, CLSCTX_INPROC_SERVER, __uuidof(ipDecoder), reinterpret_cast<void**>(&ipDecoder))))
		goto Return;

	if (FAILED(ipDecoder->Initialize(ipImageStream, WICDecodeMetadataCacheOnLoad)))
		goto ReleaseDecoder;

	UINT nFrameCount = 0;
	if (FAILED(ipDecoder->GetFrameCount(&nFrameCount)) || nFrameCount != 1)
		goto ReleaseDecoder;

	IWICBitmapFrameDecode* ipFrame = NULL;
	if (FAILED(ipDecoder->GetFrame(0, &ipFrame)))
		goto ReleaseDecoder;

	WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, ipFrame, &ipBitmap);
	ipFrame->Release();

ReleaseDecoder:
	ipDecoder->Release();
Return:
	return ipBitmap;
}

HBITMAP CreateHBITMAP(IWICBitmapSource* ipBitmap)
{
	HBITMAP hbmp = NULL;

	UINT width = 0;
	UINT height = 0;
	if (FAILED(ipBitmap->GetSize(&width, &height)) || width == 0 || height == 0)
		goto Return;

	BITMAPINFO bminfo;

	ZeroMemory(&bminfo, sizeof(bminfo));
	bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bminfo.bmiHeader.biWidth = width;
	bminfo.bmiHeader.biHeight = -((LONG)height);
	bminfo.bmiHeader.biPlanes = 1;
	bminfo.bmiHeader.biBitCount = 32;
	bminfo.bmiHeader.biCompression = BI_RGB;

	void * pvImageBits = NULL;
	HDC hdcScreen = GetDC(NULL);
	hbmp = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
	ReleaseDC(NULL, hdcScreen);
	if (hbmp == NULL)
		goto Return;

	const UINT cbStride = width * 4;
	const UINT cbImage = cbStride * height;
	if (FAILED(ipBitmap->CopyPixels(NULL, cbStride, cbImage, static_cast<BYTE *>(pvImageBits))))
	{
		DeleteObject(hbmp);
		hbmp = NULL;
	}

Return:
	return hbmp;
}

HBITMAP LoadImageFromResources(LPCTSTR lpName, LPCTSTR lpType)
{
	HBITMAP hbmpSplash = NULL;

	IStream* ipImageStream = CreateStreamOnResource(lpName, lpType);
	if (ipImageStream == NULL)
		goto Return;

	IWICBitmapSource* ipBitmap = LoadBitmapFromStream(ipImageStream);
	if (ipBitmap == NULL)
		goto ReleaseStream;

	hbmpSplash = CreateHBITMAP(ipBitmap);
	ipBitmap->Release();

ReleaseStream:
	ipImageStream->Release();

Return:
	return hbmpSplash;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HBITMAP hBitmap;
	switch (msg)
	{
	case WM_CREATE:
		CoInitialize(NULL);
		hBitmap = LoadImageFromResources(MAKEINTRESOURCE(IDB_PNG1), TEXT("PNG"));
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		if (hBitmap)
		{
			HDC hdcMem = CreateCompatibleDC(hdc);
			HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
			BitBlt(hdc, 0, 0, 100, 100, hdcMem, 0, 0, SRCCOPY);
			SelectObject(hdcMem, hOldBitmap);
			DeleteDC(hdcMem);
		}

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		DeleteObject(hBitmap);
		CoUninitialize();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("リソースからPNGファイルを読み込んで表示する（WIC : Windows Imaging Componentを使用）"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
