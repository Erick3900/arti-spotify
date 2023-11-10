#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/flexbox_config.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/color_info.hpp>
#include <ftxui/screen/terminal.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>

#include <arti/spotify/client.hpp>

#include <async/context.hpp>

#include <player_state.hpp>

#include <screens/player.hpp>

int main() {
    namespace async = arti::async;
    namespace spt = arti::spotify;
    namespace fs = std::filesystem;

    auto daily_logger = spdlog::daily_logger_mt("daily_logger", "logs", 23, 59);
    daily_logger->set_pattern("[%H:%M:%S.%e]<%L> %v");
    daily_logger->flush_on(spdlog::level::trace);
    daily_logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(daily_logger);

    async::context ctx{ 6 };

    ctx.initialize();

    arti::screens::player player_screen;

    if (auto ok = player_screen.initialize(&ctx); not ok) {
        spdlog::error(fmt::format("Failed to initialize player screen: '{}'", ok.error()));
        return -1;
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    int tab_index = 0;

    auto player_renderer = ftxui::Renderer(player_screen.controls, player_screen.get_render_fn());

    auto explore_renderer = ftxui::Renderer([&] {
        using namespace ftxui;

        return vbox();
    });

    auto library_renderer = ftxui::Renderer([&] {
        using namespace ftxui;

        return vbox();
    });

    std::vector<std::shared_ptr<ftxui::ComponentBase>> screens{
        player_renderer,
        explore_renderer,
        library_renderer
    };

    std::vector<std::string> screens_titles{
        " \uf144 Player  ",
        " \uf002 Search  ",
        " \ueb9c Library "
    };

    auto tab_selection = ftxui::Menu(
        &screens_titles,
        &tab_index,
        ftxui::MenuOption::HorizontalAnimated()
    );

    auto main_content = ftxui::Container::Tab(screens, &tab_index);

    auto main_container = ftxui::Container::Vertical({
        tab_selection | ftxui::hcenter,
        main_content
    });

    auto main_renderer = ftxui::Renderer(main_container, [&] {
        using namespace ftxui;

        return hbox(
            vbox(
                text("\uf1bc arti::Spotify") | center | bold,
                separatorEmpty(),
                main_container->Render() | flex_grow
            ) | flex_grow | borderEmpty
        ) | flex_grow;
    });

    std::atomic<bool> refresh_ui_continue = true;

    spt::client api;

    auto with_input = ftxui::CatchEvent(
        main_renderer,
        [&](ftxui::Event event) -> bool {
            if (event.is_character()) {
                switch (event.character()[0]) {
                    case 'q':
                        screen.ExitLoopClosure()();
                        return true;
                }
            }

            return false;
        }
    );

    ctx.schedule(
        async::schedule_policy::every(std::chrono::milliseconds(250)),
        [&] {
            screen.PostEvent(ftxui::Event::Custom);
            return refresh_ui_continue.load();
        }
    );

    screen.Loop(with_input);
    refresh_ui_continue = false;

    return 0;
}
