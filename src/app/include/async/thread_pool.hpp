#pragma once

#include <list>
#include <mutex>
#include <queue>
#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <condition_variable>

#include <error.hpp>

namespace arti::async {

    struct thread_pool {
        thread_pool()
            : thread_pool(std::thread::hardware_concurrency()) { }

        explicit thread_pool(uint8_t num_threads)
            : size(num_threads)
            , stop_requested(false) { }

        ~thread_pool() {
            if (not stop_requested) {
                join();
            }
        }

        thread_pool(thread_pool &&) = delete;
        thread_pool &operator=(thread_pool &&) = delete;

        thread_pool(const thread_pool &) = delete;
        thread_pool &operator=(const thread_pool &) = delete;

        void initialize() {
            for (uint8_t i = 0; i < size; ++i) {
                pool.emplace_back(std::bind_front(&thread_pool::scheduler_loop, this));
            }
        }

        void join() {
            {
                std::unique_lock lock(tasks_mtx);
                stop_requested = true;
            }

            tasks_cndtn.notify_all();

            for (auto &worker : pool) {
                worker.join();
            }

            pool.clear();
        }

        template <typename TaskCallback, typename... Args>
        auto run_async(TaskCallback &&callback, Args &&...args)
            -> std::future<std::invoke_result_t<TaskCallback, Args...>> {
            using return_t = std::invoke_result_t<TaskCallback, Args...>;

            auto task = std::make_shared<std::packaged_task<return_t()>>(
                std::bind(
                    std::forward<TaskCallback>(callback),
                    std::forward<Args>(args)...
                )
            );

            std::future<return_t> task_future = task->get_future();

            {
                std::unique_lock lck(tasks_mtx);
                tasks.emplace([task]() { (*task)(); });
            }

            tasks_cndtn.notify_one();

            return task_future;
        }

        template <typename TaskCallback, typename... Args>
        void run_and_ignore(TaskCallback &&callback, Args &&...args) {
            using return_t = std::invoke_result_t<TaskCallback, Args...>;

            auto task = std::make_shared<std::packaged_task<return_t()>>(
                std::bind(
                    std::forward<TaskCallback>(callback),
                    std::forward<Args>(args)...
                )
            );

            {
                std::unique_lock lck(tasks_mtx);
                tasks.emplace([task]() { (*task)(); });
            }

            tasks_cndtn.notify_one();
        }

      private:
        void scheduler_loop() {
            std::function<void()> task;

            while (true) {
                {
                    std::unique_lock lck(tasks_mtx);

                    tasks_cndtn.wait(lck, [&] {
                        return stop_requested || !tasks.empty();
                    });

                    if (stop_requested && tasks.empty()) {
                        break;
                    }

                    task = tasks.front();
                    tasks.pop();
                }

                task();
            }
        }

        uint8_t size;

        std::mutex tasks_mtx;
        std::condition_variable tasks_cndtn;

        std::atomic_bool stop_requested;

        std::list<std::thread> pool;
        std::queue<std::function<void()>> tasks;
    };

}    // namespace arti::async
