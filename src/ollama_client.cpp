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
    this->urls = {
        generate_url : "http://localhost:11434/api/generate",
        tags_url : "http://localhost:11434/api/tags"
    };
    this->headers = nullptr;
    this->curl = nullptr;

    this->check_ollama();

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
        exit(1);
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
                exit(1);
            }
        }
    }

    std::cout << std::endl;
}

// Send a request to the Ollama `generate` endpoint using CURL.
void OllamaClient::query_model(OllamaPromptConfig config) {
    this->init_curl();

    printf("💬 prompt: %s\n", config.prompt.c_str());

    json request_payload = {
        {"model", config.model}, {"prompt", config.prompt}, {"stream", config.stream}};

    std::string post_data = request_payload.dump();

    std::string response;

    curl_easy_setopt(this->curl, CURLOPT_URL, this->urls.generate_url.c_str());
    curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0L);

    this->headers = curl_slist_append(this->headers, "Content-Type: application/json");
    curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);

    CURLcode result = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(this->curl);
    curl_slist_free_all(this->headers);
    this->headers = nullptr;

    if (result != CURLE_OK) {
        fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(result));
        exit(1);
    }
    if (status != 200) {
        fprintf(stderr, "Ollama returned HTTP status: %ld\n", status);
        exit(1);
    }

    if (config.stream) {
        this->process_response_stream(response);
    } else {
        this->process_response(response);
    }
}

// Send a request to the Ollama endpoint for listing local models using CURL.
void OllamaClient::query_model_list() {
    this->init_curl();

    std::string response;

    curl_easy_setopt(this->curl, CURLOPT_URL, this->urls.tags_url.c_str());
    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0L);

    this->headers = curl_slist_append(this->headers, "Content-Type: application/json");
    curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);

    CURLcode result = curl_easy_perform(this->curl);

    long status = 0;
    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(this->curl);
    curl_slist_free_all(this->headers);
    this->headers = nullptr;

    if (result != CURLE_OK) {
        fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(result));
        exit(1);
    }
    if (status != 200) {
        fprintf(stderr, "Ollama returned HTTP status: %ld\n", status);
        exit(1);
    }

    try {
        const json data = json::parse(response);

        if (!data.contains("models") || !data["models"].is_array()) {
            fprintf(stderr, "Response has no models array.\n");
            exit(1);
        }

        printf("📃 available models:\n");

        for (const auto& model : data["models"]) {
            const std::string name = model.value("name", "unknown");
            const std::string modified_at = model.value("modified_at", "unknown");
            const std::string size =
                model.contains("size")
                    ? std::to_string(model["size"].get<long long>() / 1024 / 1024) + " Mb"
                    : "size unknown";

            std::cout << modified_at << "\t (" << size << ") \t" << name << "\n";
        }
        printf("\n");
    } catch (const json::exception& err) {
        fprintf(stderr, "JSON parse error: %s\n", err.what());
        exit(1);
    }
}
