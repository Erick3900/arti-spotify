#include <arti/curl/connection.hpp>

#include <algorithm>

#include <arti/curl/info.hpp>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

static size_t write_callback_helper(void *data, size_t size, size_t nmemb, void *user_data) {
    arti::curl::response *resp = reinterpret_cast<arti::curl::response *>(user_data);
    resp->body.append(reinterpret_cast<char*>(data), size * nmemb);
    return (size * nmemb);
}

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}


size_t header_callback(void *data, size_t size, size_t nmemb, void *user_data) {
    arti::curl::response *resp = reinterpret_cast<arti::curl::response *>(user_data);

    std::string header{ reinterpret_cast<char*>(data), size * nmemb };

    size_t separator = header.find_first_of(':');

    if (separator == std::string::npos) {
        trim(header);

        if (header.empty()) {
            return (size * nmemb);
        }

        resp->headers[header] = "present";
    }
    else {
        std::string key = header.substr(0, separator);
        std::string value = header.substr(separator + 1);

        trim(key);
        trim(value);

        resp->headers[key] = std::move(value);
    }

    return (size * nmemb);
}

struct upload_object {
    const char *data;
    size_t size;
};

size_t read_callback(void *data, size_t size, size_t nmemb, void *user_data) {
    upload_object *obj = reinterpret_cast<upload_object *>(user_data);
    size_t curl_size = size * nmemb;
    size_t copy_size = std::min(obj->size, curl_size);

    std::memcpy(data, obj->data, copy_size);

    obj->size = copy_size;
    obj->data += copy_size;

    return copy_size;
}

namespace arti::curl {

    connection::~connection() {
        terminate();
    }

    expected<> connection::initialize(std::string_view base_url) {
        curl_handle = curl_easy_init();

        if (not curl_handle) {
            return error<>{ "Couldn't initialize curl handle" };
        }

        connection_info.base_url = base_url;
        connection_info.timeout = 0;
        connection_info.max_redirects = -1;
        connection_info.no_signal = false;
        connection_info.progress_fn = nullptr;
        connection_info.custom_user_agent = fmt::format("arti-curl/{}", curl::info::version);

        verify_peer = true;

        connection_write_callback = write_callback_helper;

        return {};
    }

    void connection::terminate() {
        if (curl_handle) {
            curl_easy_cleanup(curl_handle);
        }

        curl_handle = nullptr;
    }

    void connection::set_basic_auth(std::string_view username, std::string_view password) {
        connection_info.basic_auth.username = username;
        connection_info.basic_auth.password = password;
	}

    void connection::set_timeout(int seconds) {
        connection_info.timeout = seconds;
	}

    void connection::set_file_progress_callback(curl_progress_callback progress_fn) {
        connection_info.progress_fn = progress_fn;
	}

    void connection::set_file_progress_callback_data(void *data) {
        connection_info.progress_fn_data = data;
	}

    void connection::set_no_signal(bool no) {
        connection_info.no_signal = no;
	}

    void connection::follow_redirects(bool follow, int64_t max_redirects) {
        connection_info.follow_redirects = follow;
        connection_info.max_redirects = max_redirects;
	}

    void connection::set_user_agent(std::string_view agent) {
        connection_info.custom_user_agent = agent;
	}

    void connection::set_ca_info_file_path(std::string_view file_path) {
        ca_info_file_path = file_path;
	}

    void connection::set_cert_path(std::string_view cert) {
        connection_info.cert_path = cert;
	}

    void connection::set_cert_type(std::string_view type) {
        connection_info.cert_type = type;
	}

    void connection::set_key_path(std::string_view key_path) {
        connection_info.key_path = key_path;
	}

    void connection::set_verify_peer(bool verify) {
        verify_peer = verify;
	}

    void connection::set_key_password(std::string_view key_password) {
        connection_info.key_password = key_password;
	}

    void connection::set_proxy(std::string_view uri_proxy) {
        std::string uri_proxy_upper{ uri_proxy };

        std::transform(
            uri_proxy_upper.begin(),
            uri_proxy_upper.end(),
            uri_proxy_upper.begin(),
            ::toupper
        );

        if (uri_proxy_upper.length() > 0 && uri_proxy_upper.compare(0, 4, "HTTP") != 0) {
            connection_info.uri_proxy = fmt::format("http://{}", uri_proxy);
        }
        else {
            connection_info.uri_proxy = uri_proxy;
        }
	}

    void connection::set_unix_socket_path(std::string_view unix_socket_path) {
        connection_info.unix_socket_path = unix_socket_path;
	}

    void connection::set_write_function(write_callback callback) {
        connection_write_callback = callback;
	}

    std::string_view connection::get_user_agent() const {
        return connection_info.custom_user_agent;
	}

    const connection::info &connection::get_info() const {
        return connection_info;
	}

    connection::info &connection::get_info() {
        return connection_info;
	}

    void connection::set_headers(header_fields headers) {
        connection_info.headers = std::move(headers);
	}

    const header_fields &connection::get_headers() const {
        return connection_info.headers;
	}

    header_fields &connection::get_headers() {
        return connection_info.headers;
	}

    void connection::append_header(std::string key, std::string value) {
        connection_info.headers.emplace(std::move(key), std::move(value));
	}

    expected<response, response_error> connection::get(std::string_view url) {
        return perform_curl_request(url);
    }

    expected<response, response_error> connection::post(std::string_view url, std::string_view data) {
        curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data.begin());
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, data.size());
        return perform_curl_request(url);
    }

    expected<response, response_error> connection::put(std::string_view url, std::string_view data) {
        upload_object up_obj;
        up_obj.data = data.begin();
        up_obj.size = data.size();

        curl_easy_setopt(curl_handle, CURLOPT_PUT, 1);
        curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);

        curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl_handle, CURLOPT_READDATA, &up_obj);

        curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE, static_cast<int64_t>(up_obj.size));

        return perform_curl_request(url);
    }

    expected<response, response_error> connection::patch(std::string_view url, std::string_view data) {
        upload_object up_obj;
        up_obj.data = data.begin();
        up_obj.size = data.size();

        const char *http_patch = "PATCH";

        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, http_patch);
        curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);

        curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl_handle, CURLOPT_READDATA, &up_obj);

        curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE, static_cast<int64_t>(up_obj.size));

        return perform_curl_request(url);
    }

    expected<response, response_error> connection::del(std::string_view url) {
        const char *http_del = "DELETE";

        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, http_del);

        return perform_curl_request(url);
    }

    expected<response, response_error> connection::head(std::string_view url) {
        const char *http_head = "HEAD";

        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, http_head);
        curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);

        return perform_curl_request(url);
    }

    expected<response, response_error> connection::options(std::string_view url) {
        const char *http_options = "OPTIONS";

        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, http_options);
        curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);

        return perform_curl_request(url);
    }

    expected<response, response_error> connection::perform_curl_request(std::string_view uri) {
        if (not curl_handle) {
            return error<response_error>{{
                .code = -1,
                .message = "Not initialized, curl handle is nullptr"
            }};
        }

        response resp;

        std::string url = fmt::format("{}{}", connection_info.base_url, uri);
        std::string header_string;

        CURLcode res = CURLE_OK;

        curl_slist *header_list = nullptr;

        curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, connection_write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, ::header_callback);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &resp);

        for (const auto &[k, v] : connection_info.headers) {
            header_string = fmt::format("{}: {}", k, v);
            header_list = curl_slist_append(header_list, header_string.c_str());
        }
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, header_list);

        if (not connection_info.basic_auth.username.empty()) {
            auto auth_string = fmt::format("{}: {}",
                connection_info.basic_auth.username,
                connection_info.basic_auth.password
            );

            curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
            curl_easy_setopt(curl_handle, CURLOPT_USERPWD, auth_string.c_str());
        }

        curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, curl_error_buffer.begin());

        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, connection_info.custom_user_agent.c_str());

        if (connection_info.timeout) {
            curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, connection_info.timeout);
            curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
        }

        if (connection_info.follow_redirects) {
            curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, connection_info.max_redirects);
        }

        if (connection_info.no_signal) {
            curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
        }

        if (connection_info.progress_fn) {
            curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0);
            curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, connection_info.progress_fn);

            if (connection_info.progress_fn_data) {
                curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, connection_info.progress_fn_data);
            }
            else {
                curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, this);
            }
        }

        if (not ca_info_file_path.empty()) {
            curl_easy_setopt(curl_handle, CURLOPT_CAINFO, ca_info_file_path.c_str());
        }

        if (not connection_info.cert_path.empty()) {
            curl_easy_setopt(curl_handle, CURLOPT_SSLCERT, connection_info.cert_path.c_str());
        }

        if (not connection_info.cert_type.empty()) {
            curl_easy_setopt(curl_handle, CURLOPT_SSLCERTTYPE, connection_info.cert_type.c_str());
        }

        if (not connection_info.key_path.empty()) {
            curl_easy_setopt(curl_handle, CURLOPT_SSLKEY, connection_info.key_path.c_str());
        }

        if (not connection_info.key_password.empty()) {
            curl_easy_setopt(curl_handle, CURLOPT_KEYPASSWD, connection_info.key_password.c_str());
        }

        if (not verify_peer) {
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, verify_peer);
        }

        if (not connection_info.uri_proxy.empty()) {
            curl_easy_setopt(curl_handle, CURLOPT_PROXY, connection_info.uri_proxy.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_HTTPPROXYTUNNEL, 1);
        }

        if (not connection_info.unix_socket_path.empty()) {
            curl_easy_setopt(curl_handle, CURLOPT_UNIX_SOCKET_PATH, connection_info.unix_socket_path.c_str());
        }

        res = curl_easy_perform(curl_handle);

        connection_info.last_request.curl_code = res;

        if (res != CURLE_OK) {
            int ret_code = res;
            if (ret_code > 99) {
                ret_code = -1;
            }

            std::string error_str = curl_easy_strerror(res);

            return error<response_error>{{
                .code = res,
                .message = std::move(error_str)
            }};
        }
        else {
            int64_t http_code = 0;
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
            resp.code = http_code;
        }

        connection_info.last_request.curl_error = std::string{ curl_error_buffer.begin() };

        curl_easy_getinfo(curl_handle, CURLINFO_TOTAL_TIME, &(connection_info.last_request.total_time));
        curl_easy_getinfo(curl_handle, CURLINFO_NAMELOOKUP_TIME, &(connection_info.last_request.name_lookup_time));
        curl_easy_getinfo(curl_handle, CURLINFO_CONNECT_TIME, &(connection_info.last_request.connect_time));
        curl_easy_getinfo(curl_handle, CURLINFO_APPCONNECT_TIME, &(connection_info.last_request.app_connect_time));
        curl_easy_getinfo(curl_handle, CURLINFO_PRETRANSFER_TIME, &(connection_info.last_request.pre_transfer_time));
        curl_easy_getinfo(curl_handle, CURLINFO_STARTTRANSFER_TIME, &(connection_info.last_request.start_transfer_time));
        curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_TIME, &(connection_info.last_request.redirect_time));
        curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_COUNT, &(connection_info.last_request.redirect_count));

        curl_slist_free_all(header_list);
        curl_easy_reset(curl_handle);

        return resp;
    }

}    // namespace arti::curl
