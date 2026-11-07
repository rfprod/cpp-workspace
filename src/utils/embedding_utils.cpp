#include "embedding_utils.hpp"
#include "utils/text_utils.hpp"
#include <string>
#include <vector>

/**
 * Constructs an EmbeddingUtils instance with the collaborators it needs for generating and storing embeddings.
 *
 * @param ui: Shared pointer to the UserInterface used to display messages and status.
 * @param curl: Shared pointer to the CurlUtils used to perform HTTP requests.
 * @param ollama_client: Shared pointer to the OllamaClient used to request embeddings from Ollama.
 * @param storage_client: Shared pointer to the QdrantClient used to store and retrieve embeddings.
 *
 * Initializes the ui, curl, ollama_client, and storage_client members by copying each shared pointer, so this instance
 * shares ownership of every dependency with the caller. The constructor body is empty, so no other work is performed.
 *
 * Note: Because the dependencies are shared, they remain alive as long as any holder still references them. Passing a
 * null pointer is not checked here, so any later use of a null dependency will fail at the point of use.
 */
EmbeddingUtils::EmbeddingUtils(std::shared_ptr<UserInterface> ui, std::shared_ptr<CurlUtils> curl,
                               std::shared_ptr<OllamaClient> ollama_client,
                               std::shared_ptr<QdrantClient> storage_client)
    : ui(ui), curl(curl), ollama_client(ollama_client), storage_client(storage_client) {
}

/**
 * Fetches documents from the given URLs, splits them into chunks, generates embeddings for those chunks, and stores
 * the chunks with their embeddings in Qdrant.
 *
 * @param urls: The web addresses whose content should be fetched and embedded.
 *
 * Runs a four-stage pipeline. First, each URL is fetched with the CurlUtils instance and its content is collected in
 * order. Second, every fetched document is split with TextUtils::split_text using a chunk size of 250 and an overlap of
 * 0, and the resulting chunks are appended to a single list. Third, the OllamaClient generates one embedding per chunk.
 * Fourth, the QdrantClient stores the chunks together with their embeddings. Before each stage, a progress message is
 * shown, using the interactive UI when it is running and the non-interactive output otherwise. When all stages finish,
 * a success message is displayed through the same mechanism.
 *
 * Note: The method assumes that the embeddings returned by the OllamaClient correspond index for index to the chunks
 * passed to it, because the storage call pairs them by position. No length check is performed. Fetch failures are not
 * detected here, so an empty or error string returned by CurlUtils is split and embedded like any other content.
 * Exceptions thrown by the curl, Ollama, Qdrant, or UI components are not caught and propagate to the caller, which
 * means a failure in any stage stops the pipeline before the later stages run.
 */
void EmbeddingUtils::create_embedings(const std::vector<std::string>& urls) {
    std::string message{};

    message = "⏳ Fetching documents from URLs...\n";
    if (UserInterface::ui_running(this->ui) == true) {
        this->ui->print_message(message);
    } else {
        UserInterface::print_message_noninteractive(message);
    }

    std::vector<std::string> all_documents;
    for (const auto& url : urls) {
        std::string content = this->curl->fetch_web_content(url);
        all_documents.push_back(content);
    }

    message = "⏳ Splitting documents into chunks...\n";
    if (UserInterface::ui_running(this->ui) == true) {
        this->ui->print_message(message);
    } else {
        UserInterface::print_message_noninteractive(message);
    }

    std::vector<std::string> document_chunks;
    for (const auto& doc : all_documents) {
        auto chunks = TextUtils::split_text(doc, 250, 0);
        document_chunks.insert(document_chunks.end(), chunks.begin(), chunks.end());
    }

    message = "⏳ Generating embeddings...\n";
    if (UserInterface::ui_running(this->ui) == true) {
        this->ui->print_message(message);
    } else {
        UserInterface::print_message_noninteractive(message);
    }
    auto embeddings = this->ollama_client->generate_embeddings(document_chunks);

    message = "⏳ Storing embeddings in Qdrant...\n";
    if (UserInterface::ui_running(this->ui) == true) {
        this->ui->print_message(message);
    } else {
        UserInterface::print_message_noninteractive(message);
    }
    this->storage_client->store(document_chunks, embeddings);

    message = "Embeddings created.\n";
    if (UserInterface::ui_running(this->ui) == true) {
        this->ui->print_success(message);
    } else {
        UserInterface::print_success_noninteractive(message);
    }
}
