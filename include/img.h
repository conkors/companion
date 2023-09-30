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

#pragma once

#define WM_HIDE_OVERLAY (WM_USER + 1)

#include <Windows.h>
#include "gdiplus.h"
#include "atlimage.h"

#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <ctime>

#include "account.h"

class ImgCore {
public:
	explicit ImgCore();
	~ImgCore();

	bool init(std::shared_ptr<AccountManager> acm);
	void uploadImg(std::string filePath);

	/////////////////////////////////////////////////////
	// UI ACCESS
	/////////////////////////////////////////////////////

	void save();

private:
	std::shared_ptr<AccountManager> acm_;

	// Overlay State; True = Showing
	bool captureState_;
	
	ULONG_PTR gdiToken_;
	Gdiplus::GdiplusStartupInput tmp_;

	// Window used for screenshot cropping
	HWND shotWnd_;
	// Rect containing desired cropping info
	RECT selection_;

	bool savePNG(HBITMAP hbitmap, std::vector<BYTE>& data);
	void overlay(const HBITMAP& bmap);
};
