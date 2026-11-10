#include "retrieving_utils.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using nlohmann::json_abi_v3_11_2::json;

/**
 * Constructs a RetrievingUtils instance with the configuration and collaborators it needs to retrieve stored
 * embeddings.
 *
 * @param config: Shared pointer to the RetrievingUtilsConfig that supplies the retrieval settings.
 * @param ui: Shared pointer to the UserInterface used to display messages and status.
 * @param curl: Shared pointer to the CurlUtils used to perform HTTP requests.
 * @param ollama_client: Shared pointer to the OllamaClient used to generate query embeddings.
 * @param storage_client: Shared pointer to the QdrantClient used to search stored embeddings.
 *
 * Initializes the config, ui, curl, ollama_client, and storage_client members by copying each shared pointer, so this
 * instance shares ownership of every dependency with the caller. The constructor body is empty, so no other work is
 * performed.
 *
 * Note: Because the dependencies are shared, they remain alive as long as any holder still references them. Passing a
 * null pointer is not checked here, so any later use of a null dependency will fail at the point of use.
 */
RetrievingUtils::RetrievingUtils(std::shared_ptr<RetrievingUtilsConfig> config, std::shared_ptr<UserInterface> ui,
                                 std::shared_ptr<CurlUtils> curl, std::shared_ptr<OllamaClient> ollama_client,
                                 std::shared_ptr<QdrantClient> storage_client)
    : config(config), ui(ui), curl(curl), ollama_client(ollama_client), storage_client(storage_client) {
}

/**
 * Retrieves the stored document chunks that are most similar to a user question.
 *
 * @param question: The natural-language question to search for.
 * @return The text payloads of the top matching chunks from Qdrant, ordered by similarity. The list has at most
 * config->k entries.
 * @throws std::runtime_error If the "embedding_model" config property or the "embed_url" or "collections_url" URL
 * property is not supported, or if the "embedding_model" value is not a std::string.
 * @throws nlohmann::json::parse_error If the embedding response or the search response is not valid JSON.
 * @throws nlohmann::json::type_error If the embedding response does not contain a numeric "embeddings" array, or if a
 * search result does not contain a string "payload" "text" field.
 *
 * Builds a JSON request with the embedding model name and the question, then posts it to the Ollama embed URL through
 * the CurlUtils instance. The first vector in the returned "embeddings" array becomes the question embedding. That
 * vector, the configured result limit k, and a flag requesting payloads are serialized into a search request. The
 * request is posted to the "rag-app/points/search" endpoint under the Qdrant collections URL, with the api-key header
 * set from the QDRANT_API_KEY environment variable. Each entry in the "result" array of the search response contributes
 * its payload text to the returned list. In debug builds, when the UI is not running, both raw responses are printed
 * to the non-interactive output.
 *
 * Note: The method depends on the QDRANT_API_KEY environment variable. If it is unset, the null pointer passed to the
 * header map is not checked, which can cause undefined behavior or a std::logic_error at construction. The response
 * is not checked for HTTP errors before parsing, so an error body from either service typically surfaces as a parse or
 * type exception. The method reads only the first embedding, so it expects the embed service to return exactly one
 * vector for the single question.
 */
std::vector<std::string> RetrievingUtils::retrieve(const std::string& question) {
    json request_payload = {{"model", this->ollama_client->get_config_value<std::string>("embedding_model")},
                            {"input", question}};

    std::string post_data = request_payload.dump();

    std::string response = this->curl->fetch_web_content(this->ollama_client->get_url_value("embed_url"), post_data);

#ifdef DEBUG
    if (this->ui->backend_running() == false) {
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
    if (this->ui->backend_running() == false) {
        UserInterface::print_message_noninteractive(std::format("\t[DEBUG] search response {}\n", response));
    }
#endif

    data = json::parse(response);

    std::vector<std::string> documents;

    std::transform(data["result"].begin(), data["result"].end(), std::back_inserter(documents),
                   [](const auto& result) { return result["payload"]["text"].template get<std::string>(); });

    return documents;
}

/**
 * Builds a retrieval-augmented prompt from a question and its supporting documents, sends it to the Ollama generate
 * endpoint, and processes the model's answer.
 *
 * @param question: The user question that the model should answer.
 * @param documents: The retrieved document texts that provide context for the answer. Each entry is joined with a
 * newline.
 * @throws std::runtime_error If the "model" or "stream" config property is not supported or has an unexpected type, or
 * if the "generate_url" URL property is not supported.
 * @throws nlohmann::json::exception If the streamed or non-streamed response cannot be parsed, as propagated from the
 * OllamaClient response handlers.
 *
 * Concatenates the document texts into one string, separating entries with newlines, and places that string together
 * with the question into a fixed instruction template. The template asks the model to answer in at most three
 * sentences and to say when it does not know the answer. The prompt, the model name, a temperature of 0, and the
 * stream flag are serialized into a JSON request. When streaming is enabled, the request is posted through
 * fetch_web_content_stream, and each received chunk is forwarded to OllamaClient::process_response_stream. The
 * resulting success flag and status are then reported through the UI or the non-interactive output. When streaming is
 * disabled, the request is posted with fetch_web_content, and the full body is passed to
 * OllamaClient::process_response.
 *
 * Note: The question and document text are inserted into the prompt without escaping or sanitizing, so instructions
 * contained in retrieved documents can influence the model. The method does not check whether the documents vector is
 * empty, although the join loop itself handles that case safely. Errors from the HTTP layer are not inspected beyond
 * the stream status message, so a failed non-streamed request reaches the response handler as-is.
 */
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
        if (this->ui->backend_running() == false) {
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

/**
 * Runs one retrieval-augmented question-answering cycle using the prompt from the configuration.
 *
 * @throws std::runtime_error If the "prompt" config property is not supported or has an unexpected type, or if
 * retrieve or generate_answer fails with a runtime error.
 * @throws nlohmann::json::parse_error If a retrieval response cannot be parsed as JSON, as propagated from retrieve.
 * @throws nlohmann::json::type_error If a retrieval response has an unexpected JSON structure, as propagated from
 * retrieve.
 *
 * Reads the "prompt" string from the Ollama client configuration and keeps a constant reference to it. The prompt is
 * then shown to the user as a message, either through the interactive UI when it is running or through non-interactive
 * output otherwise. The same prompt is used as the search input for retrieve, which embeds it and returns the matching
 * document texts from the vector store. Those documents, together with the prompt as the question, are passed to
 * generate_answer, which builds the final prompt, queries the model, and processes the answer.
 *
 * Note: The prompt is both the displayed user message and the retrieval query, and it is also used as the question in
 * generate_answer. Because the returned reference points into the configuration object, the configuration must outlive
 * this method. The method performs no check on whether retrieve returned any documents, so generate_answer is called
 * even with an empty list.
 */
void RetrievingUtils::run() {
    const std::string& prompt = this->ollama_client->get_config_value<std::string>("prompt");

    std::string message = std::format("💬 user: {}\n", prompt);
    if (this->ui->backend_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_message(message);
    }

    auto documents = this->retrieve(prompt);

    this->generate_answer(prompt, documents);
}
