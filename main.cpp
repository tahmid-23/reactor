#include <iostream>
#include <memory>
#include <thread>

#include "reactor.h"

Task coro(std::shared_ptr<Reactor> reactor) {
    co_await reactor->sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(3L)));
}

int main() {
    auto reactor = create_reactor().value();
    std::thread reactor_thread = std::thread([reactor]() {
        reactor->run();
    });
    reactor_thread.detach();

    auto task = coro(reactor);
    while (!task.done()) {
        if (task.should_resume()) {
            task.resume();
        }
    }
}
