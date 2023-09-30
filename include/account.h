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

#include <string>
#include <functional>

class AccountManager {
public:
	explicit AccountManager();
	~AccountManager();

	std::string getScreenshotDir();
	std::string getVideoDir();

	bool isLoggedIn();

	// Upload to S3
	void uploadMedia(std::string filePath, bool isVid);

	void attachUploadedSfx(std::function<void()> func);
	void attachStartLiveSfx(std::function<void()> func);
	void attachStopLiveSfx(std::function<void()> func);
	void attachSaveReplaySfx(std::function<void()> func);

	void playUploaded();
	void playStartLive();
	void playStopLive();
	void playSaveReplay();

	/////////////////////////////////////////////////////
	// UI ACCESS
	/////////////////////////////////////////////////////

	// Log In
	void setSessionToken(std::string token);
	// Log Out
	void deleteSessionToken();

private:
	// Session Token
	std::string session_;

	// File Storage
	std::string screenshotDir_;
	std::string videoDir_;

	// Audio Notifications
	std::function<void()> uploadedSfx_;
	std::function<void()> startLiveSfx_;
	std::function<void()> stopLiveSfx_;
	std::function<void()> saveReplaySfx_;

	void initFolders();
	bool createFolderIfNotExists(const std::string& folderPath);
};

