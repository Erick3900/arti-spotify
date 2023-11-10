#pragma once

namespace arti {

    template <typename ...T>
    struct overload_set : T... {
        using T::operator()...;
    };

    template <typename ...T>
    overload_set(T...) -> overload_set<T...>;

}
