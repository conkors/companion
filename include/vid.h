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

#include <Windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <mutex>

#include <obs.h>
#include <obs.hpp>

#include "account.h"

using propListStr = std::vector<std::pair<std::string, std::string>>;
using propListInt = std::vector<std::pair<std::string, int>>;

class VidCore {
public:
	enum AdapterType {
		UNKNOWN = 0,
		NVIDIA,
		AMD,
		INTEL
	};

	explicit VidCore();
	~VidCore();

	bool init(std::shared_ptr<AccountManager> acm, const std::vector<std::string>& adapters);

	void initializeDisplayCapture();
	void initializeWindowCapture();

	/// Replay Buffer
	bool recordReplay();
	void stopReplay();

	/// Live Record
	bool recordLive();
	void saveLive();

	std::string getLastReplay();
	std::string getLastLive();
	void uploadVideo(std::string filePath);

	/////////////////////////////////////////////////////
	// UI ACCESS
	/////////////////////////////////////////////////////

	bool overrideAdapter(int idx);
	void forceSoftwareEncoder();

	void captureWindow();
	void captureMonitor();

	void updateDisplay(const char* id);
	void updateSpeaker(const char* id);
	void updateMic(const char* id);

	propListInt getAdapters();
	propListStr getDisplayOpts();
	propListStr getSpeakerOpts();
	propListStr getMicOpts();

	void toggleSpeakerAudio(bool mute);
	void toggleMicAudio(bool mute);

	void toggleRecordLive();
	void saveReplay();

private:
	std::shared_ptr<AccountManager> acm_;

	std::mutex vidMtx_;
	obs_video_info ovi_;
	std::vector<std::string> availableAdapters_;

	OBSEncoder aacRecording_;
	OBSEncoder videoRecording_;

	OBSSourceAutoRelease disp_;
	OBSSourceAutoRelease sourceMic_;
	OBSSourceAutoRelease sourceAud_;

	OBSOutputAutoRelease replayBuffer_;
	OBSOutputAutoRelease fileOutput_;

	OBSSignal replayBufferSaved_;
	OBSSignal liveVideoSaved_;

	std::string encoderString_;
	std::string lastRecordingLive_;

	bool isReplayBufferActive_; // Replay Buffer
	bool isLiveActive_; // Live Recording
	bool captureWindowMode_;

	AdapterType getAdapterType(int idx);
	bool resetAudio();
	bool resetVideo(int width, int height);
	bool loadOBS();
	void configureBuffer();
	void configureLive();
	void addSources();
	void addOutputs();
	void detectVideoEncoder();
	void startEncoders();
};