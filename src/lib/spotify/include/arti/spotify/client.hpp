#pragma once

#include <nlohmann/json.hpp>

#include <arti/curl/connection.hpp>

#include <arti/spotify/config.hpp>

#include <arti/spotify/token.hpp>
#include <arti/spotify/response.hpp>

namespace arti::spotify {

    struct client {
        client() = default;
        ~client();

        client(client &&) = default;
        client &operator=(client &&) = default;

        client(const client &) = delete;
        client &operator=(const client &) = delete;

        expected<> initialize(nlohmann::json token_data);
        expected<> initialize(std::filesystem::path tokens_data_path);

        expected<api_response> get(std::string_view endpoint);
        expected<api_response> del(std::string_view endpoint);

        expected<api_response> put(std::string_view endpoint, const nlohmann::json &data);
        expected<api_response> post(std::string_view endpoint, const nlohmann::json &data);

        const nlohmann::json &get_token_data() const;

      private:
        std::filesystem::path store_path;
        static auth_token token;
    };

}

