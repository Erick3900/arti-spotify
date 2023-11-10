#pragma once

#include <chrono>

namespace arti::async {

    namespace schedule_policy {

        namespace impl {

            enum class scheduler_action {
                run,
                wait,
                cancel
            };

            struct init_visitor;
            struct after_visitor;
            struct update_visitor;

        }    // namespace impl

        struct infinity {
            friend struct impl::init_visitor;
            friend struct impl::after_visitor;
            friend struct impl::update_visitor;

            infinity() = default;
            ~infinity() = default;

            infinity(infinity &&) = default;
            infinity &operator=(infinity &&) = default;

            infinity(const infinity &) = default;
            infinity &operator=(const infinity &) = default;

          private:
            void init() { }

            impl::scheduler_action update() {
                return impl::scheduler_action::run;
            }

            impl::scheduler_action after() {
                return impl::scheduler_action::wait;
            }
        };

        struct every {
            friend struct impl::init_visitor;
            friend struct impl::after_visitor;
            friend struct impl::update_visitor;

            std::chrono::milliseconds period;

            every(std::chrono::milliseconds period)
                : period(period) { }

            ~every() = default;

            every(every &&) = default;
            every &operator=(every &&) = default;

            every(const every &) = default;
            every &operator=(const every &) = default;

          private:
            std::chrono::time_point<std::chrono::high_resolution_clock> last_update;

            void init() {
                last_update = std::chrono::high_resolution_clock::now() - period;
            }

            impl::scheduler_action update() {
                auto now = std::chrono::high_resolution_clock::now();
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);

                if (diff >= period) {
                    return impl::scheduler_action::run;
                }

                return impl::scheduler_action::wait;
            }

            impl::scheduler_action after() {
                last_update = std::chrono::high_resolution_clock::now();
                return impl::scheduler_action::wait;
            }
        };

        struct once_after {
            friend struct impl::init_visitor;
            friend struct impl::after_visitor;
            friend struct impl::update_visitor;

            std::chrono::milliseconds period;

            once_after(std::chrono::milliseconds period)
                : period(period) { }

            ~once_after() = default;

            once_after(once_after &&) = default;
            once_after &operator=(once_after &&) = default;

            once_after(const once_after &) = default;
            once_after &operator=(const once_after &) = default;

          private:
            bool expired;
            std::chrono::time_point<std::chrono::high_resolution_clock> last_update;

            void init() {
                expired = false;
                last_update = std::chrono::high_resolution_clock::now();
            }

            impl::scheduler_action update() {
                if (expired) {
                    return impl::scheduler_action::cancel;
                }

                auto now = std::chrono::high_resolution_clock::now();
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);

                if (diff >= period) {
                    return impl::scheduler_action::run;
                }

                return impl::scheduler_action::wait;
            }

            impl::scheduler_action after() {
                return impl::scheduler_action::cancel;
            }
        };

        struct n_times_every {
            friend struct impl::init_visitor;
            friend struct impl::after_visitor;
            friend struct impl::update_visitor;

            std::size_t times;
            std::chrono::milliseconds period;

            n_times_every(std::size_t times, std::chrono::milliseconds period)
                : times(times)
                , period(period) { }

            ~n_times_every() = default;

            n_times_every(n_times_every &&) = default;
            n_times_every &operator=(n_times_every &&) = default;

            n_times_every(const n_times_every &) = default;
            n_times_every &operator=(const n_times_every &) = default;

          private:
            std::chrono::time_point<std::chrono::high_resolution_clock> last_update;

            void init() {
                last_update = std::chrono::high_resolution_clock::now() - period;
            }

            impl::scheduler_action update() {
                if (times == 0) {
                    return impl::scheduler_action::cancel;
                }

                auto now = std::chrono::high_resolution_clock::now();
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);

                if (diff >= period) {
                    return impl::scheduler_action::run;
                }

                return impl::scheduler_action::wait;
            }

            impl::scheduler_action after() {
                if (times == 0)
                    return impl::scheduler_action::cancel;

                --times;
                last_update = std::chrono::high_resolution_clock::now();

                return impl::scheduler_action::wait;
            }
        };

        namespace impl {
            struct update_visitor {
                scheduler_action operator()(infinity &p) {
                    return p.update();
                }

                scheduler_action operator()(every &p) {
                    return p.update();
                }

                scheduler_action operator()(once_after &p) {
                    return p.update();
                }

                scheduler_action operator()(n_times_every &p) {
                    return p.update();
                }
            };

            struct after_visitor {
                scheduler_action operator()(infinity &p) {
                    return p.after();
                }

                scheduler_action operator()(every &p) {
                    return p.after();
                }

                scheduler_action operator()(once_after &p) {
                    return p.after();
                }

                scheduler_action operator()(n_times_every &p) {
                    return p.after();
                }
            };

            struct init_visitor {
                void operator()(infinity &p) {
                    return p.init();
                }

                void operator()(every &p) {
                    return p.init();
                }

                void operator()(once_after &p) {
                    return p.init();
                }

                void operator()(n_times_every &p) {
                    return p.init();
                }
            };

        }    // namespace impl

    };       // namespace schedule_policy

}
