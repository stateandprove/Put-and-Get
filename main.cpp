#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

std::string token;

/* Parses a responce of /oauth2/token and extracts token from it */
size_t get_token(char* buffer, size_t itemsize, size_t nitems, void* userdata) 
{

	size_t bytes = itemsize * nitems;
	std::string tkn_to_parse = "access_token";
	std::string responseAuth(buffer, bytes);

	if (responseAuth.find(tkn_to_parse) != std::string::npos) {
		int tkn_begin = responseAuth.find(tkn_to_parse) + tkn_to_parse.length() + 4;
		int tkn_end = responseAuth.find_first_of('"', tkn_begin);
		std::string tkn = responseAuth.substr(tkn_begin, tkn_end - tkn_begin);
		token = tkn;
	}
	else {
		fprintf(stderr, "Error: Wrong authorization code!\n");
		return -1;
	}
	return bytes;
}

/* Parses and handles the "put" response */
size_t handle_put_response(char* buffer, size_t itemsize, size_t nitems, void* userdata)
{
	size_t bytes = itemsize * nitems;
	std::string responsePut(buffer, bytes);
	if (responsePut.find("content_hash") == std::string::npos) {
		fprintf(stderr, "Something went wrong. Try again.\n");
		return -1;
	}
	else {
		return bytes;
	}
}

/* Parses and handles the "get" response */
size_t handle_get_response(char* buffer, size_t itemsize, size_t nitems, FILE* stream)
{
	size_t bytes = itemsize * nitems;
	size_t data_written;
	std::string responseGet(buffer, bytes);
	if (responseGet.find("{\".tag\": \"not_found\"}}}") != responseGet.find("{\".tag\": \"malformed_path\"}}}")
		or responseGet.find("could not decode input as JSON") != std::string::npos) 
	{
		printf("Error: Not Found or Malformed Source File Path!\n");
		return 0;
	}
	else {
		data_written = fwrite(buffer, itemsize, nitems, stream);
		return data_written;
	}
}

int main(int argc, char* argv[])
{

	/* Implementing authorization flow */
	while (1) {

		/* The authorization code from dropbox.com is required */
		std::string auth_code;
		printf("Enter the code from Dropbox:\n");
		std::getline(std::cin, auth_code);
		std::string auth_post_data = "code=" + auth_code
			+ "&grant_type=authorization_code&client_id=<APP_KEY>&client_secret=<APP_SECRET>";

		/* Performing the OAuth2 token request */
		CURL* curlAuth;
		CURLcode resAuth;
		curl_global_init(CURL_GLOBAL_ALL);
		curlAuth = curl_easy_init();

		if (curlAuth) {

			/* Specifying the OAuth2 POST data */
			curl_easy_setopt(curlAuth, CURLOPT_URL, "https://api.dropboxapi.com/oauth2/token");
			curl_easy_setopt(curlAuth, CURLOPT_POSTFIELDS, auth_post_data.c_str());
			curl_easy_setopt(curlAuth, CURLOPT_WRITEFUNCTION, get_token);

			/* Performing an authorization request */
			resAuth = curl_easy_perform(curlAuth);

			if (resAuth != CURLE_OK) {
				continue;
			}
			else {
				printf("Successfully Authorized\nCommands available:\n--> put\n--> get\n--> exit\n");
				break;
			}

			curl_easy_cleanup(curlAuth);

		}
	}

	std::string authHeader = "Authorization: Bearer " + token;

	while (1) {

		/* Implementing the command flow */
		printf("Enter the command:\n");
		std::string command;
		std::getline(std::cin, command);

		/* Implementing the "put" command */
		if (command == "put") {

			FILE* putFile;
			printf("Specify the source file path (project directory by default):\n");
			std::string putSrcPath;
			std::getline(std::cin, putSrcPath);
			if (putSrcPath.length() == 0 or putSrcPath.find(".") == std::string::npos) {
				fprintf(stderr, "Error: Wrong source file path format\n");
				continue;
			}
			putFile = fopen(putSrcPath.c_str(), "rb");
			if (putFile == NULL) {
				fprintf(stderr, "Error: Wrong source file path\n");
				continue;
			}

			printf("Specify the destination path (root by default):\n");
			std::string putDstPath;
			std::getline(std::cin, putDstPath);
			if (putDstPath.length() == 0) {
				fprintf(stderr, "Error: Empty destination path\n");
				continue;
			}
			std::string putPathHeader = "Dropbox-API-Arg: {\"path\":\"/" + putDstPath + "\"}";
				
			/* Loading the source file into the memory*/
			long putFileSize;
			char* putBuffer;
			size_t resultingPutFileSize;
			fseek(putFile, 0, SEEK_END);
			putFileSize = ftell(putFile);
			rewind(putFile);
			putBuffer = new char[putFileSize];
			resultingPutFileSize = fread(putBuffer, putFileSize, 1, putFile);
			fclose(putFile);

			CURL* curlPut;
			CURLcode resPut;
			curlPut = curl_easy_init();

			if (curlPut) {

				/* Specifying the "put" request headers */
				struct curl_slist* headers = NULL;
				headers = curl_slist_append(headers, authHeader.c_str());
				headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
				headers = curl_slist_append(headers, putPathHeader.c_str());

				/* Specifying the "put" request data */
				curl_easy_setopt(curlPut, CURLOPT_HTTPHEADER, headers);
				curl_easy_setopt(curlPut, CURLOPT_URL, "https://content.dropboxapi.com/2/files/upload");
				curl_easy_setopt(curlPut, CURLOPT_WRITEFUNCTION, handle_put_response);
				curl_easy_setopt(curlPut, CURLOPT_POSTFIELDS, putBuffer);
				curl_easy_setopt(curlPut, CURLOPT_POSTFIELDSIZE, putFileSize);

				/* Performing the "put" request */
				resPut = curl_easy_perform(curlPut);

				if (resPut != CURLE_OK) {
					continue;
				}
				else {
					printf("Done!\n");
				}

				delete[] putBuffer;
				curl_easy_cleanup(curlPut);
				continue;

			}

		}

		/* Implementing the "get" command */
		if (command == "get") {

			printf("Specify the source path (root by default):\n");
			std::string getSrcPath;
			std::getline(std::cin, getSrcPath);
			if (getSrcPath.length() == 0) {
				fprintf(stderr, "Error: Empty source file path\n");
				continue;
			}
			std::string getPathHeader = "Dropbox-API-Arg: {\"path\":\"/" + getSrcPath + "\"}";

			printf("Specify the destination path (project file directory by default):\n");
			std::string getDstPath;
			std::getline(std::cin, getDstPath);
			if (getDstPath.find(".") == std::string::npos) {
				fprintf(stderr, "Error: Wrong destination path format\n");
				continue;
			}

			FILE* getFile;
			getFile = fopen(getDstPath.c_str(), "wb");

			CURL* curlGet;
			CURLcode resGet;
			curlGet = curl_easy_init();

			if (curlGet) {

				/* Specifying the "get" request headers */
				struct curl_slist* headers = NULL; /* init to NULL is important */
				headers = curl_slist_append(headers, authHeader.c_str());
				headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
				headers = curl_slist_append(headers, getPathHeader.c_str());

				/* Specifying the "get" request data */
				curl_easy_setopt(curlGet, CURLOPT_HTTPHEADER, headers);
				curl_easy_setopt(curlGet, CURLOPT_URL, "https://content.dropboxapi.com/2/files/download");
				curl_easy_setopt(curlGet, CURLOPT_POSTFIELDS, "");
				curl_easy_setopt(curlGet, CURLOPT_WRITEFUNCTION, handle_get_response);
				curl_easy_setopt(curlGet, CURLOPT_WRITEDATA, getFile);

				/* Performing the "get" request */
				resGet = curl_easy_perform(curlGet);

				if (resGet != CURLE_OK) {
					continue;
				}
				else {
					printf("Done!\n");
				}

				fclose(getFile);
				curl_easy_cleanup(curlGet);
				continue;
			}

		}

		if (command == "exit") {
			break;
		}
		else {
			printf("Wrong command! Commands available: put, get, exit\n");
			continue;
		}

	}

	curl_global_cleanup();
	return 0;
	
}
