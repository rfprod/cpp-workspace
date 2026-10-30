#include "embedding_utils.hpp"
#include "utils/text_utils.hpp"
#include <string>
#include <vector>

EmbeddingUtils::EmbeddingUtils(UserInterface* ui, CurlUtils* curl, OllamaClient* ollama_client,
                               QdrantClient* storage_client)
    : ui(ui), curl(curl), ollama_client(ollama_client), storage_client(storage_client) {
}

void EmbeddingUtils::create_embedings(const std::vector<std::string>& urls) {
    std::string message{};

    message = "⏳ Fetching documents from URLs...\n";
    if (this->ui->is_running() == true) {
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
    if (this->ui->is_running() == true) {
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
    if (this->ui->is_running() == true) {
        this->ui->print_message(message);
    } else {
        UserInterface::print_message_noninteractive(message);
    }
    auto embeddings = this->ollama_client->generate_embeddings(document_chunks);

    message = "⏳ Storing embeddings in Qdrant...\n";
    if (this->ui->is_running() == true) {
        this->ui->print_message(message);
    } else {
        UserInterface::print_message_noninteractive(message);
    }
    this->storage_client->store(document_chunks, embeddings);

    message = "Embeddings created.\n";
    if (this->ui->is_running() == true) {
        this->ui->print_success(message);
    } else {
        UserInterface::print_success_noninteractive(message);
    }
}
