#include <algorithm>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "reactor.h"

constexpr int EVENT_RECEIVED = 1;

Reactor::Reactor(int epollfd, int reactor_eventfd) : m_epollfd(epollfd), m_eventfd(reactor_eventfd) {

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
    epoll_event events[8];
    while (true) {
        timespec *timeout_ptr = nullptr;
        if (timeout_opt) {
            timeout_ptr = &timeout_opt.value();
        }

        int triggered_events = epoll_pwait2(m_epollfd, events, sizeof(events) / sizeof(*events), timeout_ptr, nullptr);
        for (int i = 0; i < triggered_events; ++i) {
            switch (events[i].data.u32) {
                case EVENT_RECEIVED:
                    eventfd_read(m_eventfd, nullptr);
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

void SleepAwaitable::await_suspend(std::coroutine_handle<Task::promise_type> handle) const {
    m_mutex.lock();
    m_sleep_queue.emplace(m_until, handle);
    m_mutex.unlock();
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
    eventfd_event.data.u32 = EVENT_RECEIVED;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, reactor_eventfd, &eventfd_event) == -1) {
        close(reactor_eventfd);
        close(epollfd);
        return {};
    }

    return std::make_shared<Reactor>(epollfd, reactor_eventfd);
}