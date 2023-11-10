#pragma once

#include <mutex>
#include <thread>
#include <variant>
#include <optional>
#include <functional>
#include <unordered_map>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <error.hpp>

#include <async/schedule_policy.hpp>
#include <async/thread_pool.hpp>

namespace arti::async {

    struct context {
        using scheduled_task_id = uint16_t;
        using schedule_policy_t = std::variant<
            schedule_policy::infinity,
            schedule_policy::every,
            schedule_policy::once_after,
            schedule_policy::n_times_every
        >;

        context()
            : context(std::thread::hardware_concurrency()) { }

        context(uint8_t num_threads)
            : last_id(0)
            , pool(
                num_threads <= 2
                ? 1
                : (num_threads - 1)
            )
            , stop_flag(false) {
            spdlog::info(fmt::format("Started async context with {} threads", num_threads));
        }

        ~context() {
            if (update_thread.joinable()) {
                join();
            }
        }

        context(context &&) = delete;
        context &operator=(context &&) = delete;

        context(const context &) = delete;
        context &operator=(const context &) = delete;

        void initialize() {
            pool.initialize();
            update_thread = std::thread(std::bind_front(&context::update_loop, this));
        }

        void join() {
            stop_flag = true;
            update_thread.join();
            pool.join();
        }

        template <typename T, typename F>
        requires(
            std::is_same_v<T, schedule_policy::infinity>
            || std::is_same_v<T, schedule_policy::every>
            || std::is_same_v<T, schedule_policy::once_after>
            || std::is_same_v<T, schedule_policy::n_times_every>
        )
        void schedule(T &&policy, F &&callback) {
            last_id++;

            std::scoped_lock lock(tasks_mtx);

            if constexpr (std::is_same_v<bool, std::invoke_result_t<F>>) {
                tasks[last_id] = {
                    .id = last_id,
                    .type = std::forward<T>(policy),
                    .callback = std::forward<F>(callback),
                    .on_going_task = std::nullopt
                };
            }
            else {
                tasks[last_id] = {
                    .id = last_id,
                    .type = std::forward<T>(policy),
                    .callback = [cback = std::forward<F>(callback)] {
                        (void) cback();
                        return true;
                    },
                    .on_going_task = std::nullopt
                };
            }

            auto &ref = tasks.at(last_id);

            std::visit(schedule_policy::impl::init_visitor{}, ref.type);
        }

        template <typename TaskCallback, typename... Args>
        auto run_async(TaskCallback &&callback, Args &&...args)
            -> std::future<std::invoke_result_t<TaskCallback, Args...>> {
            return pool.run_async(
                std::forward<TaskCallback>(callback),
                std::forward<Args>(args)...
            );
        }

        template <typename TaskCallback, typename... Args>
        void run_and_ignore(TaskCallback &&callback, Args &&...args) {
            return pool.run_and_ignore(
                std::forward<TaskCallback>(callback),
                std::forward<Args>(args)...
            );
        }

      private:
        void update_loop() {
            while (not stop_flag) {
                std::list<scheduled_task_id> to_remove;

                {
                    std::scoped_lock lock(tasks_mtx);

                    for (auto &[id, task] : tasks) {
                        if (not task.on_going_task.has_value()) {
                            namespace sch_pcy = schedule_policy;
                            using sch_act = sch_pcy::impl::scheduler_action;

                            auto action = std::visit(
                                sch_pcy::impl::update_visitor{},
                                task.type
                            );

                            switch (action) {
                                case sch_act::run:
                                    task.on_going_task =
                                        pool.run_async(
                                                [](scheduled_task *tsk) {
                                                    if (not tsk->callback()) {
                                                        return sch_pcy::impl::scheduler_action::cancel;
                                                    }

                                                    return std::visit(
                                                        sch_pcy::impl::after_visitor{},
                                                        tsk->type
                                                    );
                                                },
                                                &task
                                            );
                                    break;
                                case sch_act::cancel:
                                    to_remove.push_back(id);
                                    break;
                                default: break;
                            }
                        }
                    }

                    for (auto &[id, task] : tasks) {
                        if (task.on_going_task.has_value()) {
                            auto future_status = task.on_going_task->wait_for(std::chrono::milliseconds(0));

                            if (future_status == std::future_status::ready) {
                                auto action = task.on_going_task->get();

                                switch (action) {
                                    case schedule_policy::impl::scheduler_action::cancel:
                                        to_remove.push_back(id);
                                        break;
                                    default:
                                        task.on_going_task = std::nullopt;
                                        break;
                                }
                            }
                        }
                    }

                    for (auto id : to_remove) {
                        tasks.erase(id);
                    }
                }
            }
        }

        struct scheduled_task {
            scheduled_task_id id;
            schedule_policy_t type;

            std::function<bool()> callback;

            std::optional<std::future<schedule_policy::impl::scheduler_action>> on_going_task;
        };

        scheduled_task_id last_id;

        thread_pool pool;

        std::atomic_bool stop_flag;
        std::thread update_thread;

        std::mutex tasks_mtx;
        std::unordered_map<scheduled_task_id, scheduled_task> tasks;
    };

}    // namespace arti::async
