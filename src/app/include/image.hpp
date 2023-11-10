#pragma once

#include <string>

#include <chafa.h>

#include <fmt/ostream.h>

#include <ftxui/dom/elements.hpp>

#include <stb_image.hpp>

#include <error.hpp>

namespace arti {

    struct stbi_image_array {
        int x;
        int y;
        int channels;
        stbi_uc *data;

        stbi_image_array() 
            : data(nullptr) {
        }

        stbi_image_array(stbi_image_array &&) = default;
        stbi_image_array(const stbi_image_array &) = default;

        stbi_image_array &operator=(stbi_image_array &&) = default;
        stbi_image_array &operator=(const stbi_image_array &) = default;

        expected<> construct_from(std::string_view image_data) {
            free();

            data = stbi_load_from_memory(
                (const stbi_uc *) image_data.data(),
                image_data.size(),
                &x, &y,
                &channels,
                STBI_rgb_alpha
            );

            if (not data) {
                return error<>{ "Couldn't load image data to pixel array" };
            }

            return {};
        }

        void free() {
            if (data) {
                stbi_image_free(data);
                data = nullptr;
            }
        }

        ~stbi_image_array() {
            free();
        }
    };

    struct chafa_canvas {
        int width;
        int height;

        ChafaSymbolMap *symbol_map;
        ChafaCanvasConfig *config;
        ChafaCanvas *canvas;

        chafa_canvas()
            : width(0)
            , height(0)
            , symbol_map(nullptr)
            , config(nullptr)
            , canvas(nullptr) {}

        ~chafa_canvas() {
            free();
        }

        void construct(int w, int h, const stbi_image_array &image) {
            bool different = false;

            if (width != w) {
                width = w;
                different = true;
            }

            if (height != h) {
                height = h;
                different = true;
            }

            if (different) {
                free();

                symbol_map = chafa_symbol_map_new();
                config = chafa_canvas_config_new();

                chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_ALL);
                chafa_canvas_config_set_geometry(config, width, height);
                chafa_canvas_config_set_symbol_map(config, symbol_map);

                canvas = chafa_canvas_new(config);

                chafa_canvas_draw_all_pixels(
                    canvas,
                    CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                    image.data,
                    image.x,
                    image.y,
                    image.x * 4
                );
            }
        }

        void free() {
            if (canvas) {
                chafa_canvas_unref(canvas);
                canvas = nullptr;
            }

            if (config) {
                chafa_canvas_config_unref(config);
                config = nullptr;
            }

            if (symbol_map) {
                chafa_symbol_map_unref(symbol_map);
                symbol_map = nullptr;
            }
        }
    };

    struct image {
        image() = default;
        ~image() = default;

        image(image &&) = default;
        image &operator=(image &&) = default;

        image(const image &) = delete;
        image &operator=(const image &) = delete;

        expected<> construct(std::string image_data, int w, int h) {
            data = std::move(image_data);
            return resize(w, h);
        }

        expected<> resize(int w, int h) {
            if (cw == w and ch == h) {
                return {};
            }

            if (data.empty()) {
                return {};
            }

            cw = w;
            ch = h;

            auto constructed = pixel_array.construct_from(data);

            if (not constructed) {
                return constructed;
            }

            chafa_canvas canvas;

            canvas.construct(w, h, pixel_array);

            std::string buffer(10, 0);

            render_elements.clear();

            for (int y = 0; y < h; ++y) {
                ftxui::Elements line;

                for (int x = 0; x < w; ++x) {
                    auto pixel_wchar = chafa_canvas_get_char_at(canvas.canvas, x, y);

                    gint bg, fg;
                    chafa_canvas_get_colors_at(canvas.canvas, x, y, &fg, &bg);

                    g_unichar_to_utf8(pixel_wchar, buffer.data());

                    line.push_back(
                        ftxui::text(buffer)
                        | ftxui::color(
                            ftxui::Color(
                                (fg & 0x00ff0000) >> 16,
                                (fg & 0x0000ff00) >>  8,
                                (fg & 0x000000ff) >>  0
                            )
                        )
                        | ftxui::bgcolor(
                            ftxui::Color(
                                (bg & 0x00ff0000) >> 16,
                                (bg & 0x0000ff00) >>  8,
                                (bg & 0x000000ff) >>  0
                            )
                        )
                    );
                }

                render_elements.push_back(ftxui::hbox(std::move(line)));
            }

            return {};
        }

        ftxui::Elements render() {
            return render_elements;
        }

        void swap(image &other) {
            std::swap(data, other.data);
            std::swap(pixel_array.x, other.pixel_array.x);
            std::swap(pixel_array.y, other.pixel_array.y);
            std::swap(pixel_array.data, other.pixel_array.data);
            std::swap(pixel_array.channels, other.pixel_array.channels);
            std::swap(render_elements, other.render_elements);
        }

    private:
        int cw = 0;
        int ch = 0;
        std::string data;
        stbi_image_array pixel_array;
        ftxui::Elements render_elements;
    };

}

