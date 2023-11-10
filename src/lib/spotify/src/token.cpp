#include <arti/spotify/token.hpp>

#include <atomic>
#include <fstream>
#include <filesystem>

#include <mongoose.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <arti/curl/client.hpp>

#include <arti/spotify/utils.hpp>

namespace arti::spotify {

    struct shared_data {
        std::atomic_bool waiting;
        std::string response;
    };

    static void on_message_callback(mg_connection *conn, int ev, void *ev_data, void *fn_data);

    expected<> auth_token::set(nlohmann::json token_data) {
        std::scoped_lock lock(data_mtx);
        data = std::move(token_data);
        return {};
    }

    expected<> auth_token::read_from(std::string_view config_path) {
        namespace fs = std::filesystem;

        if (fs::exists(config_path)) {
            return error<>{ fmt::format("File '{}' does not exist", config_path) };
        }

        try {
            std::ifstream config_file{ fs::path(config_path) };

            std::scoped_lock lock(data_mtx);
            config_file >> data;
        }
        catch(std::exception &exp) {
            return error<>{ exp.what() };
        }

        return {};
	}

    // TODO: Maybe implement a coroutine based transaction of data?
    expected<> auth_token::login() {
        auto state = utils::random_string(16);

        auto login_url = [&] {
            auto url_query_params = fmt::format(
                "response_type=code&client_id={}&scope={}&redirect_uri={}&state={}",
                utils::url_encode_string(client_id),
                utils::url_encode_string(api_scope),
                utils::url_encode_string(redirect_url),
                utils::url_encode_string(state)
            );

            return fmt::format("https://accounts.spotify.com/authorize?{}", url_query_params);
        }();

        auto listen_url = [] {
            auto uri = mg_url_uri(redirect_url.c_str());

            return redirect_url.substr(0, uri - redirect_url.c_str());
        }();

        fmt::print("Login link:\n{}\n", login_url);

        shared_data sh_data;
        sh_data.waiting = true;

        bool finished = false;
        auto finished_time = std::chrono::high_resolution_clock::now();

        mg_mgr manager;
        mg_mgr_init(&manager);
        mg_http_listen(&manager, listen_url.c_str(), on_message_callback, &sh_data);

        while (true) {
            mg_mgr_poll(&manager, 1000);

            if (not finished) {
                if (not sh_data.waiting) {
                    finished = true;
                    finished_time = std::chrono::high_resolution_clock::now();
                }
            }
            else {
                auto now = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - finished_time).count();

                if (duration > 250) {
                    break;
                }
            }
        }

        mg_mgr_free(&manager);

        auto query_data = [&] {
            auto decoded_query_str = utils::url_decode_string(sh_data.response);
            return utils::query_string_to_map(decoded_query_str);
        }();

        if (query_data.contains("error")) {
            return error<>{ query_data.at("error") };
        }

        if (query_data["state"] != state) {
            return error<>{ "State mismatch" };
        }

        auto code = query_data["code"];
        auto authorization_token = fmt::format("{}:{}", client_id, client_secret);

        auto body = fmt::format(
            "code={}&grant_type=authorization_code&redirect_uri={}",
            utils::url_encode_string(code),
            utils::url_encode_string(redirect_url)
        );

        auto expected_response = curl::request::post(
            "https://accounts.spotify.com/api/token",
            "application/x-www-form-urlencoded",
            body,
            {
                {
                    "Authorization",
                    fmt::format("Basic {}", 
                        utils::encode_base64(authorization_token)
                    )
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

        auto response = std::move(expected_response).value();

        if (response.code != 200) {
            return error<>{ 
                fmt::format(
                    "Api error:\nCode {}, Response {}",
                    response.code,
                    response.body
                )
            };
        }

        auto json_response = nlohmann::json::parse(response.body);

        auto time = std::chrono::system_clock::now();

        std::scoped_lock lock(data_mtx);
        data["access_token"] = json_response["access_token"].get<std::string>();
        data["refresh_token"] = json_response["refresh_token"].get<std::string>();
        data["api_scope"] = json_response["scope"].get<std::string>();
        data["token_type"] = json_response["token_type"].get<std::string>();
        data["expires_in"] = json_response["expires_in"].get<int64_t>();
        data["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();

        return {};
	}

    expected<> auth_token::renew_token() {
        auto request_data = fmt::format(
            "grant_type=refresh_token&refresh_token={}",
            data["refresh_token"].get<std::string_view>()
        );

        auto authorization_token = fmt::format("{}:{}", client_id, client_secret);

        auto expected_response = curl::request::post(
            "https://accounts.spotify.com/api/token",
            "application/x-www-form-urlencoded",
            request_data,
            {
                {
                    "Authorization",
                    fmt::format("Basic {}", 
                        utils::encode_base64(authorization_token)
                    )
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

        auto response = std::move(expected_response).value();

        if (response.code != 200) {
            return error<>{ 
                fmt::format(
                    "Api error:\nCode {}, Response {}",
                    response.code,
                    response.body
                )
            };
        }

        auto json_response = nlohmann::json::parse(response.body);

        auto time = std::chrono::system_clock::now();

        spdlog::info("Renewed token");

        std::scoped_lock lock(data_mtx);
        data["access_token"] = json_response["access_token"].get<std::string>();
        data["expires_in"] = json_response["expires_in"].get<int64_t>();
        data["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();

        if (json_response.contains("refresh_token")) {
            data["refresh_token"] = json_response["refresh_token"].get<std::string>();
        }

        return {};
	}

    expected<std::string_view, token_error> auth_token::get() {
        {
            std::scoped_lock lock(data_mtx);

            if (not data.contains("access_token")) {
                return error<token_error>{ token_error::no_data };
            }

            auto now_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            auto request_timestamp = data["timestamp"].get<int64_t>();
            auto expiracy_time = data["expires_in"].get<int64_t>();

            if (((now_timestamp + 1) - request_timestamp) < expiracy_time) {
                return data["access_token"].get<std::string_view>();
            }
        }

        auto expected_response = renew_token();

        if (not expected_response) {
            return error<token_error>{ token_error::renew };
        }

        std::scoped_lock lock(data_mtx);
        return data["access_token"].get<std::string_view>();
	}

    void on_message_callback(mg_connection *conn, int ev, void *ev_data, void *fn_data) {
        if (ev == MG_EV_HTTP_MSG) {
            auto http_message = static_cast<mg_http_message *>(ev_data);
            auto sh_data = static_cast<shared_data *>(fn_data);

            if (mg_http_match_uri(http_message, "/callback")) {
                if (sh_data->waiting) {
                    sh_data->response = std::string(http_message->query.ptr, http_message->query.len);

                    mg_http_reply(conn, 200, "Content-Type: text/plain\r\n", "%s\n", "Ok, you can close this page now");

                    sh_data->waiting = false;
                }
            }
        }
    }

    const nlohmann::json &auth_token::get_data() const {
        return data;
    }

}
