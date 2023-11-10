#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace arti::spotify::utils {

    std::string random_string(size_t sz);
    std::string encode_base64(std::string_view str);
    std::string decode_base64(std::string_view str);
    std::string url_encode_string(std::string_view str);
    std::string url_decode_string(std::string_view str);
    std::unordered_map<std::string, std::string> query_string_to_map(std::string_view query_str);

}    // namespace arti::str
