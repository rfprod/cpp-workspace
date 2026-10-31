#include "interface/user_interface.hpp"
#include "ollama/ollama_client.hpp"
#include "storage/qdrant_client.hpp"
#include "utils/curl_utils.hpp"
#include <string>
#include <vector>

struct RetrievingUtilsConfig {
    int k = 4;
};

class RetrievingUtils {
  private:
    /**
     * Pointer to the Retrieving Utils configuration.
     */
    RetrievingUtilsConfig* config;

    /**
     * Pointer to the UserInterface for displaying messages and errors.
     */
    UserInterface* ui;

    /**
     * Pointer to the CURL wrapper containing pre-configured internal logic.
     */
    CurlUtils* curl;

    /**
     * Pointer to the OllamaClient for LLM ops.
     */
    OllamaClient* ollama_client;

    /**
     * Pointer to the QdrantClient for vector database ops.
     */
    QdrantClient* storage_client;

    std::vector<std::string> retrieve(const std::string& question);

    void generate_answer(const std::string& question, const std::vector<std::string>& documents);

  public:
    RetrievingUtils(RetrievingUtilsConfig* config, UserInterface* ui, CurlUtils* curl, OllamaClient* ollama_client,
                    QdrantClient* storage_client);

    void run(void);
};
