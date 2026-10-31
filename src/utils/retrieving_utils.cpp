#include "retrieving_utils.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using nlohmann::json_abi_v3_11_2::json;

RetrievingUtils::RetrievingUtils(RetrievingUtilsConfig* config, UserInterface* ui, CurlUtils* curl,
                                 OllamaClient* ollama_client, QdrantClient* storage_client)
    : config(config), ui(ui), curl(curl), ollama_client(ollama_client), storage_client(storage_client) {
}

std::vector<std::string> RetrievingUtils::retrieve(const std::string& question) {
    json request_payload = {{"model", this->ollama_client->get_config_value<std::string>("embedding_model")},
                            {"input", question}};

    std::string post_data = request_payload.dump();

    std::string response = this->curl->fetch_web_content(this->ollama_client->get_url_value("embed_url"), post_data);

#ifdef DEBUG
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(std::format("\t[DEBUG] question embed response {}\n", response));
    }
#endif

    json data = json::parse(response);

    std::vector<float> question_embedding = data["embeddings"][0].get<std::vector<float>>();

    request_payload = {{"vector", question_embedding}, {"limit", this->config->k}, {"with_payload", true}};

    post_data = request_payload.dump();

    std::string url =
        std::format("{}/{}", this->storage_client->get_url_value("collections_url"), "rag-app/points/search");

    const char* qdrant_key = std::getenv("QDRANT_API_KEY");
    std::map<std::string, std::string> headers{{"api-key", qdrant_key}};

    response = this->curl->fetch_web_content(url, post_data, "POST", headers);

#ifdef DEBUG
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(std::format("\t[DEBUG] search response {}\n", response));
    }
#endif

    data = json::parse(response);

    std::vector<std::string> documents;

    std::transform(data["result"].begin(), data["result"].end(), std::back_inserter(documents),
                   [](const auto& result) { return result["payload"]["text"].template get<std::string>(); });

    return documents;
}

void RetrievingUtils::generate_answer(const std::string& question, const std::vector<std::string>& documents) {
    std::string document_texts{};

    for (size_t i = 0; i < documents.size(); i += 1) {
        document_texts += documents[i];
        if (i < documents.size() - 1) document_texts += "\n";
    }

    std::string prompt = R"(You are an assistant for question-answering tasks.
Use the following documents to answer the question.
If you don't know the answer, just say that you don't know.
Use three sentences maximum and keep the answer concise:
Question: )" + question + R"(
Documents: )" + document_texts
                         + R"(
Answer:
)";

    json request_payload = {{"model", this->ollama_client->get_config_value<std::string>("model")},
                            {"prompt", prompt},
                            {"temperature", 0},
                            {"stream", this->ollama_client->get_config_value<bool>("stream")}};

    std::string post_data = request_payload.dump();

    if (this->ollama_client->get_config_value<bool>("stream")) {
        WebContentStream response = this->curl->fetch_web_content_stream(
            this->ollama_client->get_url_value("generate_url"), post_data,
            [this](const std::string& chunk) { this->ollama_client->process_response_stream(chunk); });
        std::string message = std::format("Stream successful: {}, Status: {}\n", response.success, response.status);
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(message);
        } else {
            this->ui->print_message(message);
        }
    } else {
        std::string response =
            this->curl->fetch_web_content(this->ollama_client->get_url_value("generate_url"), post_data);
        this->ollama_client->process_response(response);
    }
}

void RetrievingUtils::run() {
    const std::string& prompt = this->ollama_client->get_config_value<std::string>("prompt");

    std::string message = std::format("💬 user: {}\n", prompt);
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_message(message);
    }

    auto documents = this->retrieve(prompt);

    this->generate_answer(prompt, documents);
}
