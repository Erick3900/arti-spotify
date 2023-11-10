#include <arti/spotify/client.hpp>

#include <fstream>
#include <ranges>

#include <fmt/format.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

namespace arti::spotify {

    auth_token client::token;

    client::~client() {
        if (not store_path.empty()) {
            std::ofstream store_file{ store_path };
            store_file << token.get_data().dump(4) << std::endl;
            store_file.close();
        }
    }

    const nlohmann::json &client::get_token_data() const {
        return token.get_data();
    }

    expected<> client::initialize(nlohmann::json token_data) {
        if (auto ok = token.set(std::move(token_data)); not ok) {
            return error<>{ ok.error() };
        }

        auto api_token = token.get();

        if (not api_token) {
            if (api_token.error() == token_error::no_data) {
                auto ok = token.login();

                if (not ok) {
                    return error<>{ ok.error() };
                }

                return {};
            }

            return error<>{ "Error getting api token" };
        }

        return {};
    }

    expected<> client::initialize(std::filesystem::path tokens_data_path) {
        namespace fs = std::filesystem;

        store_path = tokens_data_path;

        if (fs::exists(store_path)) {
            try {
                std::ifstream input_file{ tokens_data_path };

                nlohmann::json tokens_data;

                input_file >> tokens_data;

                return initialize(std::move(tokens_data));
            }
            catch(std::exception &exc) {
                return error<>{ exc.what() };
            }
        }
        else {
            return initialize(nlohmann::json{});
        }
    }

    expected<api_response> client::get(std::string_view endpoint) {
        auto expected_token = token.get();

        if (not expected_token) {
            return error<>{ "Error getting the api token" };
        }

        auto api_token = expected_token.value();

        auto expected_response = curl::request::get(
            fmt::format("https://api.spotify.com/v1{}", endpoint),
            {
                {
                    "Authorization",
                    fmt::format("Bearer {}", api_token)
                },
                {
                    "Content-Type",
                    "application/json"
                }
            }
        );

        if (not expected_response) {
            return error<>{
                fmt::format(
                    "Curl error:\nCode {}, Response {}",
                    expected_response.error().code,
                    expected_response.error().message
                )
            };
        }

        auto response_body = [&] {
            if (expected_response->body.empty()) {
                return nlohmann::json{};
            }

            return nlohmann::json::parse(expected_response->body);
        }();

        return api_response{
            expected_response->code,
            response_body 
        };
	}

    expected<api_response> client::put(std::string_view endpoint, const nlohmann::json &data) {
        auto expected_token = token.get();

        if (not expected_token) {
            return error<>{ "Error getting the api token" };
        }

        auto api_token = expected_token.value();

        auto expected_response = curl::request::put(
            fmt::format("https://api.spotify.com/v1{}", endpoint),
            "application/json",
            data.dump(),
            {
                {
                    "Authorization",
                    fmt::format("Bearer {}", api_token)
                }
            }
        );

        if (not expected_response) {
            return error<>{
                fmt::format(
                    "Curl error:\nCode {}, Response {}",
                    expected_response.error().code,
                    expected_response.error().message
                )
            };
        }

        auto response_body = [&] {
            if (expected_response->body.empty()) {
                return nlohmann::json{};
            }

            return nlohmann::json::parse(expected_response->body);
        }();

        return api_response{
            expected_response->code,
            response_body 
        };
	}

    expected<api_response> client::del(std::string_view endpoint) {
        auto expected_token = token.get();

        if (not expected_token) {
            return error<>{ "Error getting the api token" };
        }

        auto api_token = expected_token.value();

        auto expected_response = curl::request::del(
            fmt::format("https://api.spotify.com/v1{}", endpoint),
            {
                {
                    "Authorization",
                    fmt::format("Bearer {}", api_token)
                },
                {
                    "Content-Type",
                    "application/json"
                }
            }
        );

        if (not expected_response) {
            return error<>{
                fmt::format(
                    "Curl error:\nCode {}, Response {}",
                    expected_response.error().code,
                    expected_response.error().message
                )
            };
        }

        auto response_body = [&] {
            if (expected_response->body.empty()) {
                return nlohmann::json{};
            }

            return nlohmann::json::parse(expected_response->body);
        }();

        return api_response{
            expected_response->code,
            response_body 
        };
	}

    expected<api_response> client::post(std::string_view endpoint, const nlohmann::json &data) {
        auto expected_token = token.get();

        if (not expected_token) {
            return error<>{ "Error getting the api token" };
        }

        auto api_token = expected_token.value();

        auto expected_response = curl::request::post(
            fmt::format("https://api.spotify.com/v1{}", endpoint),
            "application/json",
            data.dump(),
            {
                {
                    "Authorization",
                    fmt::format("Bearer {}", api_token)
                }
            }
        );

        if (not expected_response) {
            return error<>{
                fmt::format(
                    "Curl error:\nCode {}, Response {}",
                    expected_response.error().code,
                    expected_response.error().message
                )
            };
        }

        auto response_body = [&] {
            if (expected_response->body.empty()) {
                return nlohmann::json{};
            }

            return nlohmann::json::parse(expected_response->body);
        }();

        return api_response{
            expected_response->code,
            response_body 
        };
	}

}
