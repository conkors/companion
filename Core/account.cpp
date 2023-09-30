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

#include "account.h"
#include "webapi.h"
#include <ShlObj.h>
#include <KnownFolders.h>

AccountManager::AccountManager() {
	/// Authentication
	session_ = "";

	/// Folder Configs
	initFolders();
}
AccountManager::~AccountManager() {
}

void AccountManager::initFolders() {
	PWSTR picturesFolderPath = nullptr;
	PWSTR videosFolderPath = nullptr;
	HRESULT resPic = SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &picturesFolderPath);
	HRESULT resVid = SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &videosFolderPath);

	std::wstring picturesPathWstr(picturesFolderPath);
	std::string picturesPath(picturesPathWstr.begin(), picturesPathWstr.end());
	std::wstring videosPathWstr(videosFolderPath);
	std::string videosPath(videosPathWstr.begin(), videosPathWstr.end());

	screenshotDir_ = picturesPath + "\\Conkors\\";
	videoDir_ = videosPath + "\\Conkors\\";

	printf("CK::ACM Set Screenshot DIR: %s\n", screenshotDir_.c_str());
	printf("CK::ACM Set Video DIR: %s\n", videoDir_.c_str());

	// Screenshots
	if (!createFolderIfNotExists(screenshotDir_)) {
		printf("CK::ACM [FATAL] Unable to set screenshot directory! %s\n", screenshotDir_.c_str());
		throw "Failed to create screenshot directory!";
	}
	// Videos
	if (!createFolderIfNotExists(videoDir_)) {
		printf("CK::ACM [FATAL] Unable to set video directory! %s\n", videoDir_.c_str());
		throw "Failed to create video directory!";
	}
}

bool AccountManager::createFolderIfNotExists(const std::string& folderPath) {
	// Check if folder already exists
	DWORD fileAttributes = GetFileAttributesA(folderPath.c_str());
	if (fileAttributes != INVALID_FILE_ATTRIBUTES && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return true;
	}

	// Create the folder
	if (CreateDirectoryA(folderPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
		return true;
	}

	// Failed to create the folder
	return false;
}

std::string AccountManager::getScreenshotDir() {
	return screenshotDir_;
}

std::string AccountManager::getVideoDir() {
	return videoDir_;
}

void AccountManager::setSessionToken(std::string token) {
	printf("CK::GOT NEW SESSION\n");
	session_ = token;
}

void AccountManager::deleteSessionToken() {
	printf("CK::DELETE SESSION\n");
	session_.clear();
}

bool AccountManager::isLoggedIn() {
	return !session_.empty();
}

void AccountManager::uploadMedia(std::string filePath, bool isVid) {
	if (session_ == "") {
		printf("CK::ACM No active account session, skipping file upload!\n");
		return;
	}

	// Get Upload URL
	webapi::UploadResult res = webapi::getSignedUploadURL(session_, isVid);
	// Upload
	if (res.url == "") {
		printf("CK::ACM Failed to get a valid upload URL!!\n");
		return;
	}

	// Audio Notification
	auto cb = [this]() {
		this->playUploaded();
	};
	webapi::uploadAsset(res.id, res.url, filePath, session_, cb);
}

void AccountManager::attachUploadedSfx(std::function<void()> func) {
	uploadedSfx_ = func;
}

void AccountManager::attachStartLiveSfx(std::function<void()> func) {
	startLiveSfx_ = func;
}

void AccountManager::attachStopLiveSfx(std::function<void()> func) {
	stopLiveSfx_ = func;
}

void AccountManager::attachSaveReplaySfx(std::function<void()> func) {
	saveReplaySfx_ = func;
}

void AccountManager::playUploaded() {
	if (!uploadedSfx_) return;
	uploadedSfx_();
}

void AccountManager::playStartLive() {
	if (!startLiveSfx_) return;
	startLiveSfx_();
}

void AccountManager::playStopLive() {
	if (!stopLiveSfx_) return;
	stopLiveSfx_();
}

void AccountManager::playSaveReplay() {
	if (!saveReplaySfx_) return;
	saveReplaySfx_();
}
