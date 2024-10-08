#include <iostream>
#include <memory>
#include <thread>

#include <reactor.h>

Task example_sleep(std::shared_ptr<Reactor> reactor) {
    std::cout << "Sleeping for 3 seconds!" << std::endl;
    co_await reactor->sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(3L)));
    std::cout << "Done!\n";
}

int main() {
    auto reactor = create_reactor().value();
    std::thread reactor_thread = std::thread([reactor]() {
        reactor->run();
    });
    reactor_thread.detach();

    auto task = example_sleep(reactor);
    while (!task.done()) {
        if (task.should_resume()) {
            task.resume();
        }
    }
}
