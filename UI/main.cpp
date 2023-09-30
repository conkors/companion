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

#define _CRT_SECURE_NO_WARNINGS
#include "stdafx.h"

#include <string>
#include <thread>
#include <algorithm>
#include <dxgi.h>

#include "webapi.h"
#include "account.h"
#include "vid.h"
#include "img.h"

#include "ConkorsCompanion.h"
#include <QtWidgets/QApplication>

static inline std::vector<std::string> queryAdapters()
{
	std::vector<std::string> adapters;

	IDXGIFactory* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	if (FAILED(hr)) {
		throw "FAILED TO ENUMERATE VIDEO CARDS!";
		return adapters;
	}

	// Enumerate DXGI adapters
	IDXGIAdapter* adapter = nullptr;
	for (UINT i = 0; dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC adapterDesc;
		hr = adapter->GetDesc(&adapterDesc);
		if (adapterDesc.VendorId == 0x1414 && adapterDesc.DeviceId == 0x8c)
			continue;
		if (SUCCEEDED(hr)) {
			std::wstring ws(adapterDesc.Description);
			std::string adapterName(ws.begin(), ws.end());
			adapters.push_back(adapterName);
		}
	}
	dxgiFactory->Release();

	return adapters;
}

static void handleMultipleInstances(const std::string& authArg) {
	// Another instance is already running
	// If we received an auth token, lets pipe that over and then silently exit
	if (!authArg.empty()) {
		HANDLE hPipe = CreateFileA("\\\\.\\pipe\\ConkorsCompanion", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hPipe != INVALID_HANDLE_VALUE) {
			// Send the authentication token to the running instance
			WriteFile(hPipe, authArg.data(), authArg.size(), NULL, NULL);
			CloseHandle(hPipe);
		}
		else {
			printf("CK::MULTI Failed to pipe auth token!!\n");
		}
	}
	else {
		// Otherwise show an error and quit
		MessageBoxA(NULL, "Another instance of the Companion is already running.", "Application Already Running", MB_ICONINFORMATION | MB_OK);
	}
}

int main(int argc, char *argv[])
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());

#ifdef _DEBUG
	// REDIRECT COMMON STREAMS TO CONSOLE
	FILE* fileStream;
	freopen_s(&fileStream, "CONIN$", "r", stdin);
	freopen_s(&fileStream, "CONOUT$", "w", stdout);
	freopen_s(&fileStream, "CONOUT$", "w", stderr);
#else
	freopen("log.txt", "w", stdout);
	setvbuf(stdout, NULL, _IONBF, 0);

	// HIDE CONSOLE WINDOW
	HWND hwndConsole = GetConsoleWindow();
	ShowWindow(hwndConsole, SW_HIDE);
#endif
	// NEEDED FOR DUPLICATOR DISPLAY CAPTURE?
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// AUTH PASSED IN VIA URI
	std::string authArg;
	if (argc == 2) {
		authArg = std::string(argv[1]);
	}

	// CHECK MULTIPLE INSTANCES
	HANDLE hMutex = CreateMutexA(NULL, FALSE, "ConkorsCompanion");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		handleMultipleInstances(authArg);
		return 0;
	}

	// INIT BACKEND
	std::shared_ptr<AccountManager> accMgr = std::make_shared<AccountManager>();
	std::shared_ptr<VidCore> vc = std::make_shared<VidCore>();
	std::shared_ptr<ImgCore> ic = std::make_shared<ImgCore>();
	vc->init(accMgr, queryAdapters());
	ic->init(accMgr);

	// HOTKEYS
	std::thread([&]() {
		bool screenshot = false;
		bool live = false;
		bool replay = false;

		while (1) {
			// Screenshot: ALT + A
			if (GetAsyncKeyState(VK_MENU) & 0x8000 && (GetAsyncKeyState('A') & 1) && !screenshot) {
				screenshot = true;
				printf("CK::KEY SCREENSHOT!\n");
				ic->save();
			}
			else if (GetAsyncKeyState('A') == 0) {
				screenshot = false;
			}

			// Record Live: ALT + W
			if (GetAsyncKeyState(VK_MENU) & 0x8000 && (GetAsyncKeyState('W') & 1) && !live) {
				live = true;
				printf("CK::KEY LIVE!\n");
				vc->toggleRecordLive();
			}
			else if (GetAsyncKeyState('W') == 0) {
				live = false;
			}

			// Save Replay ALT + D
			if (GetAsyncKeyState(VK_MENU) & 0x8000 && (GetAsyncKeyState('D') & 1) && !replay) {
				replay = true;
				printf("CK::KEY REPLAY!\n");
				vc->saveReplay();
			}
			else if (GetAsyncKeyState('D') == 0) {
				replay = false;
			}
		}
	}).detach();

	// START REPLAY BUFFER
	vc->recordReplay();

	// SETTINGS
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QApplication::setOrganizationName("Conkors");
	QApplication::setApplicationName("Companion");

  QApplication a(argc, argv);
	ConkorsCompanion w(accMgr, vc, ic, authArg);
  w.show();

  return a.exec();
}
