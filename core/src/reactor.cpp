#include <algorithm>
#include <climits>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "reactor.h"

Reactor::Reactor(int epollfd, int reactor_eventfd) : m_epollfd(epollfd), m_eventfd(reactor_eventfd) {

}

Reactor::~Reactor() {
    close(m_epollfd);
    close(m_eventfd);
}

[[noreturn]] void Reactor::run() {
    int waitTime = -1;
    epoll_event events[8];
    while (true) {
        int triggered_events = epoll_wait(m_epollfd, events, sizeof(events) / sizeof(*events), waitTime);
        for (int i = 0; i < triggered_events; ++i) {
            eventfd_read(m_eventfd, nullptr);
        }

        waitTime = -1;
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
            auto sleep_time = awake_time - current_time;
            auto sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time);
            waitTime = static_cast<int>(std::clamp(sleep_ms.count(), 1000L, static_cast<long>(INT_MAX)));
            break;
        }
    }
}

SleepAwaitable Reactor::sleep_for(std::chrono::nanoseconds duration) {
    auto now = std::chrono::steady_clock::now();
    auto until = now + duration;

    return SleepAwaitable{m_mutex, m_sleep_queue, m_eventfd, until};
}

void SleepAwaitable::await_suspend(std::coroutine_handle<Task::promise_type> handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sleep_queue.emplace(m_until, handle);
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

    epoll_event eventfd_event{};
    eventfd_event.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, reactor_eventfd, &eventfd_event) == -1) {
        close(reactor_eventfd);
        close(epollfd);
        return {};
    }

    return std::make_shared<Reactor>(epollfd, reactor_eventfd);
}