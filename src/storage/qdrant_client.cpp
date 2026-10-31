#include "qdrant_client.hpp"
#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using nlohmann::json_abi_v3_11_2::json;

/**
 * Initializes a QdrantClient instance with the provided UI and networking utilities.
 * Constructs the base URL from the QDRANT_URL environment variable, defaulting to the local Qdrant service.
 * Performs an immediate health check of the Qdrant service upon construction.
 *
 * @param ui Pointer to a UserInterface instance for status message output.
 * @param curl Pointer to a CurlUtils instance for HTTP communication with Qdrant.
 * @param system_utils Pointer to a SustemUtils instance for `system()` call status handling.
 *
 * @note Constructor calls check_qdrant() as a side effect, which may produce console or UI output.
 */
QdrantClient::QdrantClient(UserInterface* ui, CurlUtils* curl, SystemUtils* system_utils) {
    this->curl = curl;
    this->ui = ui;
    this->system_utils = system_utils;

    this->setup();

    sleep(1);

    const std::string& base_url = std::getenv("QDRANT_URL") ? std::getenv("QDRANT_URL") : "http://localhost:6333";
    this->urls = {
        collections_url : std::format("{}/collections", base_url),
    };

    this->check_qdrant();

    if (ui->is_running() == false) {
        UserInterface::print_message_noninteractive("\n");
    }
}

/**
 * Cleans up Docker resources associated with the Qdrant container and network.
 * Stops and removes the qdrant-rag-app container and destroys the app-rag-net Docker network.
 * Suppresses errors during cleanup to ensure all resources are removed even if individual commands fail.
 *
 * @note This destructor is called automatically when the QdrantClient instance is destroyed.
 */
QdrantClient::~QdrantClient() {
    int status;
    std::string message{};

    message = "🛈 Qdrant docker container cleanup...\n";
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_message(message);
    }

    status = system("docker stop qdrant-rag-app >/dev/null 2>&1 || true");
    this->system_utils->catch_error(status);

    status = system("docker rm qdrant-rag-app >/dev/null 2>&1 || true");
    this->system_utils->catch_error(status);

    status = system("docker network rm app-rag-net >/dev/null 2>&1 || true");
    this->system_utils->catch_error(status);

    message = "Done.\n";
    if (this->ui->is_running() == false) {
        UserInterface::print_success_noninteractive(message);
    } else {
        this->ui->print_success(message);
    }
}

const std::string& QdrantClient::get_url_value(const std::string& property_name) const {
    if (property_name == "collections_url") {
        return this->urls.collections_url;
    }

    throw std::runtime_error("Property not supported: " + property_name);
}

/**
 * Initializes and starts the Qdrant Docker container with necessary configuration.
 * Pulls the Qdrant image, validates the QDRANT_API_KEY and QDRANT_API_KEY_READ_ONLY environment variables,
 * creates a persistent data directory, establishes a Docker network, and launches the container with API key
 * authentication. If a Qdrant container is already running, this method returns early without restarting it.
 *
 * @throws Exits with status code 1 if required environment variables are missing or container startup fails.
 *
 * @note This method should be called before check_qdrant() to ensure the service is available.
 * @note Does not wait for the container to be fully operational; health verification is delegated to check_qdrant().
 */
void QdrantClient::setup() {
    int status;
    std::string message{};

    status = system("docker pull qdrant/qdrant");
    this->system_utils->catch_error(status);

    status = system("docker ps | grep -q qdrant");
    if (status == 0) {
        message = "Qdrant is already running. Skipping docker run.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_success_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
        return;
    }

    message = "🛈 Starting Qdrant container...\n";
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_message(message);
    }

    const char* qdrant_key = std::getenv("QDRANT_API_KEY");
    const char* qdrant_key_readonly = std::getenv("QDRANT_API_KEY_READ_ONLY");

    if (!qdrant_key || std::string(qdrant_key).empty()) {
        message = "QDRANT_API_KEY not set.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }

        exit(1);
    }

    if (!qdrant_key_readonly || std::string(qdrant_key_readonly).empty()) {
        message = "QDRANT_API_KEY_READ_ONLY not set.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }

        exit(1);
    }

    const std::string data_dir = std::string(std::getenv("HOME")) + "/qdrant";
    std::string mkdir_cmd = std::string("mkdir -p ") + data_dir;
    status = system(mkdir_cmd.c_str());
    this->system_utils->catch_error(status);

    status = system("docker network create app-rag-net 2>/dev/null || true");
    this->system_utils->catch_error(status);

    std::string docker_run_cmd =
        std::format("docker run -d --name qdrant-rag-app --restart always --network app-rag-net "
                    "-p 6333:6333 -p 6334:6334 -v {}:/qdrant/storage "
                    "-e QDRANT_API_KEY='{}' -e QDRANT_API_KEY_READ_ONLY='{}' qdrant/qdrant:latest",
                    data_dir, qdrant_key, qdrant_key_readonly);

    if (system(docker_run_cmd.c_str()) != 0) {
        message = "Failed to start Qdrant container.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }

        exit(1);
    }

    message = std::format("🛈 Qdrant container started with data dir {}\n", data_dir.c_str());
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_message(message);
    }

    message = "🛈 Qdrant dashboard is available at http://localhost:6333/dashboard\n";
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_message(message);
    }
}

/**
 * Checks the health status of the Qdrant service using the configured API endpoint.
 * Retrieves the API key from the QDRANT_API_KEY environment variable for authentication.
 * Outputs a status message (success, error, or warning) to the UI or console based on the HTTP response code.
 */
void QdrantClient::check_qdrant() {
    std::string message{};

    const char* qdrant_key = std::getenv("QDRANT_API_KEY");
    std::map<std::string, std::string> headers{{"api-key", qdrant_key}};

    long status = this->curl->wait_for_service(this->urls.collections_url, headers);

    if (status == 200) {
        message = "Qdrant is healthy.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_success_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
    } else if (status == -1) {
        message = "Failed to connect to Qdrant.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }
    } else {
        message = std::format("⚠️ Qdrant returned {}.\n", status);
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(message);
        } else {
            this->ui->print_message(message);
        }
    }
};

/**
 * @brief Checks if a collection exists in Qdrant.
 * @param collection_name The name of the collection to check.
 * @return true if the collection exists, false otherwise.
 */
bool QdrantClient::collection_exists(const std::string& collection_name) {
    std::string message{};

    std::string url = std::format("{}/{}", this->urls.collections_url, collection_name);

    const char* qdrant_key = std::getenv("QDRANT_API_KEY");
    std::map<std::string, std::string> headers{{"api-key", qdrant_key}};

    try {
        this->curl->fetch_web_content(url, std::nullopt, "GET", headers, false);
        message = std::format("Collection '{}' exists", collection_name);
        if (this->ui->is_running() == false) {
            UserInterface::print_success_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
        return true;
    } catch (...) {
        message = std::format("⚠️ Collection '{}' does not exist", collection_name);
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(message);
        } else {
            this->ui->print_message(message);
        }
        return false;
    }
}

/**
 * @brief Deletes a collection from Qdrant.
 * @param collection_name The name of the collection to delete.
 */
void QdrantClient::delete_collection(const std::string& collection_name) {
    std::string url = std::format("{}/{}", this->urls.collections_url, collection_name);

    const char* qdrant_key = std::getenv("QDRANT_API_KEY");
    std::map<std::string, std::string> headers{{"api-key", qdrant_key}};

    try {
        this->curl->fetch_web_content(url, std::nullopt, "DELETE", headers);

        std::string message = std::format("Deleted collection '{}'\n", collection_name);
        if (this->ui->is_running() == false) {
            UserInterface::print_success_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
    } catch (const HttpException& e) {
        std::string message =
            std::format("[{}] Failed to delete collection '{}': {}\n", e.get_status_code(), collection_name, e.what());
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }
        throw;
    }
}

/**
 * @brief Creates a new collection in Qdrant.
 * @param collection_name The name of the collection to create.
 * @param vector_size The dimensionality of the vectors (e.g., 384 for embeddinggemma:latest).
 */
void QdrantClient::create_collection(const std::string& collection_name, size_t vector_size) {
    json config = {{"vectors", {{"size", vector_size}, {"distance", "Cosine"}}}};

    std::string url = std::format("{}/{}", this->urls.collections_url, collection_name);
    std::string post_data = config.dump();

    const char* qdrant_key = std::getenv("QDRANT_API_KEY");
    std::map<std::string, std::string> headers{{"api-key", qdrant_key}};

    try {
        this->curl->fetch_web_content(url, post_data, "PUT", headers);

        std::string message =
            std::format("Created collection '{}' with vector size {}\n", collection_name, vector_size);
        if (this->ui->is_running() == false) {
            UserInterface::print_success_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
    } catch (const HttpException& e) {
        std::string message =
            std::format("[{}] Failed to create collection '{}': {}\n", e.get_status_code(), collection_name, e.what());
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }
        throw;
    }
}

/**
 * Stores text documents and their corresponding embeddings into the Qdrant collection.
 * Constructs points from the provided texts and embeddings, then sends them to the Qdrant service via PUT request.
 * Authentication is performed using the QDRANT_API_KEY environment variable.
 *
 * @param texts A vector of text strings to be stored as payloads in Qdrant points.
 * @param embeddings A vector of embedding vectors (floats) corresponding to each text string.
 *
 * @note The size of texts and embeddings vectors should match; behavior is undefined if they differ.
 */
void QdrantClient::store(const std::vector<std::string>& texts, const std::vector<std::vector<float>>& embeddings) {
    std::string message{};
    const std::string collection_name = "rag-app";

    if (texts.empty() || embeddings.empty()) {
        message = "Cannot store: texts or embeddings are empty\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }
        return;
    }

    if (texts.size() != embeddings.size()) {
        message = std::format("Mismatch: {} texts but {} embeddings\n", texts.size(), embeddings.size());
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }
        return;
    }

    size_t vector_size = embeddings[0].size();

#ifdef DEBUG
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(
            std::format("\t[DEBUG] Checking collection '{}'\n", collection_name));
    }
#endif

    if (this->collection_exists(collection_name)) {
#ifdef DEBUG
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(
                std::format("\t[DEBUG] Collection '{}' exists, deleting...\n", collection_name));
        }
#endif
        this->delete_collection(collection_name);
    }

#ifdef DEBUG
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(
            std::format("\t[DEBUG] Creating collection '{}' with vector size {}\n", collection_name, vector_size));
    }
#endif
    this->create_collection(collection_name, vector_size);

    json points = json::array();
    for (size_t i = 0; i < texts.size(); i += 1) {
        points.push_back({{"id", i}, {"vector", embeddings[i]}, {"payload", {{"text", texts[i]}}}});
    }

    json request_payload = {{"points", points}};

    std::string url = std::format("{}/{}/points", this->urls.collections_url, collection_name);
    std::string post_data = request_payload.dump();

    const char* qdrant_key = std::getenv("QDRANT_API_KEY");
    std::map<std::string, std::string> headers{{"api-key", qdrant_key}};

#ifdef DEBUG
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(
            std::format("\t[DEBUG] Storing {} embeddings to URL: {}\n", texts.size(), url));
    }
#endif
    std::string response = this->curl->fetch_web_content(url, post_data, "PUT", headers);

    message = std::format("Stored {} embeddings in Qdrant.\nResponse: {}\n", texts.size(), response);
    if (this->ui->is_running() == false) {
        UserInterface::print_success_noninteractive(message);
    } else {
        this->ui->print_success(message);
    }
}
