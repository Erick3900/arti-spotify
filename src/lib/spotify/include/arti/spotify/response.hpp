#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

namespace arti::spotify {

    struct api_response {
        int code;
        nlohmann::json body;
    };

}
