#pragma once

#include <array>
#include <string>
#include <cstdlib>
#include <cstdint>

#include <curl/curl.h>

#include <arti/curl/client.hpp>

namespace arti::curl {

    using write_callback = size_t (*)(void *data, size_t size, size_t nmemb, void *user_data);

    class connection {
      public:
        struct request_info {
            int curl_code;

            double total_time;
            double name_lookup_time;
            double connect_time;
            double app_connect_time;
            double pre_transfer_time;
            double start_transfer_time;
            double redirect_time;

            uint64_t redirect_count;

            std::string curl_error;
        };

        struct info {
            bool follow_redirects;
            bool no_signal;

            int timeout;
            int64_t max_redirects;

            void *progress_fn_data;
            curl_progress_callback progress_fn;

            struct {
                std::string username;
                std::string password;
            } basic_auth;

            std::string base_url;
            std::string cert_path;
            std::string cert_type;
            std::string key_path;
            std::string key_password;
            std::string custom_user_agent;
            std::string uri_proxy;
            std::string unix_socket_path;

            header_fields headers;
            request_info last_request;
        };

        connection() noexcept = default;

        ~connection();

        expected<> initialize(std::string_view base_url);

        void terminate();

        void set_basic_auth(std::string_view username, std::string_view password);

        void set_timeout(int seconds);

        void set_file_progress_callback(curl_progress_callback progress_fn);

        void set_file_progress_callback_data(void *data);

        void set_no_signal(bool no);

        void follow_redirects(bool follow, int64_t max_redirects = -1);

        void set_user_agent(std::string_view agent);

        void set_ca_info_file_path(std::string_view ca_info_file_path);

        void set_cert_path(std::string_view cert);

        void set_cert_type(std::string_view type);

        void set_key_path(std::string_view key_path);

        void set_verify_peer(bool verify_peer);

        void set_key_password(std::string_view key_password);

        void set_proxy(std::string_view uri_proxy);

        void set_unix_socket_path(std::string_view unix_socket_path);

        void set_write_function(write_callback callback);

        std::string_view get_user_agent() const;

        info &get_info();
        const info &get_info() const;

        void set_headers(header_fields headers);

        header_fields &get_headers();
        const header_fields &get_headers() const;

        void append_header(std::string key, std::string value);

        expected<response, response_error> get(std::string_view url);

        expected<response, response_error> post(std::string_view url, std::string_view data);

        expected<response, response_error> put(std::string_view url, std::string_view data);

        expected<response, response_error> patch(std::string_view url, std::string_view data);

        expected<response, response_error> del(std::string_view url);

        expected<response, response_error> head(std::string_view url);

        expected<response, response_error> options(std::string_view url);

      private:
        expected<response, response_error> perform_curl_request(std::string_view uri);

        bool verify_peer;
        CURL *curl_handle = nullptr;
        write_callback connection_write_callback;
        std::string ca_info_file_path;
        std::array<char, CURL_ERROR_SIZE> curl_error_buffer;
        info connection_info;
    };

}    // namespace arti::curl
