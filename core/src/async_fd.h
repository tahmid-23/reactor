#ifndef REACTOR_ASYNC_FD_H
#define REACTOR_ASYNC_FD_H

#include <atomic>
#include <coroutine>
#include <optional>

#include "reactor.h"
#include "task.h"

class ReadOrWriteAwaitable {
    std::atomic<int> &m_io_ready;

    std::optional<std::coroutine_handle<Task::promise_type>>& m_io_coro;

public:
    explicit ReadOrWriteAwaitable(std::atomic<int> &io_count, std::optional<std::coroutine_handle<Task::promise_type>>&);

    [[nodiscard]] constexpr bool await_ready() const noexcept {
        return m_io_ready.load() >= 0;
    }

    // TODO: race condition, await_ready returns false, data becomes available, never resumed
    constexpr void await_suspend(std::coroutine_handle<Task::promise_type> handle) const noexcept {
        m_io_coro = handle;
    }

    constexpr void await_resume() const noexcept {
        m_io_ready.fetch_sub(1);
        m_io_coro = std::nullopt;
    }
};

class Reactor;

class AsyncFd {
    std::atomic<int> m_read_count, m_write_count;

    std::optional<std::coroutine_handle<Task::promise_type>> m_read_coro{}, m_write_coro{};

    int m_fd;

public:
    explicit AsyncFd(int fd);

    ~AsyncFd();

    constexpr void set_read_ready() noexcept {
        m_read_count.fetch_add(1);
    }

    constexpr void set_write_ready() noexcept {
        m_write_count.fetch_add(1);
    }

    constexpr void mark_read() noexcept {
        m_read_count.fetch_sub(1);
    }

    constexpr void mark_written() noexcept {
        m_write_count.fetch_sub(1);
    }

    [[nodiscard]] constexpr int get() const noexcept {
        return m_fd;
    }

    ReadOrWriteAwaitable wait_read() noexcept;

    ReadOrWriteAwaitable wait_write() noexcept;

    friend void Reactor::run();

};

class RegisteredAsyncFd {
    std::shared_ptr<Reactor> m_reactor;

    AsyncFd m_fd;

public:
    RegisteredAsyncFd(std::shared_ptr<Reactor> reactor, int fd);

    ~RegisteredAsyncFd();

    AsyncFd &get();
};

#endif
