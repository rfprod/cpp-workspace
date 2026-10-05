#include "ollama_client.hpp"
#include <algorithm>
#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using nlohmann::json_abi_v3_11_2::json;

static size_t WriteCallback(char* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(contents, size * nmemb);
    return size * nmemb;
}

// Create an instance of OllamaClient.
OllamaClient::OllamaClient() {
    this->url = "http://localhost:11434/api/generate";
    this->headers = nullptr;

    this->check_ollama();

    this->init_curl();

    printf("\n");
}

// Checks whether Ollama binary exists on the file system.
void OllamaClient::check_ollama() {
    if (system("which ollama > /dev/null 2>&1")) {
        fprintf(stderr, "Ollama binary does not exist.\nInstallation instructions: "
                        "https://docs.ollama.com/linux\n");
        exit(0);
    } else {
        printf("✅ Ollama binary exists.\n");
    }
};

// Initialize CURL.
void OllamaClient::init_curl() {
    this->curl = curl_easy_init();
    if (!this->curl) {
        fprintf(stderr, "Failed to initialize CURL.\n");
        exit(1);
    } else {
        printf("✅ CURL initialized.\n");
    }
}

// Trims leading whitespace and trailing whitespace from a json response.
std::string OllamaClient::trim_response(std::string response) {
    response.erase(response.begin(),
                   std::find_if(response.begin(), response.end(),
                                [](unsigned char ch) { return !std::isspace(ch); }));

    response.erase(std::find_if(response.rbegin(), response.rend(),
                                [](unsigned char ch) { return !std::isspace(ch); })
                       .base(),
                   response.end());
    return response;
}

// Process a response.
void OllamaClient::process_response(std::string response) {
    try {
        json response_json = json::parse(response);

        std::cout << "🦙 llama:\n"
                  << this->trim_response(response_json["response"].get<std::string>()) << "\n---\n"
                  << std::endl;
    } catch (const json::exception& err) {
        fprintf(stderr, "JSON parse error: %s\n", err.what());
    }
}

// Process a streaming response.
void OllamaClient::process_response_stream(std::string response) {
    std::istringstream stream(response);
    std::string line;

    std::cout << "Response: ";
    std::cout.flush();

    while (std::getline(stream, line)) {
        if (!line.empty()) {
            try {
                json chunk = json::parse(line);
                std::string text = chunk["response"].get<std::string>();
                std::cout << text;
                std::cout.flush();

                if (chunk["done"].get<bool>()) {
                    break;
                }
            } catch (const json::exception& err) {
                fprintf(stderr, "JSON parse error: %s\n", err.what());
            }
        }
    }

    std::cout << std::endl;
}

// Send a request to the Ollama endpoint using CURL.
void OllamaClient::curl_request(OllamaPromptConfig config) {
    json request_payload = {
        {"model", config.model}, {"prompt", config.prompt}, {"stream", config.stream}};

    std::string post_data = request_payload.dump();

    std::string response;

    curl_easy_setopt(this->curl, CURLOPT_URL, this->url.c_str());
    curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0L);

    this->headers = curl_slist_append(this->headers, "Content-Type: application/json");
    curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(this->headers);
        curl_easy_cleanup(this->curl);
        exit(1);
    }

    if (config.stream) {
        this->process_response_stream(response);
    } else {
        this->process_response(response);
    }

    curl_slist_free_all(this->headers);
    curl_easy_cleanup(this->curl);
}
