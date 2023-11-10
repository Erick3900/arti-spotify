#pragma once

#include <mutex>
#include <functional>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <fmt/std.h>

#include <spdlog/spdlog.h>

#include <arti/spotify/client.hpp>

#include <async/context.hpp>
#include <internal/config.hpp>

#include <player_state.hpp>

static std::array<const char *, 2> play_icons{
    "\xf3\xb0\x8f\xa4",
    "\xf3\xb0\x90\x8a"
};

static std::array<const char *, 2> heart_icons{
    "\xf3\xb1\xa2\xa0",
    "\xf3\xb0\xa3\x90"
};

static std::array<const char *, 2> shuffle_icons{
    "\xf3\xb0\x92\x9e",
    "\xf3\xb0\x92\x9d",
};



static std::string rewind_icon = "\xf3\xb1\x87\xb9";
static std::string previous_icon = "\xf3\xb0\x92\xae";
static std::string next_icon = "\xf3\xb0\x92\xad";
static std::string forward_icon = "\xf3\xb1\x87\xb8";

static std::string shuffle_icon{ shuffle_icons[0] };
static std::string heart_icon{ heart_icons[0] };
static std::string play_icon{ play_icons[0] };

namespace arti::screens {

    struct player {
        enum class ui_layout {
            compact,
            vertical,
            horizontal,
        };

        expected<> initialize(async::context *context) {
            ctx = context;

            if (auto ok = api.initialize(config::cfg_path); not ok) {
                return ok;
            }

            state = std::make_shared<player_state>();

            if (auto ok = update_layout(); not ok) {
                return ok;
            }

            update_state();

            using namespace std::chrono_literals;

            ctx->schedule(
                async::schedule_policy::every(750ms),
                std::bind(&player::update_state, this)
            );

            auto custom_button = ftxui::ButtonOption{};            

            custom_button.transform = [](const ftxui::EntryState &e_state) {
                auto element = ftxui::text(" " + e_state.label + " ");

                if (e_state.focused) {
                    element |= ftxui::bold;
                }

                return element;
            };

            custom_button.animated_colors.background.Set(ftxui::Color{}, ftxui::Color::White, 0ms);
            custom_button.animated_colors.foreground.Set(ftxui::Color::White, ftxui::Color::Black, 0ms);

            controls = ftxui::Container::Horizontal({
                // Like
                ftxui::Button(
                    &heart_icon,
                    [&] {
                        std::scoped_lock lock(state_mtx);
                        ctx->run_and_ignore(
                            [](std::optional<std::string> type, bool saved, std::string id, std::atomic_bool &update_saved) {
                                if (not type) {
                                    return spdlog::error("Called like in null item");
                                }

                                auto api = spotify::client{};

                                if (saved) {
                                    auto ok = api.del(fmt::format("/me/{}?ids={}", *type, id));

                                    if (ok and ok->code == 200) {
                                        update_saved = true;
                                    }
                                }
                                else {
                                    auto ok = api.put(fmt::format("/me/{}?ids={}", *type, id), nlohmann::json::object_t{});

                                    if (ok and ok->code == 200) {
                                        update_saved = true;
                                    }
                                }
                            },
                            state->item.has_value()
                            ? std::optional<std::string>(
                                state->is_track()
                                ? "tracks"
                                : "episodes"
                            )
                            : std::nullopt,
                            saved_current,
                            std::string{ state->get_id() },
                            std::ref(update_saved)
                        );
                    },
                    custom_button
                ),
            
                // Repeat
                // ftxui::Button(
                //     &heart_icon,
                //     [&] {

                //     },
                //     custom_button
                // ),

                // Rewind
                ftxui::Button(
                    &rewind_icon,
                    [&] {
                        ctx->run_and_ignore([](std::mutex &mtx, std::shared_ptr<player_state> &stt,int32_t ms) {
                            auto api = spotify::client{};

                            if (auto ok = api.put(fmt::format("/me/player/seek?position_ms={}", ms), ""); not ok) {
                                spdlog::error("Coudlnt PUT to next");
                            }

                            std::scoped_lock lock(mtx);
                            stt->progress_ms -= 3500;
                        }, std::ref(state_mtx), std::ref(state), [&] {
                            std::scoped_lock lock(state_mtx);
                            return std::min<int32_t>(state->progress_ms - 2500, state->get_duration_ms());
                        }());
                    },
                    custom_button
                ),

                // Previous
                ftxui::Button(
                    &previous_icon,
                    [&] {
                        ctx->run_and_ignore([] {
                            auto api = spotify::client{};

                            if (auto ok = api.post("/me/player/previous", ""); not ok) {
                                spdlog::error("Coudlnt PUT to next");
                            }
                        });
                    },
                    custom_button
                ),

                // Play/Pause
                ftxui::Button(
                    &play_icon,
                    [&] {
                        std::scoped_lock lock(state_mtx);
                        ctx->run_and_ignore([] (bool active, std::optional<std::string> id) {
                            auto api = spotify::client{};

                            if (active) {
                                auto response = api.put(fmt::format(
                                    "/me/player/pause{}",
                                    id
                                    ? fmt::format("?device_id={}", *id)
                                    : ""
                                ), "");

                                if (not response) {
                                    return spdlog::error("ERROR 1");
                                }

                                if (not (response->code == 200 or response->code == 204)) {
                                    return spdlog::error(fmt::format("ERROR 2, {}", response->body.dump(2)));
                                }
                            }
                            else {
                                auto response = api.put(fmt::format(
                                    "/me/player/play{}",
                                    id
                                    ? fmt::format("?device_id={}", *id)
                                    : ""
                                ), nlohmann::json::object_t{});

                                if (not response) {
                                    return spdlog::error("ERROR 3");
                                }

                                if (not (response->code == 200 or response->code == 204)) {
                                    return spdlog::error(fmt::format("ERROR 4, {}\n{}", *id, response->body.dump(2)));
                                }
                            }
                        }, state->is_active, state->device ? std::optional{ state->device->id } : std::nullopt);
                    },
                    custom_button
                ),

                // Next
                ftxui::Button(
                    &next_icon,
                    [&] {
                        ctx->run_and_ignore([] {
                            auto api = spotify::client{};

                            if (auto ok = api.post("/me/player/next", ""); not ok) {
                                spdlog::error("POST to spotify api failed, endpoint: '/me/player/next'", ok.error());
                            }
                        });
                    },
                    custom_button
                ),

                // Forward
                ftxui::Button(
                    &forward_icon,
                    [&] {
                        ctx->run_and_ignore([](std::mutex &mtx, std::shared_ptr<player_state> &stt,int32_t ms) {
                            auto api = spotify::client{};

                            if (auto ok = api.put(fmt::format("/me/player/seek?position_ms={}", ms), ""); not ok) {
                                spdlog::error("Coudlnt PUT to next");
                            }

                            std::scoped_lock lock(mtx);
                            stt->progress_ms += 3500;
                        }, std::ref(state_mtx), std::ref(state), [&] {
                            std::scoped_lock lock(state_mtx);
                            return std::min<int32_t>(state->progress_ms + 2500, state->get_duration_ms());
                        }());
                    },
                    custom_button
                ),

                // Shuffle
                ftxui::Button(
                    &shuffle_icon,
                    [&] {
                        std::scoped_lock lock(state_mtx);
                        ctx->run_and_ignore(
                            [](spotify_state::shuffle shffl, std::optional<std::string> id) {
                                auto api = spotify::client{};

                                auto ok = api.put(
                                    fmt::format(
                                        "/me/player/shuffle?state={}{}",
                                        shffl != spotify_state::shuffle::on,
                                        id
                                        ? fmt::format("&device_id={}", *id)
                                        : ""
                                    ),
                                    nlohmann::json::object_t{}
                                );

                                if (not ok or (ok and not (ok->code == 204 or ok->code == 200))) {
                                    spdlog::error("Failed to update shuffle state");
                                }
                            },
                            state->shuffle,
                            state->device ? std::optional{ state->device->id } : std::nullopt
                        );
                    },
                    custom_button
                )
            });

            return {};
        }

        void update_state() {
            auto id = [&] -> std::string {
                std::scoped_lock lock(state_mtx);
                return std::string{ state->get_id() };
            }();

            auto response = api.get("/me/player?additional_types=track,episode");

            if (not response) {
                spdlog::error(fmt::format("Failed to update player state: {}", response.error()));
                return;
            }

            auto new_state = std::make_shared<player_state>();
            new_state->update(response.value());

            auto new_id = new_state->get_id();

            if (not new_id.empty() and new_id != id) {
                update_saved = true;

                auto images = [&]() -> std::vector<arti::spotify_state::image> * {
                    if (new_state->is_track()) {
                        return &new_state->as<spotify_state::track>().album.images;
                    }
                    else if (new_state->is_episode()) {
                        return &new_state->as<spotify_state::episode>().images;
                    }

                    return nullptr;
                }();

                if (images != nullptr and not images->empty()) {
                    auto image_url = images->front().url;

                    ctx->run_and_ignore(
                    [image_url = std::move(image_url)] (decltype(img) &img_) {
                        auto image_data = arti::curl::request::get(image_url);

                        if (not image_data) {
                            return spdlog::error(
                                fmt::format(
                                    "Failed image curl request, code: {}, message: {}",
                                    image_data.error().code,
                                    image_data.error().message
                                )
                            );
                        }

                        if (image_data->code != 200) {
                            return spdlog::error(
                                fmt::format(
                                    "Image curl request responded with code: {}",
                                    image_data->code
                                )
                            );
                        }

                        image new_image;

                        auto ok = new_image.construct(std::move(image_data->body), img_.dimx, img_.dimy);

                        if (not ok) {
                            return spdlog::error(
                                fmt::format(
                                    "Failed to construct image, '{}'",
                                    ok.error()
                                )
                            );
                        }

                        {
                            std::scoped_lock lock(img_.img_mtx);
                            img_.img.swap(new_image);
                        }
                    }, std::ref(img));
                }
            }

            if (update_saved) {
                auto saved = [&] {
                    if (new_state->is_track()) {
                        auto saved_response = api.get(fmt::format("/me/tracks/contains?ids={}", new_id));

                        if (not saved_response.has_value()) {
                            spdlog::error(fmt::format("Failed to fetch saved state of track, '{}'", saved_response.error()));
                            return false;
                        }

                        if (saved_response->code != 200) {
                            spdlog::error(fmt::format("Failed to fetch saved state of track, code {}: body: '{}'", saved_response->code, saved_response->body.dump()));
                            return false;
                        }

                        return saved_response->body[0].get<bool>();
                    }
                    else if (new_state->is_episode()) {
                        auto saved_response = api.get(fmt::format("/me/episodes/contains?ids={}", new_id));

                        if (not saved_response.has_value()) {
                            spdlog::error(fmt::format("Failed to fetch saved state of episode, '{}'", saved_response.error()));
                            return false;
                        }

                        if (saved_response->code != 200) {
                            spdlog::error(fmt::format("Failed to fetch saved state of episode, code {}: body: '{}'", saved_response->code, saved_response->body.dump()));
                            return false;
                        }

                        return saved_response->body[0].get<bool>();
                    }

                    return false;
                }();
                saved_current = saved;
                update_saved = false;
            }


            auto new_shuffle = new_state->shuffle == spotify_state::shuffle::on;
            // auto new_repeat = 0;

            std::scoped_lock lock(state_mtx);
            state.swap(new_state);

            shuffle_icon = shuffle_icons[new_shuffle];
            heart_icon = heart_icons[saved_current];
            play_icon = play_icons[state->is_active ? 0 : 1];
        }

        ftxui::Element render() {
            auto state_ptr = get_state();

            if (auto ok = update_layout(); not ok) {
                ftxui::ScreenInteractive::Active()->ExitLoopClosure()();
                return ftxui::vbox();
            }

            using namespace ftxui;
            if (not state_ptr->item.has_value()) {
                return vbox(
                    vbox(
                        text("Playback not active")
                        | bold
                    )
                    | vcenter
                    | hcenter
                    | flex_grow
                )
                | flex_grow;
            }

            if (auto ok = img.img.resize(img.dimx, img.dimy); not ok) {
                spdlog::error("Couldnt resize image");
            }

            auto progress = 
                static_cast<float>(state_ptr->progress_ms) / 
                static_cast<float>(state_ptr->get_duration_ms());

            auto progress_widget = Slider(
                "", &progress,
                0.0f, 1.0f, 0.01f
            );

            if (layout == ui_layout::compact) {
                return vbox(
                    vbox(
                        text(state_ptr->get_name().data())
                        | center
                        | bold,

                        text(state_ptr->get_artist_name().data())
                        | center,

                        controls->Render()
                        | hcenter,

                        progress_widget->Render()
                    )
                    | vcenter
                    | hcenter
                    | flex_grow
                )
                | flex_grow;
            }

            if (layout == ui_layout::horizontal) {
                return vbox(
                    hbox(
                        vbox(
                            img.img.render()
                        )
                        | size(WIDTH, Constraint::EQUAL, img.dimx)
                        | size(HEIGHT, Constraint::EQUAL, img.dimy),
                        separatorEmpty(),
                        separatorEmpty(),
                        vbox(
                            text(state_ptr->get_name().data())
                            | bold
                            | center,

                            text(state_ptr->get_artist_name().data())
                            | center,

                            separatorEmpty(),

                            controls->Render()
                            | hcenter,

                            separatorEmpty(),

                            progress_widget->Render()
                        )
                        | vcenter
                        | size(WIDTH, Constraint::EQUAL, std::min(ts.dimx - img.dimx - 5, img.dimx + 2))
                    )
                    | vcenter
                    | hcenter
                    | flex_grow
                )
                | flex_grow;
            }

            std::scoped_lock lock(img.img_mtx);
            
            return vbox(
                vbox(
                    vbox(
                        img.img.render()
                    )
                    | size(WIDTH, Constraint::EQUAL, img.dimx)
                    | size(HEIGHT, Constraint::EQUAL, img.dimy)
                    | hcenter,

                    vbox(
                        text(state_ptr->get_name().data())
                        | center
                        | bold,

                        text(state_ptr->get_album().data())
                        | center,

                        text(state_ptr->get_artist_name().data())
                        | center,

                        separatorEmpty(),

                        controls->Render()
                        | hcenter,

                        separatorEmpty(),

                        progress_widget->Render()
                    )
                    | border
                    | size(WIDTH, Constraint::EQUAL, img.dimx + 2)
                )
                | vcenter
                | hcenter
                | flex_grow
            )
            | flex_grow;
        }

        std::function<ftxui::Element()> get_render_fn() {
            return std::bind(
                &player::render,
                this
            );
        }

        std::shared_ptr<player_state> get_state() {
            std::scoped_lock lock(state_mtx);
            return state;
        }

        expected<> update_layout() {
            auto n_ts = ftxui::Terminal::Size();

            if (n_ts.dimx == ts.dimx and
                n_ts.dimy == ts.dimy) {
                return {};
            }

            ts = n_ts;
            layout = ui_layout::vertical;

            if (ts.dimx < 20) {
                return error<>{ "Terminal size too small" };
            }

            img.dimx = std::min(
                (ts.dimy - 15) * 2,
                ts.dimx
            );

            img.dimy = img.dimx / 2;

            if (ts.dimy >= 16 and
                ((ts.dimx - 26) >= ((ts.dimy - 6) * 2))) {
                layout = ui_layout::horizontal;
                img.dimx = (ts.dimy - 6) * 2;
                img.dimy = (ts.dimy - 6);
            }
            
            if (img.dimy < 10) {
                layout = ui_layout::compact;
                return {};
            }

            img.dimx = std::min(img.dimx, 70);
            img.dimy = img.dimx / 2;

            return {};
        }

        ui_layout layout;
        ftxui::Dimensions ts;

        async::context *ctx;
        
        std::mutex state_mtx;
        std::shared_ptr<player_state> state;

        spotify::client api;

        ftxui::Component controls;

        bool saved_current;
        std::atomic_bool update_saved;
        struct {
            int dimx;
            int dimy;
            image img;
            std::mutex img_mtx;
        } img;
    };

}
