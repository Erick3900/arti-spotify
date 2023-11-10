#include <arti/curl/client.hpp>

#include <arti/curl/connection.hpp>

namespace arti::curl {

    expected<> request::initialize() {
        static raii object;

        if (object) {
            return {};
        }

        return object.initialize();
    }

    expected<> request::raii::initialize() {
        CURLcode res = curl_global_init(CURL_GLOBAL_ALL);

        if (res == CURLE_OK) {
            initialized = true;
            return {};
        }
        return error<>{ "Couldn't initialize global curl" };
    }

    request::raii::~raii() {
        curl_global_cleanup();
    }

    request::raii::operator bool() {
        return initialized;
    }

    expected<response, response_error> request::get(std::string_view url, header_fields headers) {
        connection conn;

        if (auto init = conn.initialize(""); not init) {
            return error<response_error>{{
                .code = -1,
                .message = init.error()
            }};
        }

        for (const auto &[k, v] : headers) {
            conn.append_header(k, v);
        }

        return conn.get(url);
    }

    expected<response, response_error> request::post(std::string_view url, std::string_view content_type, std::string_view data, header_fields headers) {
        connection conn;

        if (auto init = conn.initialize(""); not init) {
            return error<response_error>{{
                .code = -1,
                .message = init.error()
            }};
        }

        conn.append_header("Content-Type", std::string{ content_type });

        for (const auto &[k, v] : headers) {
            conn.append_header(k, v);
        }

        return conn.post(url, data);
    }

    expected<response, response_error> request::put(std::string_view url, std::string_view content_type, std::string_view data, header_fields headers) {
        connection conn;

        if (auto init = conn.initialize(""); not init) {
            return error<response_error>{{
                .code = -1,
                .message = init.error()
            }};
        }

        conn.append_header("Content-Type", std::string{ content_type });

        for (const auto &[k, v] : headers) {
            conn.append_header(k, v);
        }

        return conn.put(url, data);
    }

    expected<response, response_error> request::patch(std::string_view url, std::string_view content_type, std::string_view data, header_fields headers) {
        connection conn;

        if (auto init = conn.initialize(""); not init) {
            return error<response_error>{{
                .code = -1,
                .message = init.error()
            }};
        }

        conn.append_header("Content-Type", std::string{ content_type });

        for (const auto &[k, v] : headers) {
            conn.append_header(k, v);
        }

        return conn.patch(url, data);
	}

    expected<response, response_error> request::del(std::string_view url, header_fields headers) {
        connection conn;

        if (auto init = conn.initialize(""); not init) {
            return error<response_error>{{
                .code = -1,
                .message = init.error()
            }};
        }

        for (const auto &[k, v] : headers) {
            conn.append_header(k, v);
        }

        return conn.del(url);
	}

    expected<response, response_error> request::head(std::string_view url, header_fields headers) {
         connection conn;

        if (auto init = conn.initialize(""); not init) {
            return error<response_error>{{
                .code = -1,
                .message = init.error()
            }};
        }

        for (const auto &[k, v] : headers) {
            conn.append_header(k, v);
        }

        return conn.head(url);
	}

    expected<response, response_error> request::options(std::string_view url, header_fields headers) {
         connection conn;

        if (auto init = conn.initialize(""); not init) {
            return error<response_error>{{
                .code = -1,
                .message = init.error()
            }};
        }

        for (const auto &[k, v] : headers) {
            conn.append_header(k, v);
        }

        return conn.options(url);
	}

}    // namespace arti::curl
