#include <curl/curl.h>
#include <iostream>

struct OllamaPromptConfig {
    std::string model;
    std::string prompt;
    bool stream;
};

class OllamaClient {
  private:
    CURL* curl;
    std::string url;
    struct curl_slist* headers;

    void check_ollama();
    void init_curl();
    void process_response(std::string response);
    void process_response_stream(std::string response);

  public:
    OllamaClient();

    void curl_request(OllamaPromptConfig config);
};
