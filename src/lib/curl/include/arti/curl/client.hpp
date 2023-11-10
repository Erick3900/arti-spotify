#pragma once

#include <map>
#include <string>

#include <curl/curl.h>

#include <arti/curl/error.hpp>

namespace arti::curl {

    using header_fields = std::map<std::string, std::string>;

    struct response {
        int code;
        std::string body;
        header_fields headers;
    };

    struct request {

        static expected<response, response_error> get(std::string_view url, header_fields headers = {});
        static expected<response, response_error> post(std::string_view url, std::string_view content_type, std::string_view data, header_fields headers = {});
        static expected<response, response_error> put(std::string_view url, std::string_view content_type, std::string_view data, header_fields headers = {});
        static expected<response, response_error> patch(std::string_view url, std::string_view content_type, std::string_view data, header_fields headers = {});
        static expected<response, response_error> del(std::string_view url, header_fields headers = {});
        static expected<response, response_error> head(std::string_view url, header_fields headers = {});
        static expected<response, response_error> options(std::string_view url, header_fields headers = {});

      private:
        static expected<> initialize();

        struct raii {
            raii() = default;
            ~raii();

            expected<> initialize();

            operator bool();

            bool initialized;
        };
    };

}    // namespace arti::curl
