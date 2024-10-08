#ifndef REACTOR_REACTOR_H
#define REACTOR_REACTOR_H

#include <coroutine>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include "task.h"

class SleepAwaitable;

class Reactor {

    std::mutex m_mutex = {};

    std::priority_queue<std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>, std::coroutine_handle<Task::promise_type>>, std::vector<std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>, std::coroutine_handle<Task::promise_type>>>, std::greater<>> m_sleep_queue;

    int m_epollfd;

    int m_eventfd;

public:
    [[noreturn]] void run();

    SleepAwaitable sleep_for(std::chrono::nanoseconds duration);

public:
    explicit Reactor(int epollfd, int reactor_eventfd);

    ~Reactor();
};

class SleepAwaitable {

    std::mutex &m_mutex;

    std::priority_queue<std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>, std::coroutine_handle<Task::promise_type>>, std::vector<std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>, std::coroutine_handle<Task::promise_type>>>, std::greater<>> &m_sleep_queue;

    int m_eventfd;

    std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> m_until;

    SleepAwaitable(std::mutex &mutex,
                   std::priority_queue<std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>, std::coroutine_handle<Task::promise_type>>, std::vector<std::pair<std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>, std::coroutine_handle<Task::promise_type>>>, std::greater<>> &sleep_queue,
                   int eventfd,
                   std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> until)
            : m_mutex(mutex), m_sleep_queue(sleep_queue), m_eventfd(eventfd), m_until(until) {

    }

public:
    friend SleepAwaitable Reactor::sleep_for(std::chrono::nanoseconds duration);

    [[nodiscard]] bool await_ready() const { return std::chrono::steady_clock::now() >= m_until; }

    void await_suspend(std::coroutine_handle<Task::promise_type> handle) const;

    constexpr void await_resume() const noexcept {}
};

std::optional<std::shared_ptr<Reactor>> create_reactor();

#endif
