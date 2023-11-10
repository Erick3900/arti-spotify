#pragma once

#include <string>

#include <expected>

template <typename ExpectedT = void, typename ErrorT = std::string>
using expected = std::expected<ExpectedT, ErrorT>;

template <typename ErrorT = std::string>
using error = std::unexpected<ErrorT>;
