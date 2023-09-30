/******************************************************************************
	CONKORS COMPANION
	www.conkors.com
	Copyright (C) Conkors LLC

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "img.h"
#include "webapi.h"

// Window Callback Procedure for Screenshots
LRESULT CALLBACK OverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static POINT p1;
	static POINT p2;
	static BOOL isSelecting = FALSE;

	switch (uMsg)
	{
	case WM_RBUTTONDOWN: {
		p1 = { 0 };
		p2 = { 0 };
		isSelecting = FALSE;
		DWORD* tid = (DWORD*)GetPropA(hwnd, "thread");
		PostThreadMessage(*tid, WM_HIDE_OVERLAY, 0, 0);
		break;
	}
	case WM_LBUTTONDOWN:
		p1.x = LOWORD(lParam);
		p1.y = HIWORD(lParam);
		isSelecting = TRUE;
		break;
	case WM_LBUTTONUP:
		if (isSelecting)
		{
			p2.x = LOWORD(lParam);
			p2.y = HIWORD(lParam);

			int left = min(p1.x,p2.x);
			int right = max(p1.x, p2.x);
			int top = min(p1.y, p2.y);
			int bottom = max(p1.y, p2.y);

			RECT* rct = (RECT*)GetPropA(hwnd, "selection");
			DWORD* tid = (DWORD*)GetPropA(hwnd, "thread");
			if (rct) {
				rct->left = left;
				rct->top = top;
				rct->right = right;
				rct->bottom = bottom;
			}
			isSelecting = FALSE;
			PostThreadMessage(*tid, WM_HIDE_OVERLAY, 0, 0);
		}
		break;
	case WM_MOUSEMOVE:
		InvalidateRect(hwnd, NULL, TRUE); // Invalidate the entire client area

		if (isSelecting)
		{
			HDC hdc = GetDC(hwnd);
			RECT rect;
			GetClientRect(hwnd, &rect);

			POINT currentPoint;
			GetCursorPos(&currentPoint);
			HRGN region = CreateRectRgn(p1.x, p1.y, currentPoint.x, currentPoint.y);

			FrameRgn(hdc, region, (HBRUSH)GetStockObject(BLACK_BRUSH), 2, 2);

			DeleteObject(region);
			ReleaseDC(hwnd, hdc);
		}
		break;
	case WM_PAINT:
		if (isSelecting) {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			RECT rect;
			GetClientRect(hwnd, &rect);

			POINT currentPoint;
			GetCursorPos(&currentPoint);
			ScreenToClient(hwnd, &currentPoint);

			HRGN region = CreateRectRgn(p1.x, p1.y, currentPoint.x, currentPoint.y);

			FrameRgn(hdc, region, (HBRUSH)GetStockObject(BLACK_BRUSH), 2, 2);

			DeleteObject(region);
			EndPaint(hwnd, &ps);
		}
		break;
	case WM_DESTROY:
		p1 = { 0 };
		p2 = { 0 };
		isSelecting = FALSE;
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

ImgCore::ImgCore() {
}
ImgCore::~ImgCore() {
	Gdiplus::GdiplusShutdown(gdiToken_);
}

bool ImgCore::init(std::shared_ptr<AccountManager> acm) {
	if (!acm) {
		return false;
	}
	acm_ = acm;

	captureState_ = false;

	Gdiplus::GdiplusStartup(&gdiToken_, &tmp_, NULL);

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// Create Window Class for Screenshot Overlay
	WNDCLASSEX wc = { 0 };
	wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = OverlayProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_CROSS);
	wc.lpszClassName = L"CKCAP";
	RegisterClassEx(&wc);

	shotWnd_ = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		wc.lpszClassName,
		L"Conkors Screen Capture",
		WS_POPUP | WS_VISIBLE,
		0, 0, screenWidth, screenHeight,
		NULL, NULL, wc.hInstance, NULL);
	ShowWindow(shotWnd_, SW_HIDE);

	selection_ = { 0 };
	SetPropA(shotWnd_, "selection", (HANDLE)&selection_);

	return true;
}

bool ImgCore::savePNG(HBITMAP hbitmap, std::vector<BYTE>& data)
{
	/* Converts the data in hbitmap to PNG image data in data */
	////////////////////////////////////////////////////////////

	Gdiplus::Bitmap bmp(hbitmap, nullptr);

	// Write to IStream
	IStream* istream = nullptr;
	if (CreateStreamOnHGlobal(NULL, TRUE, &istream) != 0)
		return false;

	CLSID clsid_png;
	if (CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &clsid_png) != 0)
		return false;
	Gdiplus::Status status = bmp.Save(istream, &clsid_png);
	if (status != Gdiplus::Status::Ok)
		return false;

	// Get memory handle associated with istream
	HGLOBAL hg = NULL;
	if (GetHGlobalFromStream(istream, &hg) != S_OK)
		return 0;

	// Copy IStream to buffer
	int bufsize = GlobalSize(hg);
	data.resize(bufsize);

	// Lock & unlock memory
	LPVOID pimage = GlobalLock(hg);
	if (!pimage)
		return false;
	memcpy(&data[0], pimage, bufsize);
	GlobalUnlock(hg);

	istream->Release();
	return true;
}

void ImgCore::overlay(const HBITMAP& bmap) {
	/* Create the overlay window, initialized to hidden state */

	// Set the thread ID so overlay window can message our queue
	DWORD dwThreadId = GetCurrentThreadId();
	SetPropA(shotWnd_, "thread", (HANDLE)&dwThreadId);

	// Update the screenshot BG
	HBRUSH hBrush = CreatePatternBrush(bmap);
	SetClassLongPtr(shotWnd_, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(hBrush));
	InvalidateRect(shotWnd_, NULL, TRUE);

	// Show the overlay
	SetWindowPos(shotWnd_, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	ShowWindow(shotWnd_, SW_SHOW);
	SetFocus(shotWnd_);

	// Message loop
	MSG msg;
	BOOL bRet;
	while (captureState_ && (bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (msg.message == WM_HIDE_OVERLAY)
		{
			break;
		}
		else {
			DispatchMessage(&msg);
		}
	}

	// Make sure to hide the overlay when done
	ShowWindow(shotWnd_, SW_HIDE);
}

void ImgCore::save()
{
	if (!gdiToken_ || captureState_) { return; }

	captureState_ = true;

	// Capture Full Screen
	RECT inRec;
	GetClientRect(GetDesktopWindow(), &inRec);
	int width = inRec.right - inRec.left;
	int height = inRec.bottom - inRec.top;

	HDC hdc = GetDC(NULL);
	HDC memdc = CreateCompatibleDC(hdc);
	HBITMAP hbitmap = CreateCompatibleBitmap(hdc, width, height);
	HGDIOBJ oldbmp = SelectObject(memdc, hbitmap);
	BitBlt(memdc, 0, 0, width, height, hdc, 0, 0, SRCCOPY);

	// Pass to Window Overlay, get back desired crop
	overlay(hbitmap);
	int cropWidth = selection_.right - selection_.left;
	int cropHeight = selection_.bottom - selection_.top;
	if (!cropWidth || !cropHeight) {
		printf("CK::IMG Illegal bounds for screenshot, resetting.\n");
		selection_ = { 0 };
		captureState_ = false;
		return;
	}

	// Crop to selection
	HDC cropdc = CreateCompatibleDC(hdc);
	HBITMAP croppedBitmap = CreateCompatibleBitmap(hdc, cropWidth, cropHeight);
	HGDIOBJ oldCropBmp = SelectObject(cropdc, croppedBitmap);
	BitBlt(cropdc, 0, 0, cropWidth, cropHeight, memdc, selection_.left, selection_.top, SRCCOPY);

	SelectObject(memdc, oldbmp);
	SelectObject(cropdc, oldCropBmp);
	DeleteDC(memdc);
	DeleteDC(cropdc);
	ReleaseDC(0, hdc);

	// Save as PNG
	std::vector<BYTE> data;
	if (savePNG(croppedBitmap, data))
	{
		std::string baseFilePath = acm_->getScreenshotDir();
		std::string filePrefix = "CKSNAP_";
		std::string timestamp = webapi::getTimestamp();
		std::string fileFormat = ".png";
		std::string filePath = baseFilePath + filePrefix + timestamp + fileFormat;

		std::ofstream fout(filePath, std::ios::binary);
		fout.write((char*)data.data(), data.size());

		printf("CK::IMG Saved screenshot!: %s\n", filePath.c_str());
		uploadImg(filePath);
	}

	DeleteObject(hbitmap);
	DeleteObject(croppedBitmap);
	selection_ = { 0 };
	captureState_ = false;
}

void ImgCore::uploadImg(std::string filePath) {
	acm_->uploadMedia(filePath, false);
}

