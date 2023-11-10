#pragma once

#include <mutex>

#include <nlohmann/json.hpp>

#include <arti/spotify/config.hpp>

namespace arti::spotify {

    enum class token_error {
        no_data,
        renew
    };

    struct auth_token {
        auth_token() = default;
        ~auth_token() = default;

        auth_token(auth_token &&) = delete;
        auth_token &operator=(auth_token &&) = delete;

        auth_token(const auth_token &) = delete;
        auth_token &operator=(const auth_token &) = delete;

        expected<> set(nlohmann::json token_data);
        expected<> read_from(std::string_view config_path);

        expected<> login();
        expected<> renew_token();

        expected<std::string_view, token_error> get();

        const nlohmann::json &get_data() const;

      private:
        std::mutex data_mtx;
        nlohmann::json data;
    };

}

