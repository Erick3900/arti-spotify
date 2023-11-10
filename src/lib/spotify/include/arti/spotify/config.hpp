#pragma once

#include <string>
#include <expected>

namespace arti::spotify {

    extern std::string client_id;
    extern std::string client_secret;

    extern std::string api_scope;
    extern std::string redirect_url;

    template <typename ExpectedT = void, typename ErrorT = std::string>
    using expected = std::expected<ExpectedT, ErrorT>;

    template <typename ErrorT = std::string>
    using error = std::unexpected<ErrorT>;


}
