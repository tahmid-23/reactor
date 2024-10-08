#ifndef REACTOR_TASK_H
#define REACTOR_TASK_H

class Task {
public:
    struct promise_type {
        bool should_resume = false;

        Task get_return_object() noexcept;

        constexpr std::suspend_never initial_suspend() noexcept {
            return {};
        }

        std::suspend_always final_suspend() noexcept;

        template<typename T>
        T &&await_transform(T &&awaitable) noexcept {
            should_resume = false;
            return static_cast<T &&>(awaitable);
        }

        constexpr void unhandled_exception() const noexcept {};

        constexpr void return_void() const noexcept {}
    };

private:
    std::coroutine_handle<promise_type> m_handle;

    explicit Task(std::coroutine_handle<promise_type> handle);

public:

    ~Task();

    [[nodiscard]] bool done() const;

    [[nodiscard]] bool should_resume() const;

    void resume() const;
};

#endif
