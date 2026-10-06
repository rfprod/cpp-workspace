#include "progress_spinner.hpp"
#include <curl/curl.h>
#include <iostream>

struct OllamaPromptConfig {
    std::string model;
    std::string prompt;
    bool stream;
};

struct OllamaServerURLs {
    std::string generate_url;
    std::string tags_url;
};

class OllamaClient {
  private:
    CURL* curl;
    OllamaServerURLs urls;
    struct curl_slist* headers;
    ProgressSpinner spinner{ProgressSpinner()};

    void check_ollama();
    void init_curl();

    std::string trim_response(std::string response);

    void process_response(std::string response);
    void process_response_stream(std::string response);

  public:
    OllamaClient();

    void query_model(OllamaPromptConfig config);
    void query_model_list();
};
