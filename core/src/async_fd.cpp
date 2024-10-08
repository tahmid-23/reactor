#include <unistd.h>

#include "async_fd.h"

ReadOrWriteAwaitable::ReadOrWriteAwaitable(
        std::atomic<int> &io_count,
        std::optional<std::coroutine_handle<Task::promise_type>> &io_coro
) : m_io_ready(io_count), m_io_coro(io_coro) {

}

AsyncFd::AsyncFd(int fd) : m_fd(fd) {

}

AsyncFd::~AsyncFd() {
    close(m_fd);
}

ReadOrWriteAwaitable AsyncFd::wait_read() noexcept {
    return ReadOrWriteAwaitable(m_read_count, m_read_coro);
}

ReadOrWriteAwaitable AsyncFd::wait_write() noexcept {
    return ReadOrWriteAwaitable(m_write_count, m_write_coro);
}

RegisteredAsyncFd::RegisteredAsyncFd(
        std::shared_ptr<Reactor> reactor,
        int fd
) : m_reactor(std::move(reactor)), m_fd(fd) {
    m_reactor->register_fd(&m_fd);
}

RegisteredAsyncFd::~RegisteredAsyncFd() {
    m_reactor->deregister_fd(&m_fd);
}

AsyncFd &RegisteredAsyncFd::get() {
    return m_fd;
}
