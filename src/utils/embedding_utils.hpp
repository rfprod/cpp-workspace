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
    std::shared_ptr<UserInterface> ui;

    /**
     * Pointer to the CURL wrapper containing pre-configured internal logic.
     */
    std::shared_ptr<CurlUtils> curl;

    /**
     * Pointer to the OllamaClient for LLM ops.
     */
    std::shared_ptr<OllamaClient> ollama_client;

    /**
     * Pointer to the QdrantClient for vector database ops.
     */
    std::shared_ptr<QdrantClient> storage_client;

  public:
    EmbeddingUtils(std::shared_ptr<UserInterface> ui, std::shared_ptr<CurlUtils> curl,
                   std::shared_ptr<OllamaClient> ollama_client, std::shared_ptr<QdrantClient> storage_client);

    void create_embedings(const std::vector<std::string>& urls);
};
