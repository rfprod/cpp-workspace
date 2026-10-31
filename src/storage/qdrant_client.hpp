#pragma once

#include "interface/user_interface.hpp"
#include "utils/curl_utils.hpp"
#include "utils/system_utils.hpp"
#include <curl/curl.h>
#include <iostream>

/**
 * URLs for Qdrant API endpoints.
 *
 * @param collections_url The endpoint for querying collections.
 */
struct QdrantServerURLs {
    std::string collections_url;
};

/**
 * Manages the lifecycle and API interaction with a Qdrant vector database server running in Docker.
 *
 * The QdrantClient class is responsible for:
 * - Automatically pulling and starting a Qdrant Docker container during construction
 * - Validating required environment variables (QDRANT_API_KEY, QDRANT_API_KEY_READ_ONLY)
 * - Creating and managing a Docker network for inter-service communication
 * - Verifying server health and availability
 * - Storing text embeddings as vector points in the Qdrant database
 * - Cleaning up Docker resources during destruction
 *
 * The Qdrant server listens on localhost:6333 (gRPC) and localhost:6334 (HTTP API) by default,
 * with configuration via the QDRANT_URL environment variable.
 *
 * Lifecycle:
 * - Constructor calls setup() to initialize Docker resources and check_qdrant() to verify connectivity.
 * - Destructor automatically stops and removes the container and associated network.
 *
 * @param ui         Pointer to UserInterface for displaying status messages and errors.
 *                   Must remain valid for the lifetime of this QdrantClient instance.
 * @param curl       Pointer to CurlUtils for executing HTTP requests to the Qdrant API.
 *                   Must remain valid for the lifetime of this QdrantClient instance.
 *
 * @note Requires Docker to be installed and running on the host system.
 * @note Environment variables QDRANT_API_KEY and QDRANT_API_KEY_READ_ONLY must be set before construction.
 * @note Creates persistent data directory at $HOME/qdrant for container use.
 * @note Creates Docker network app-rag-net for service communication.
 */
class QdrantClient {
  private:
    /**
     * API endpoint URLs for the Qdrant server.
     */
    QdrantServerURLs urls;

    /**
     * Pointer to the UserInterface for displaying messages and errors.
     * Used for both CLI and interactive UI output.
     */
    UserInterface* ui;

    /**
     * Pointer to the SystemUtils for processing `system()` calls.
     */
    SystemUtils* system_utils;

    /**
     * A CURL wrapper containing pre-configured internal logic.
     */
    CurlUtils* curl;

    void setup();

    void check_qdrant();

    bool collection_exists(const std::string& collection_name);

    void delete_collection(const std::string& collection_name);

    void create_collection(const std::string& collection_name, size_t vector_size);

  public:
    explicit QdrantClient(UserInterface* ui, CurlUtils* curl, SystemUtils* system_utils);

    ~QdrantClient();

    const std::string& get_url_value(const std::string& property_name) const;

    void store(const std::vector<std::string>& texts, const std::vector<std::vector<float>>& embeddings);
};
