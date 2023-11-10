#include <arti/spotify/utils.hpp>

#include <random>
#include <ranges>
#include <algorithm>

#include <mongoose.h>

namespace arti::spotify::utils {

    std::string random_string(size_t sz) {
        std::string_view possible{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" };

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, possible.size() - 1);

        std::string ret_str(sz, 0);

        for (size_t i = 0; i < sz; ++i) {
            ret_str[i] = possible[distrib(gen)];
        }

        return ret_str;
    }

    std::unordered_map<std::string, std::string> query_string_to_map(std::string_view query_str) {
        std::unordered_map<std::string, std::string> ret;

        auto splitted = query_str | std::views::split('&');

        for (const auto &substr_rng : splitted) {
            std::string_view substr{ substr_rng.begin(), substr_rng.size() };

            auto pos = std::find(substr.begin(), substr.end(), '=');

            if (pos == substr.end()) {
                ret[std::string{ substr }] = "";
            }
            else {
                auto off = pos - substr.begin();
                auto key = substr.substr(0, off);
                auto value = substr.substr(off + 1);
                ret[std::string{ key }] = value;
            }
        }

        return ret;
    }

    std::string url_encode_string(std::string_view str) {
        std::vector<char> encoded(str.size() * 5, 0);

        auto encoded_sz = mg_url_encode(
            str.data(),
            str.size(),
            encoded.data(),
            encoded.size()
        );

        return std::string{ encoded.data(), size_t(encoded_sz) };
    }

    std::string url_decode_string(std::string_view str) {
        std::vector<char> decoded(str.size() * 5, 0);

        auto decoded_sz = mg_url_decode(
            str.data(),
            str.size(),
            decoded.data(),
            decoded.size(),
            1
        );

        return std::string{ decoded.data(), size_t(decoded_sz) };
    }

    std::string encode_base64(std::string_view str) {
        std::vector<char> encoded(str.size() * 5, 0);

        auto encoded_sz = mg_base64_encode(
            (uint8_t *) str.data(),
            str.size(),
            encoded.data(),
            encoded.size()
        );

        return std::string{ encoded.data(), size_t(encoded_sz) };
    }

    std::string decode_base64(std::string_view str) {
        std::vector<char> decoded(str.size() * 5, 0);

        auto decoded_sz = mg_base64_decode(
            str.data(),
            str.size(),
            decoded.data(),
            decoded.size()
        );

        return std::string{ decoded.data(), size_t(decoded_sz) };
    }

}
