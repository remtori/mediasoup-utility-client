#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "./logger.hpp"

namespace cm {

class [[nodiscard]] Executor {
public:
    explicit Executor(size_t num_concurrency)
    {
        m_threads.reserve(num_concurrency);
        for (size_t i = 0; i < num_concurrency; i++) {
            m_threads.emplace_back(&Executor::worker, this);
        }
    }

    /**
     * Destruct the thread pool. Waits for all tasks to complete, then destroys all threads.
     */
    ~Executor()
    {
        wait_for_tasks();

        {
            const std::scoped_lock tasks_lock(m_tasks_mutex);
            m_workers_running = false;
        }
        m_task_available_cv.notify_all();

        for (auto& thread : m_threads) {
            if (thread.joinable())
                thread.join();
        }
    }

    /**
     * Push a function with zero or more arguments, but no return value, into the task queue. Does not return a future, so the user must use wait_for_tasks() or some other method to ensure that the task finishes executing, otherwise bad things will happen.
     *
     * @param task The function to push.
     * @param args The zero or more arguments to pass to the function. Note that if the task is a class member function, the first argument must be a pointer to the object, i.e. &object (or this), followed by the actual arguments.
     */
    template<typename F, typename... A>
    void push_task(F&& task, A&&... args)
    {
        auto task_fn = std::bind(std::forward<F>(task), std::forward<A>(args)...);

        {
            const std::scoped_lock tasks_lock(m_tasks_mutex);
            m_tasks.push(task_fn); // cppcheck-suppress ignoredReturnValue
        }
        m_task_available_cv.notify_one();
    }

    /**
     * Submit a function with zero or more arguments into the task queue.
     * If the function has a return value, get a future for the eventual returned value.
     * If the function has no return value, get an std::future<void> which can be used to wait until the task finishes.
     *
     * @param task The function to submit.
     * @param args The zero or more arguments to pass to the function.
     *             Note that if the task is a class member function,
     *             the first argument must be a pointer to the object,
     *             i.e. &object (or this), followed by the actual arguments.
     * @return A future to be used later to wait for the function to finish executing and/or obtain its returned value if it has one.
     */
    template<typename F, typename... A, typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>
    [[nodiscard]] std::future<R> submit(F&& task, A&&... args)
    {
        std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
        push_task(
            [task_function = std::bind(std::forward<F>(task), std::forward<A>(args)...), task_promise] {
                try {
                    if constexpr (std::is_void_v<R>) {
                        std::invoke(task_function);
                        task_promise->set_value();
                    } else {
                        task_promise->set_value(std::invoke(task_function));
                    }
                } catch (...) {
                    try {
                        task_promise->set_exception(std::current_exception());
                    } catch (...) {
                    }
                }
            });

        return task_promise->get_future();
    }

    /**
     * Wait for tasks to be completed, both those that are currently running in the threads and those that are still m_waiting in the queue.
     * Note: To wait for just one specific task, use submit() instead, and call the wait() member function of the generated future.
     */
    void wait_for_tasks()
    {
        std::unique_lock tasks_lock(m_tasks_mutex);
        m_waiting = true;
        m_tasks_done_cv.wait(tasks_lock, [this] { return !m_tasks_running && m_tasks.empty(); });
        m_waiting = false;
    }

private:
    /**
     * A worker function to be assigned to each thread in the pool.
     * Waits until it is notified by push_task() that a task is available,
     * and then retrieves the task from the queue and executes it.
     * Once the task finishes, the worker notifies wait_for_tasks() in case it is m_waiting.
     */
    void worker()
    {
        std::function<void()> task;
        while (true) {
            std::unique_lock tasks_lock(m_tasks_mutex);
            m_task_available_cv.wait(tasks_lock, [this] { return !m_tasks.empty() || !m_workers_running; });
            if (!m_workers_running)
                break;

            task = std::move(m_tasks.front());
            m_tasks.pop();

            ++m_tasks_running;
            tasks_lock.unlock();

            try {
                task();
            } catch (const std::exception& ex) {
                cm::log("[Executor][ERROR] {}", ex.what());
            } catch (const std::string& ex) {
                cm::log("[Executor][ERROR] {}", ex);
            } catch (const char* ex) {
                cm::log("[Executor][ERROR] {}", ex);
            } catch (...) {
                cm::log("[Executor][ERROR] Unknown exception");
            }

            tasks_lock.lock();
            --m_tasks_running;

            if (m_waiting && !m_tasks_running && m_tasks.empty())
                m_tasks_done_cv.notify_all();
        }
    }

private:
    std::vector<std::thread> m_threads {};
    std::condition_variable m_task_available_cv {};
    std::condition_variable m_tasks_done_cv {};
    std::queue<std::function<void()>> m_tasks {};

    size_t m_tasks_running { 0 };
    mutable std::mutex m_tasks_mutex {};

    bool m_waiting { false };
    bool m_workers_running { true };
};

}
