#include <coroutine>

#include "task.h"

Task::Task(std::coroutine_handle<promise_type> handle) : m_handle(handle) {

}

Task::Task(Task &&other) noexcept {
    m_handle = other.m_handle;
    other.m_handle = nullptr;
}

Task::~Task() {
    if (m_handle) {
        m_handle.destroy();
    }
}

Task Task::promise_type::get_return_object() noexcept {
    return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
}

std::suspend_always Task::promise_type::final_suspend() noexcept {
    should_resume = false;
    return {};
}

bool Task::should_resume() const {
    return m_handle.promise().should_resume;
}

bool Task::done() const {
    return m_handle.done();
}

void Task::resume() const {
    m_handle.resume();
}
