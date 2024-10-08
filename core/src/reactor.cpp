#include <algorithm>
#include <memory>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "async_fd.h"
#include "reactor.h"

Reactor::Reactor(
        int epollfd,
        int reactor_eventfd,
        std::unique_ptr<EpollInfo>&& eventfd_info
) : m_epollfd(epollfd),
    m_eventfd(reactor_eventfd),
    m_eventfd_info(std::move(eventfd_info)) {

}

Reactor::~Reactor() {
    close(m_epollfd);
    close(m_eventfd);
}

timespec chrono_to_ts(const std::chrono::nanoseconds &duration) {
    timespec ts{};
    ts.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    ts.tv_nsec = (duration - std::chrono::seconds(ts.tv_sec)).count();
    return ts;
}

[[noreturn]] void Reactor::run() {
    std::optional<timespec> timeout_opt = std::nullopt;
    epoll_event epoll_events[8];
    while (true) {
        timespec *timeout_ptr = nullptr;
        if (timeout_opt) {
            timeout_ptr = &timeout_opt.value();
        }

        int triggered_events = epoll_pwait2(m_epollfd, epoll_events, sizeof(epoll_events) / sizeof(*epoll_events), timeout_ptr, nullptr);
        for (int i = 0; i < triggered_events; ++i) {
            auto events = epoll_events[i].events;
            auto *info = static_cast<EpollInfo *>(epoll_events[i].data.ptr);
            switch (info->type) {
                case EpollEventType::EVENTFD:
                    eventfd_read(m_eventfd, nullptr);
                    break;
                case EpollEventType::ASYNCFD:
                    AsyncFd *fd = info->data.async_fd;

                    if (events & EPOLLIN) {
                        fd->set_read_ready();
                        auto& read_coro = fd->m_read_coro;
                        if (read_coro) {
                            read_coro.value().promise().should_resume = true;
                        }
                    }
                    if (events & EPOLLOUT) {
                        fd->set_write_ready();
                        auto& write_coro = fd->m_write_coro;
                        if (write_coro) {
                            write_coro.value().promise().should_resume = true;
                        }
                    }
                    break;
            }
        }

        timeout_opt = std::nullopt;
        while (true) {
            m_mutex.lock();
            if (m_sleep_queue.empty()) {
                m_mutex.unlock();
                break;
            }

            auto [awake_time, awake_handle] = m_sleep_queue.top();
            auto current_time = std::chrono::steady_clock::now();
            if (awake_time <= current_time) {
                m_sleep_queue.pop();
                m_mutex.unlock();
                awake_handle.promise().should_resume = true;
                continue;
            }

            m_mutex.unlock();
            timeout_opt = chrono_to_ts(awake_time - current_time);
            break;
        }
    }
}

SleepAwaitable Reactor::sleep_for(std::chrono::nanoseconds duration) {
    auto now = std::chrono::steady_clock::now();
    auto until = now + duration;

    return SleepAwaitable{m_mutex, m_sleep_queue, m_eventfd, until};
}

void Reactor::register_fd(AsyncFd *fd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto [it, _] = m_async_fds.insert({fd->get(), {fd, {} }});
    auto& epoll_info = it->second.second;
    epoll_info.type = EpollEventType::ASYNCFD;
    epoll_info.data.async_fd = fd;

    epoll_event eventfd_event{};
    eventfd_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    eventfd_event.data.ptr = &epoll_info;
    if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd->get(), &eventfd_event) == -1) {
        m_async_fds.erase(it);
    }
}

void Reactor::deregister_fd(AsyncFd *fd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_async_fds.find(fd->get());
    if (it == m_async_fds.end()) {
        return;
    }

    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd->get(), nullptr);
    m_async_fds.erase(it);
}

void SleepAwaitable::await_suspend(std::coroutine_handle<Task::promise_type> handle) const {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sleep_queue.emplace(m_until, handle);
    }
    eventfd_write(m_eventfd, 1);
}

std::optional<std::shared_ptr<Reactor>> create_reactor() {
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        return {};
    }

    int reactor_eventfd = eventfd(0, 0);
    if (reactor_eventfd == -1) {
        close(epollfd);
        return {};
    }

    std::unique_ptr<EpollInfo> eventfd_info = std::make_unique<EpollInfo>(EpollInfo{EpollEventType::EVENTFD, {}});

    epoll_event eventfd_event{};
    eventfd_event.events = EPOLLIN;
    eventfd_event.data.ptr = eventfd_info.get();
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, reactor_eventfd, &eventfd_event) == -1) {
        close(reactor_eventfd);
        close(epollfd);
        return {};
    }

    return std::make_shared<Reactor>(epollfd, reactor_eventfd, std::move(eventfd_info));
}