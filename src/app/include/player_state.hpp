#pragma once

#include <optional>
#include <string>
#include <variant>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <arti/spotify/client.hpp>

#include <overload_set.hpp>

#include <image.hpp>

namespace arti {

    namespace spotify_state {

        struct device {
            int volume;
            std::string id;
            std::string name;
            std::string type;
        };

        struct context {
            std::string type;
            std::string href;
            std::string uri;
        };

        struct image {
            int32_t width;
            int32_t height;
            std::string url;
        };

        struct album {
            int32_t total_tracks;
            std::string id;
            std::string uri;
            std::string href;
            std::string type;
            std::string name;
            std::string label;
            std::vector<std::string> genres;
            std::vector<image> images;
        };

        struct artist {
            std::string id;
            std::string uri;
            std::string href;
            std::string name;
            std::vector<std::string> genres;
            std::vector<image> images;
        };

        struct show {
            bool is_explicit;
            int32_t total_episodes;
            std::string id;
            std::string uri;
            std::string href;
            std::string name;
            std::string publisher;
            std::string description;
            std::vector<image> images;
        };

        struct track {
            bool is_explicit;
            int32_t duration_ms;

            std::string id;
            std::string uri;
            std::string href;
            std::string name;

            spotify_state::album album;
            std::vector<artist> artists;
        };

        struct episode {
            bool is_explicit;
            int32_t duration_ms;

            struct {
                bool fully_played;
                int32_t resume_position_ms;
            } resume_point;

            std::string id;
            std::string uri;
            std::string href;
            std::string name;
            std::string description;

            spotify_state::show show;
            std::vector<image> images;
        };

        enum class repeat {
            off,
            track,
            context
        };

        enum class shuffle {
            off,
            on
        };

        enum class playing_type {
            unknown,
            track,
            episode
        };
    }    // namespace spotify_state

    struct player_state {
        void update(const spotify::api_response &api_response) {
            if (api_response.code != 200 and api_response.code != 204 and api_response.code != 202) {
                spdlog::error(fmt::format("Unexpected response from API: {}", api_response.body.dump(4)));
                return;
            }

            if (api_response.code == 204) {
                is_active = false;
                item = std::nullopt;
                device = std::nullopt;
                context = std::nullopt;
                return;
            }

            is_active = api_response.body.at("is_playing");
            timestamp = api_response.body.at("timestamp");
            progress_ms = api_response.body.at("progress_ms");
            shuffle = api_response.body.at("shuffle_state") ? spotify_state::shuffle::on : spotify_state::shuffle::off;

            repeat = [&] {
                auto repeat_str = api_response.body.at("repeat_state").get<std::string_view>();

                if (repeat_str == "track") {
                    return spotify_state::repeat::track;
                }

                if (repeat_str == "context") {
                    return spotify_state::repeat::context;
                }

                return spotify_state::repeat::off;
            }();

            play_type = [&] {
                auto play_type_str = api_response.body.at("currently_playing_type").get<std::string_view>();

                if (play_type_str == "track") {
                    return spotify_state::playing_type::track;
                }

                if (play_type_str == "episode") {
                    return spotify_state::playing_type::episode;
                }

                return spotify_state::playing_type::unknown;
            }();

            if (not api_response.body.at("device").is_null()) {
                auto device_obj = api_response.body.at("device");

                device = spotify_state::device{ .volume = device_obj.at("volume_percent"),
                                                .id = device_obj.at("id"),
                                                .name = device_obj.at("name"),
                                                .type = device_obj.at("type") };
            }
            else
                device = std::nullopt;

            if (not api_response.body.at("context").is_null()) {
                auto context_obj = api_response.body.at("context");

                context = spotify_state::context{ .type = context_obj.at("type"),
                                                  .href = context_obj.at("href"),
                                                  .uri = context_obj.at("uri") };
            }
            else
                context = std::nullopt;

            auto item_obj = api_response.body.at("item");

            if (play_type == spotify_state::playing_type::track and not item_obj.is_null()) {
                item = spotify_state::track{
                    .is_explicit = item_obj.at("explicit"),
                    .duration_ms = item_obj.at("duration_ms"),
                    .id = item_obj.at("id"),
                    .uri = item_obj.at("uri"),
                    .href = item_obj.at("href"),
                    .name = item_obj.at("name"),
                    .album = [&]() -> spotify_state::album {
                        auto album_obj = item_obj.at("album");

                        return spotify_state::album{ .total_tracks = album_obj.at("total_tracks"),
                                                     .id = album_obj.at("id"),
                                                     .uri = album_obj.at("uri"),
                                                     .href = album_obj.at("href"),
                                                     .type = album_obj.at("album_type"),
                                                     .name = album_obj.at("name"),
                                                     .label = album_obj.value("label", ""),
                                                     .genres =
                                                         [&] {
                                                             if (not album_obj.contains("genres")) {
                                                                 return std::vector<std::string>{};
                                                             }

                                                             std::vector<std::string> genres;

                                                             for (auto genre : album_obj.at("genres")) {
                                                                 genres.push_back(genre.get<std::string>());
                                                             }

                                                             return genres;
                                                         }(),
                                                     .images =
                                                         [&] {
                                                             if (not album_obj.contains("images")) {
                                                                 return std::vector<spotify_state::image>{};
                                                             }

                                                             std::vector<spotify_state::image> images;

                                                             for (auto img : album_obj.at("images")) {
                                                                 images.push_back(
                                                                     spotify_state::image{ .width = img.at("width"),
                                                                                           .height = img.at("height"),
                                                                                           .url = img.at("url") });
                                                             }

                                                             return images;
                                                         }() };
                    }(),
                    .artists =
                        [&] {
                            std::vector<spotify_state::artist> artists;

                            for (auto artist : item_obj.at("artists")) {
                                artists.push_back(spotify_state::artist{
                                    .id = artist.at("id"),
                                    .uri = artist.at("uri"),
                                    .href = artist.at("href"),
                                    .name = artist.at("name"),
                                    .genres =
                                        [&] {
                                            if (not artist.contains("genres")) {
                                                return std::vector<std::string>{};
                                            }

                                            std::vector<std::string> genres;

                                            for (auto genre : artist.at("genres")) {
                                                genres.push_back(genre.get<std::string>());
                                            }

                                            return genres;
                                        }(),
                                    .images =
                                        [&] {
                                            if (not artist.contains("images")) {
                                                return std::vector<spotify_state::image>{};
                                            }

                                            std::vector<spotify_state::image> images;

                                            for (auto img : artist.at("images")) {
                                                images.push_back(spotify_state::image{ .width = img.at("width"),
                                                                                       .height = img.at("height"),
                                                                                       .url = img.at("url") });
                                            }

                                            return images;
                                        }() });
                            }

                            return artists;
                        }()
                };
            }
            else if (play_type == spotify_state::playing_type::episode and not item_obj.is_null()) {
                item = spotify_state::episode{
                    .is_explicit = item_obj.at("explicit"),
                    .duration_ms = item_obj.at("duration_ms"),

                    .resume_point = {
                        .fully_played = item_obj.at("resume_point").at("fully_played"),
                        .resume_position_ms = item_obj.at("resume_point").at("resume_position_ms"),
                    },

                    .id = item_obj.at("id"),
                    .uri = item_obj.at("uri"),
                    .href = item_obj.at("href"),
                    .name = item_obj.at("name"),
                    .description = item_obj.at("description"),

                    .show = [&]() -> spotify_state::show{
                        auto show_obj = item_obj.at("show");

                        return spotify_state::show{
                            .is_explicit = show_obj.at("explicit"),
                            .total_episodes = show_obj.at("total_episodes"),

                            .id = show_obj.at("id"),
                            .uri = show_obj.at("uri"),
                            .href = show_obj.at("href"),
                            .name = show_obj.at("name"),
                            .publisher = show_obj.at("publisher"),
                            .description = show_obj.at("description"),

                            .images = [&] {
                                if (not show_obj.contains("images")) {
                                    return std::vector<spotify_state::image>{};
                                }

                                std::vector<spotify_state::image> images;

                                for (auto img : show_obj.at("images")) {
                                    images.push_back(spotify_state::image{
                                        .width = img.at("width"),
                                        .height = img.at("height"),
                                        .url = img.at("url")
                                    });
                                }

                                return images;
                            }()
                        };
                    }(),
                    .images = [&] {
                        if (not item_obj.contains("images")) {
                            return std::vector<spotify_state::image>{};
                        }

                        std::vector<spotify_state::image> images;

                        for (auto img : item_obj.at("images")) {
                            images.push_back(spotify_state::image{
                                .width = img.at("width"),
                                .height = img.at("height"),
                                .url = img.at("url")
                            });
                        }

                        return images;
                    }()
                };
            }
            else
                item = std::nullopt;
        }

        std::string_view get_id() {
            if (not item.has_value()) {
                return "";
            }

            return std::visit(overload_set{ [](spotify_state::track &i) -> std::string_view { return i.id; },
                                            [](spotify_state::episode &i) -> std::string_view { return i.id; } },
                              item.value());
        }

        std::string_view get_name() {
            if (not item.has_value()) {
                return "";
            }

            return std::visit(overload_set{ [](spotify_state::track &i) -> std::string_view { return i.name; },
                                            [](spotify_state::episode &i) -> std::string_view { return i.name; } },
                              item.value());
        }

        std::string get_artist_name() {
            if (not item.has_value()) {
                return "";
            }

            return std::visit(
                overload_set{ [](spotify_state::track &i) -> std::string {
                                 return fmt::format(
                                     "{}",
                                     fmt::join(i.artists | std::views::transform(
                                                               [](const spotify_state::artist &a) { return a.name; }),
                                               ", "));
                             },
                              [](spotify_state::episode &i) -> std::string { return i.show.name; } },
                item.value());
        }

        std::string_view get_album() {
            if (not item.has_value()) {
                return "";
            }

            return std::visit(
                overload_set{ [](spotify_state::track &i) -> std::string_view { return i.album.name; },
                              [](spotify_state::episode &i) -> std::string_view { return i.show.publisher; } },
                item.value());
        }

        int32_t get_duration_ms() {
            if (not item.has_value()) {
                return std::numeric_limits<int32_t>::max();
            }

            return std::visit(overload_set{ [](spotify_state::track &i) { return i.duration_ms; },
                                            [](spotify_state::episode &i) { return i.duration_ms; } },
                              item.value());
        }

        bool is_track() {
            return std::holds_alternative<spotify_state::track>(item.value());
        }

        bool is_episode() {
            return std::holds_alternative<spotify_state::episode>(item.value());
        }

        template <typename T>
        requires(std::is_same_v<spotify_state::track, T> || std::is_same_v<spotify_state::episode, T>)
        T &as() {
            return std::get<T>(item.value());
        }

        bool is_active;

        spotify_state::repeat repeat;
        spotify_state::shuffle shuffle;
        spotify_state::playing_type play_type;

        int32_t timestamp;
        int64_t progress_ms;

        std::optional<spotify_state::device> device;
        std::optional<spotify_state::context> context;
        std::optional<std::variant<spotify_state::track, spotify_state::episode>> item;
    };
}    // namespace arti
