#pragma once

#include <string>
#include <expected>

namespace arti::curl {

    struct response_error {
        int code;
        std::string message;
    };

    template <typename ExpectedT = void, typename ErrorT = std::string>
    using expected = std::expected<ExpectedT, ErrorT>;

    template <typename ErrorT = std::string>
    using error = std::unexpected<ErrorT>;

}    // namespace arti::curl
