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

#define NOMINMAX // WINDOWS SUX???

#include "vid.h"
#include "webapi.h"
#include <chrono>
#include <thread>
#include <algorithm>

static void log_props(obs_source_t* src) {
	if (!src) return;
	obs_properties_t* props = obs_source_properties(src);
	if (!props) return;

	blog(LOG_INFO, "\n\n====== Properties For [%s] ======", obs_source_get_name(src));
	obs_property_t* item = obs_properties_first(props);
	while (item) {
		if (obs_property_get_type(item) == OBS_PROPERTY_LIST) {
			blog(LOG_INFO, "List - [Name: %s][Type: %d][Format: %d]:", obs_property_name(item), obs_property_list_type(item), obs_property_list_format(item));
			for (int i = 0; i < obs_property_list_item_count(item); i++) {
				blog(LOG_INFO, "\tName: %s", obs_property_list_item_name(item, i));
			}
		}
		else {
			blog(LOG_INFO, "Name: %s | Type: %d", obs_property_name(item), obs_property_get_type(item));
		}
		obs_property_next(&item);
	}
	blog(LOG_INFO, "====================================\n\n");
}

static void SIGSaved(void* data, calldata_t* params) {
	VidCore* core = (VidCore*)data;
	std::string filePath = core->getLastReplay();
	printf("CK::VID NEW REPLAY SAVED!: %s\n", filePath.c_str());
	core->uploadVideo(filePath);
}

static void SIGSavedLive(void* data, calldata_t* params) {
	VidCore* core = (VidCore*)data;
	std::string filePath = core->getLastLive();
	printf("CK::VID NEW VIDEO SAVED!: %s\n", filePath.c_str());
	core->uploadVideo(filePath);
}

VidCore::VidCore() {
	isReplayBufferActive_ = false;
	isLiveActive_ = false;
	captureWindowMode_ = false;

	lastRecordingLive_ = "";
}

VidCore::~VidCore() {}

bool VidCore::resetAudio() {
	struct obs_audio_info2 ai = {};
	ai.samples_per_sec = 48000;
	ai.speakers = SPEAKERS_STEREO;
	if (!obs_reset_audio2(&ai)) {
		printf("CK::VID Failed to reset audio!!!\n");
		return false;
	}
	obs_set_audio_monitoring_device("Default", "default");
	return true;
}

bool VidCore::resetVideo(int width, int height) {
	// Cap Dimensions - Maintain Aspect Ratio
	int newWidth = width;
	int newHeight = height;
	if (width > 1280 || height > 720) {
		double aspectRatio = static_cast<double>(width) / height;
		newWidth = std::min(width, 1280);
		newHeight = static_cast<int>(newWidth / aspectRatio);
		if (newHeight > 720) {
			newHeight = 720;
			newWidth = static_cast<int>(newHeight * aspectRatio);
		}
	}
	
	ovi_.graphics_module = "libobs-d3d11.dll";
	ovi_.fps_num = 60;
	ovi_.fps_den = 1;
	ovi_.base_width = width;
	ovi_.base_height = height;
	ovi_.output_width = newWidth;
	ovi_.output_height = newHeight;
	ovi_.output_format = VIDEO_FORMAT_NV12;
	ovi_.colorspace = VIDEO_CS_709;
	ovi_.range = VIDEO_RANGE_PARTIAL;
	ovi_.gpu_conversion = true;
	ovi_.scale_type = OBS_SCALE_LANCZOS;
	int ret = obs_reset_video(&ovi_);

	if (ret != OBS_VIDEO_SUCCESS) {
		printf("CK::VID Failed to reset video!!!\n");
		return false;
	}
	obs_set_video_levels(300.000000, 1000.00000);
	return true;
}

VidCore::AdapterType VidCore::getAdapterType(int idx) {
	// Essentially guessing the vendor based on keywords in name
	std::string nvidiaKeys[] = { "nvidia", "geforce", "gtx", "rtx" };
	std::string amdKeys[] = { "amd", "radeon", "rx", "ati" };
	std::string intelKeys[] = { "intel", "arc" };

	std::string name = availableAdapters_[idx];
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);

	for (const std::string& keyword : nvidiaKeys) {
		if (name.find(keyword) != std::string::npos) {
			return NVIDIA;
		}
	}
	for (const std::string& keyword : amdKeys) {
		if (name.find(keyword) != std::string::npos) {
			return AMD;
		}
	}
	for (const std::string& keyword : intelKeys) {
		if (name.find(keyword) != std::string::npos) {
			return INTEL;
		}
	}

	printf("CK::VID Unable to determine type!\n");
	return UNKNOWN;
}

void VidCore::detectVideoEncoder() {
	bool jimNvenc = false;
	bool ffmpegNvenc = false;
	bool amf = false;
	bool qsv = false;

	// Enumerate available encoders
	size_t idx = 0;
	const char* id;
	while (obs_enum_encoder_types(idx++, &id)) {
		printf("CK::VID [ENUM_ENCODERS] - %s\n", id);
		if (strcmp(id, "ffmpeg_nvenc") == 0) {
			ffmpegNvenc = true;
		} else if (strcmp(id, "jim_nvenc") == 0) {
			jimNvenc = true;
		} else if (strcmp(id, "obs_qsv11_v2") == 0) {
			qsv = true;
		} else if (strcmp(id, "h264_texture_amf") == 0) {
			amf = true;
		}
	}

	// Identify Adapter Type & Set Encoder Accordingly
	AdapterType at = getAdapterType(ovi_.adapter);
	std::string selectedEncoder;
	switch (at) {
	case NVIDIA:
		if (jimNvenc) {
			selectedEncoder = "jim_nvenc";
		}
		else if (ffmpegNvenc) {
			selectedEncoder = "ffmpeg_nvenc";
		}
		break;
	case AMD:
		if (amf) {
			selectedEncoder = "h264_texture_amf";
		}
		break;
	case INTEL:
		if (qsv) {
			selectedEncoder = "obs_qsv11_v2";
		}
		break;
	default:
		selectedEncoder = "obs_x264";
	}

	if (selectedEncoder.empty()) {
		encoderString_ = "obs_x264";
		printf("CK::VID Encoder fallback to software\n");
	}
	else {
		encoderString_ = selectedEncoder;
	}

	printf("CK::VID [ENCODER] chose %s with card type %d\n", encoderString_.c_str(), at);
}

bool VidCore::loadOBS() {
	if (!obs_startup("en-US", nullptr, nullptr)) {
		printf("CK::VID OBS STARTUP FAILED!\n");
		return false;
	}

	resetAudio();

	// Effects Files
	CHAR szFileName[MAX_PATH];
	GetModuleFileNameA(NULL, szFileName, MAX_PATH);
	std::string path = std::string(szFileName);
	std::replace(path.begin(), path.end(), '\\', '/');
	std::size_t lastSlashIndex = path.find_last_of('/');
	if (lastSlashIndex != std::string::npos) {
		path.erase(lastSlashIndex + 1);
	}
	path += "effects/";
	obs_add_data_path(path.c_str());

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	ovi_.adapter = 0; // Default to first graphics card
	resetVideo(screenWidth, screenHeight);

	// Load Modules
	obs_module_t* loaded;
	if (obs_open_module(&loaded, "obs-ffmpeg", nullptr) == MODULE_SUCCESS) {
		obs_init_module(loaded);
	}
	if (obs_open_module(&loaded, "obs-x264", nullptr) == MODULE_SUCCESS) {
		obs_init_module(loaded);
	}
	if (obs_open_module(&loaded, "obs-qsv11", nullptr) == MODULE_SUCCESS) {
		obs_init_module(loaded);
	}
	if (obs_open_module(&loaded, "win-capture", nullptr) == MODULE_SUCCESS) {
		obs_init_module(loaded);
	}
	if (obs_open_module(&loaded, "win-wasapi", nullptr) == MODULE_SUCCESS) {
		obs_init_module(loaded);
	}

	detectVideoEncoder();

	return true;
}

void VidCore::configureBuffer() {
	OBSDataAutoRelease settings = obs_data_create();

	std::string videoDir = acm_->getVideoDir();
	obs_data_set_string(settings, "directory", videoDir.c_str());
	obs_data_set_string(settings, "format", "CKREPLAY_%CCYY-%MM-%DD_%hh-%mm-%ss");
	obs_data_set_string(settings, "extension", "mp4");
	obs_data_set_bool(settings, "allow_spaces", false);
	obs_data_set_int(settings, "max_time_sec", 30);
	obs_data_set_int(settings, "max_size_mb", 512);

	obs_output_update(replayBuffer_, settings);
}

void VidCore::configureLive() {
	OBSDataAutoRelease settings = obs_data_create();

	std::string dir = acm_->getVideoDir();
	std::string timestamp = webapi::getTimestamp();
	std::string extension = ".mp4";

	lastRecordingLive_ = dir + "CKVID_" + timestamp + extension;

	obs_data_set_string(settings, "path", lastRecordingLive_.c_str());
	obs_output_update(fileOutput_, settings);
}

void VidCore::addSources() {
	for (int i = 0; i < MAX_CHANNELS; i++)
		obs_set_output_source(i, nullptr);

	// Display Capture
	if (captureWindowMode_) {
		captureWindow();
	} else {
		captureMonitor();
	}

	// Audio Out
	sourceAud_ = obs_source_create("wasapi_output_capture", "Desktop Audio", NULL, nullptr);
	obs_set_output_source(3, sourceAud_);

	// Audio In
	sourceMic_ = obs_source_create("wasapi_input_capture", "Mic/Aux", NULL, nullptr);
	obs_set_output_source(4, sourceMic_);

#ifdef _DEBUG
	log_props(disp_);
	log_props(sourceAud_);
	log_props(sourceMic_);
#endif
}

void VidCore::addOutputs() {
	// Replay Buffer
	replayBuffer_ = obs_output_create("replay_buffer",
		"Replay Buffer",
		nullptr, nullptr);
	signal_handler_t* rbSignal =
		obs_output_get_signal_handler(replayBuffer_);

	replayBufferSaved_.Connect(rbSignal, "saved",
		SIGSaved, (void*)this);

	// File Output (ffmpeg_muxer)
	fileOutput_ = obs_output_create(
		"ffmpeg_muxer", "simple_file_output", nullptr, nullptr);
	if (!fileOutput_)
		throw "Failed to create recording output (simple output)";

	signal_handler_t* liveSignal =
		obs_output_get_signal_handler(fileOutput_);

	liveVideoSaved_.Connect(liveSignal, "stop",
		SIGSavedLive, (void*)this);
}

void VidCore::startEncoders() {
	videoRecording_ = obs_video_encoder_create(
		encoderString_.c_str(), "simple_video_recording", nullptr, nullptr);
	if (!videoRecording_) {
		printf("CK::FATAL: Failed to create video encoder!!!\n");
	}
	aacRecording_ = obs_audio_encoder_create("ffmpeg_aac", "simple_aac_recording", nullptr, 0, nullptr);
	if (!aacRecording_) {
		printf("CK::FATAL: Failed to create audio encoder!!!\n");
	}

	// Video Encoder Settings
	OBSDataAutoRelease vsettings = obs_data_create();
	obs_data_set_string(vsettings, "rate_control", "CBR");
	obs_data_set_string(vsettings, "profile", "high");
	obs_data_set_int(vsettings, "bitrate", 3000);
	obs_encoder_update(videoRecording_, vsettings);

	// Audio Encorder Settings
	OBSDataAutoRelease asettings = obs_data_create();
	obs_data_set_int(asettings, "bitrate", 128);
	obs_data_set_string(asettings, "rate_control", "CBR");
	obs_encoder_update(aacRecording_, asettings);

	// Attach Encoders
	obs_encoder_set_video(videoRecording_, obs_get_video());
	obs_encoder_set_audio(aacRecording_, obs_get_audio());

	obs_output_set_video_encoder(fileOutput_, videoRecording_);
	obs_output_set_audio_encoder(fileOutput_, aacRecording_, 0);

	obs_output_set_video_encoder(replayBuffer_, videoRecording_);
	obs_output_set_audio_encoder(replayBuffer_, aacRecording_, 0);
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

bool VidCore::init(std::shared_ptr<AccountManager> acm, const std::vector<std::string>& adapters) {
	if (!acm) {
		return false;
	}
	acm_ = acm;
	availableAdapters_ = adapters;

	if (!loadOBS()) {
		return false;
	}
	addSources();
	addOutputs();
	startEncoders();
	return true;
}

bool VidCore::overrideAdapter(int idx) {
	if (idx == ovi_.adapter) return false;

	// Stop outputs
	stopReplay();
	saveLive();
	// Release outputs
	replayBuffer_ = nullptr;
	fileOutput_ = nullptr;
	// Release sources
	disp_ = nullptr;
	sourceMic_ = nullptr;
	sourceAud_ = nullptr;

	ovi_.adapter = idx;

	// Reset video
	int ret = obs_reset_video(&ovi_);
	if (ret != OBS_VIDEO_SUCCESS) {
		printf("CK::VID Failed to override graphics card!!!\n");
	}
	
	// Re-add sources
	addSources();
	// Re-add outputs
	addOutputs();
	// Restart encoders
	startEncoders();
	
	// Restart replay buffer
	recordReplay();

	return ret == OBS_VIDEO_SUCCESS;
}

void VidCore::forceSoftwareEncoder() {
	// Stop the outputs
	stopReplay();
	saveLive();
	// Release the outputs
	replayBuffer_ = nullptr;
	fileOutput_ = nullptr;
	// Release sources
	disp_ = nullptr;
	sourceMic_ = nullptr;
	sourceAud_ = nullptr;

	// Force
	encoderString_ = "obs_x264";

	// Re-add sources
	addSources();
	// Re-add outputs
	addOutputs();
	// Restart encoders
	startEncoders();
	
	// Restart replay buffer
	recordReplay();
}

propListInt VidCore::getAdapters() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	propListInt list;
	int i = 0;
	for (auto& name : availableAdapters_) {
		list.push_back(std::make_pair(name, i++));
	}
	return list;
}

propListStr VidCore::getDisplayOpts() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	propListStr list;

	const char* name = obs_source_get_name(disp_);
	int isDisplayCapture = std::strcmp(name, "Display Capture") == 0;

	obs_properties_t* props = obs_source_properties(disp_);
	obs_property_t* monitors = obs_properties_get(props, isDisplayCapture ? "monitor_id" : "window");
	for (int i = 0; i < obs_property_list_item_count(monitors); i++) {
		std::string name = std::string(obs_property_list_item_name(monitors, i));
		std::string handle = std::string(obs_property_list_item_string(monitors, i));
		list.push_back(std::make_pair(name, handle));
	}
	return list;
}

propListStr VidCore::getSpeakerOpts() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	propListStr list;

	obs_properties_t* props = obs_source_properties(sourceAud_);
	obs_property_t* speakers = obs_properties_get(props, "device_id");
	for (int i = 0; i < obs_property_list_item_count(speakers); i++) {
		std::string name = std::string(obs_property_list_item_name(speakers, i));
		std::string handle = std::string(obs_property_list_item_string(speakers, i));
		list.push_back(std::make_pair(name, handle));
	}
	return list;
}

propListStr VidCore::getMicOpts() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	propListStr list;

	obs_properties_t* props = obs_source_properties(sourceMic_);
	obs_property_t* mics = obs_properties_get(props, "device_id");
	for (int i = 0; i < obs_property_list_item_count(mics); i++) {
		std::string name = std::string(obs_property_list_item_name(mics, i));
		std::string handle = std::string(obs_property_list_item_string(mics, i));
		list.push_back(std::make_pair(name, handle));
	}
	return list;
}

void VidCore::updateDisplay(const char* id) {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	const char* name = obs_source_get_name(disp_);
	int isDisplayCapture = std::strcmp(name, "Display Capture") == 0;

	OBSDataAutoRelease settings = obs_source_get_settings(disp_);
	obs_data_set_string(settings, isDisplayCapture ? "monitor_id" : "window", id);
	obs_source_update(disp_, settings);

	// FIXME: WAIT FOR DIMENSIONS TO PROPOGATE?
	std::this_thread::sleep_for(std::chrono::seconds(2));

	int width = obs_source_get_width(disp_);
	int height = obs_source_get_height(disp_);
	resetVideo(width, height);
}

void VidCore::updateSpeaker(const char* id){
	const std::lock_guard<std::mutex> lock(vidMtx_);

	OBSDataAutoRelease settings = obs_source_get_settings(sourceAud_);
	obs_data_set_string(settings, "device_id", id);
	obs_source_update(sourceAud_, settings);
}

void VidCore::updateMic(const char* id){
	const std::lock_guard<std::mutex> lock(vidMtx_);

	OBSDataAutoRelease settings = obs_source_get_settings(sourceMic_);
	obs_data_set_string(settings, "device_id", id);
	obs_source_update(sourceMic_, settings);
}

void VidCore::toggleSpeakerAudio(bool mute) {
	const std::lock_guard<std::mutex> lock(vidMtx_);
	if (!mute) {
		obs_source_set_volume(sourceAud_, 1.0);
	} else {
		obs_source_set_volume(sourceAud_, 0.0);
	}
}

void VidCore::toggleMicAudio(bool mute) {
	const std::lock_guard<std::mutex> lock(vidMtx_);
	if (!mute) {
		obs_source_set_volume(sourceMic_, 1.0);
	} else {
		obs_source_set_volume(sourceMic_, 0.0);
	}
}

void VidCore::captureWindow() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	if (disp_ && captureWindowMode_) {
		printf("CK::VID Already capturing window\n");
		return;
	}

	// Window Capture
	disp_ = obs_source_create("window_capture", "Window Capture", NULL, nullptr);
	obs_set_output_source(1, disp_);

	initializeWindowCapture();

	// FIXME: WAIT FOR DIMENSIONS TO PROPOGATE?
	std::this_thread::sleep_for(std::chrono::seconds(2));

	int width = obs_source_get_width(disp_);
	int height = obs_source_get_height(disp_);
	resetVideo(width, height);

	captureWindowMode_ = true;
}

void VidCore::captureMonitor() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	if (disp_ && !captureWindowMode_) {
		printf("CK::VID Already capturing monitor\n");
		return;
	}

	// Display Capture
	disp_ = obs_source_create("monitor_capture", "Display Capture", NULL, nullptr);
	obs_set_output_source(1, disp_);
	
	initializeDisplayCapture();

	// FIXME: WAIT FOR DIMENSIONS TO PROPOGATE?
	std::this_thread::sleep_for(std::chrono::seconds(2));

	int width = obs_source_get_width(disp_);
	int height = obs_source_get_height(disp_);
	resetVideo(width, height);

	captureWindowMode_ = false;
}

void VidCore::initializeDisplayCapture() {
	// Using props, select the first monitor available
	obs_properties_t* props = obs_source_properties(disp_);
	obs_property_t* monitors = obs_properties_get(props, "monitor_id");
	const char* devId = obs_property_list_item_string(monitors, 0);

	OBSDataAutoRelease settings = obs_source_get_settings(disp_);
	obs_data_set_int(settings, "method", 0); // AUTO
	obs_data_set_string(settings, "monitor_id", devId);
	obs_source_update(disp_, settings);
}

void VidCore::initializeWindowCapture() {
	// Using props, select the first window available
	obs_properties_t* props = obs_source_properties(disp_);
	obs_property_t* windows = obs_properties_get(props, "window");
	const char* devId = obs_property_list_item_string(windows, 0);

	OBSDataAutoRelease settings = obs_source_get_settings(disp_);
	obs_data_set_int(settings, "method", 2); // WGC
	obs_data_set_string(settings, "window", devId);
	obs_data_set_bool(settings, "client_area", false);
	obs_source_update(disp_, settings);
}

bool VidCore::recordReplay() {
	const std::lock_guard<std::mutex> lock(vidMtx_);
	if (isReplayBufferActive_) { return false; } // Already Started

	configureBuffer();

	if (!obs_output_start(replayBuffer_)) {
		printf("CK::VID Unable to start replay buffer!\n");
		return false;
	}

	isReplayBufferActive_ = true;
	return true;
}

void VidCore::stopReplay() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	if (!isReplayBufferActive_) { return; }

	obs_output_stop(replayBuffer_);
	isReplayBufferActive_ = false;
}

void VidCore::saveReplay() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	if (!isReplayBufferActive_) { return; } // Replay Buffer Inactive

	calldata_t cd = { 0 };
	proc_handler_t* ph =
		obs_output_get_proc_handler(replayBuffer_);
	proc_handler_call(ph, "save", &cd);
	calldata_free(&cd);

	acm_->playSaveReplay();
}

void VidCore::toggleRecordLive() {
	const std::lock_guard<std::mutex> lock(vidMtx_);

	if (isLiveActive_) {
		saveLive();
	}
	else {
		recordLive();
	}
}

bool VidCore::recordLive() {
	if (isLiveActive_) { return false; } // Already Started

	configureLive();

	if (!obs_output_start(fileOutput_)) {
		printf("CK::VID Unable to start live recording!\n");
		return false;
	}

	// 30s Max
	std::thread([this]() {
		std::this_thread::sleep_for(std::chrono::seconds(30));
		if (isLiveActive_) {
			saveLive();
		}
	}).detach();

	isLiveActive_ = true;
	acm_->playStartLive();
	return true;
}

void VidCore::saveLive() {
	if (!isLiveActive_) { return; } // Not recording

	obs_output_stop(fileOutput_);

	isLiveActive_ = false;
	acm_->playStopLive();
}

std::string VidCore::getLastLive() {
	return lastRecordingLive_;
}

std::string VidCore::getLastReplay() {
	calldata_t cd = { 0 };
	proc_handler_t* ph =
		obs_output_get_proc_handler(replayBuffer_);

	proc_handler_call(ph, "get_last_replay", &cd);
	const char* path = calldata_string(&cd, "path");
	std::string strPath = std::string(path);
	calldata_free(&cd);
	return strPath;
}

void VidCore::uploadVideo(std::string filePath) {
	acm_->uploadMedia(filePath, true);
}

