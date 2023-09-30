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

#define CURL_STATICLIB

#include <string>
#include <thread>
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

#define AUTH_COOKIE_NAME ""
#define AUTH_COOKIE ""

#define DISCORD_BASE ""
#define DISCORD_OPTS ""

#define ENDPOINT_STAGE_CREATE ""
#define ENDPOINT_UPLOADED ""
#define ENDPOINT_LOGIN ""
#define ENDPOINT_PP ""

#ifdef _DEBUG
	#define SERVER_BASE_URL ""
	#define DISCORD_REDIRECT_URI ""
#else
	#define SERVER_BASE_URL ""
	#define DISCORD_REDIRECT_URI ""
#endif

#define DISCORD_OAUTH_LINK DISCORD_BASE DISCORD_REDIRECT_URI DISCORD_OPTS
#define URL_CREATE_STAGE SERVER_BASE_URL ENDPOINT_STAGE_CREATE
#define URL_UPLOADED SERVER_BASE_URL ENDPOINT_UPLOADED
#define URL_LOGIN SERVER_BASE_URL ENDPOINT_LOGIN
#define URL_PP SERVER_BASE_URL ENDPOINT_PP

namespace webapi {

using json = nlohmann::json;

//// GENERAL UTILITY FUNCTIONS /////
inline std::string getTimestamp()
{
	std::time_t now = std::time(nullptr);

	std::ostringstream oss;
	oss << now;

	return oss.str();
}
////////////////////////////////////

struct ResponseData {
	std::string body;
	long status;
};

struct UploadResult {
	std::string url;
	std::string id;
};

// Callback function to write received data to the response buffer
inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* buffer) {
	size_t totalSize = size * nmemb;
	buffer->append(static_cast<char*>(contents), totalSize);
	return totalSize;
}

// Function to perform the HTTP request and retrieve the response
inline ResponseData performRequest(const std::string& url, const std::string& cookie, const json& requestBody) {
	ResponseData response;
	CURL* curl = curl_easy_init();

	if (curl) {
		struct curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "charset: utf-8");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_COOKIE, cookie.c_str());

		// Set the request method to POST
		curl_easy_setopt(curl, CURLOPT_POST, 1L);

		// Set the request body
		curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, requestBody.dump().c_str());

		// Set the callback function for writing response data
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		// Set the response buffer as the user-defined data to pass to the callback function
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			printf("CK::API Failed to perform CURL request: %s\n", curl_easy_strerror(res));

		// Get the HTTP response status code
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);

		curl_easy_cleanup(curl);
	}

	return response;
}

inline UploadResult getSignedUploadURL(const std::string stoken, bool forVid) {
	std::string url = std::string(URL_CREATE_STAGE);
	std::string sessionCookie = AUTH_COOKIE_NAME + stoken;

	const std::string media_type_value = forVid ? "MOV" : "IMG";

	// Create the request body JSON object
	json requestBody = {
		{"media_type", media_type_value}
	};

	// Perform the HTTP request
	ResponseData response = performRequest(url, sessionCookie, requestBody);

	UploadResult result; // Create a struct instance to hold the result

	// Check if the request was successful (200 status code)
	if (response.status == 200) {
		// Parse the JSON response
		json j = json::parse(response.body);

		// Extract the value of the "url" field
		if (j.contains("url") && j["url"].is_string()) {
			result.url = j["url"].get<std::string>();

			// Extract the value of the "_id" field
			if (j.contains("_id") && j["_id"].is_string()) {
				result.id = j["_id"].get<std::string>();

				// Use the extracted "url" and "_id" values as needed
				printf("CK::API Got upload URL for ID: %s - %s\n", result.id.c_str(), result.url.c_str());
			}
		}
	}
	else {
		printf("CK::API Request failed with status code: %lu", response.status);
	}

	return result;
}

inline void triggerThumbnailJob(const std::string stoken, const std::string stageId) {
	std::string url = std::string(URL_UPLOADED);
	std::string sessionCookie = AUTH_COOKIE_NAME + stoken;

	// Create the request body JSON object
	json requestBody = {
		{"stage_id", stageId}
	};

	// Perform the HTTP request
	ResponseData response = performRequest(url, sessionCookie, requestBody);

	// Check if the request was successful (200 status code)
	if (response.status == 200) {
		printf("CK::API THUMBNAIL JOB INVOKED\n");
	}
	else {
		printf("CK::API FAILED TO INVOKE THUMBNAIL JOB! %d\n", response.status);
	}
}

// Perform the file upload using libcurl
inline void performFileUpload(const std::string stageId,
							const std::string preSignedUrl,
							const std::string filePath, 
							const std::string sessionToken, 
							const std::function<void()>& callback) {
	CURL* curl = curl_easy_init();
	if (!curl) {
		printf("CK::API CURL FAILED INIT!!!\n");
		return;
	}
	// Set the URL
	curl_easy_setopt(curl, CURLOPT_URL, preSignedUrl.c_str());

	// Set the upload file as the read callback data
	FILE* file;
	fopen_s(&file, filePath.c_str(), "rb");
	if (!file) {
		printf("CK::API Unable to open desired upload file!\n");
		return;
	}
	curl_easy_setopt(curl, CURLOPT_READDATA, file);

	// Set the read callback function
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, fread);
	// Set the PUT method
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	// Disable transfer encoding
	curl_easy_setopt(curl, CURLOPT_TRANSFER_ENCODING, 0L);

	// Set the Content-Length header
	fseek(file, 0L, SEEK_END);
	long sz = ftell(file);
	rewind(file);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, sz);

	// Perform the request
	CURLcode res = curl_easy_perform(curl);

	// Check the result
	if (res != CURLE_OK) {
		printf("CK::API curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}
	else {
		callback();
		printf("CK::API S3 UPLOAD SUCCESS!!!\n");
		triggerThumbnailJob(sessionToken, stageId);
	}

	// Clean up
	fclose(file);
	curl_easy_cleanup(curl);
}

inline void uploadAsset(const std::string stageId, 
						const std::string preSignedUrl, 
						const std::string filePath, 
						const std::string sessionToken, 
						const std::function<void()>& callback) {
    std::thread uploadThread(performFileUpload, stageId, preSignedUrl, filePath, sessionToken, callback);
    uploadThread.detach();
}

}
