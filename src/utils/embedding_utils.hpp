#include "interface/user_interface.hpp"
#include "ollama/ollama_client.hpp"
#include "storage/qdrant_client.hpp"
#include "utils/curl_utils.hpp"
#include <vector>

class EmbeddingUtils {
  private:
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

  public:
    EmbeddingUtils(UserInterface* ui, CurlUtils* curl, OllamaClient* ollama_client, QdrantClient* storage_client);

    void create_embedings(const std::vector<std::string>& urls);
};
