#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <reactor.h>
#include <async_fd.h>
#include <sys/socket.h>
#include <netinet/in.h>

Task example_sleep(const std::shared_ptr<Reactor> &reactor, long i) {
    std::cout << "Sleeping for " << i << " seconds!" << std::endl;
    co_await reactor->sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(i)));
    std::cout << "Returned after " << i << " seconds." << std::endl;
}

void async_sleep(const std::shared_ptr<Reactor> &reactor) {
    std::vector<Task> tasks;
    for (int i = 0; i <= 10; ++i) {
        Task task = example_sleep(reactor, i);
        tasks.push_back(std::move(task));
    }

    bool allDone;
    do {
        allDone = true;

        for (auto &task: tasks) {
            if (task.done()) {
                continue;
            }

            allDone = false;
            if (task.should_resume()) {
                task.resume();
            }
        }
    } while (!allDone);
}

Task socket_loop(RegisteredAsyncFd &async_fd) {
    char buffer[256];
    while (true) {
        co_await async_fd.get().wait_read();
        long bytes_read = recvfrom(async_fd.get().get(), buffer, sizeof(buffer) - sizeof(char), 0, nullptr, nullptr);
        buffer[bytes_read] = '\0';
        async_fd.get().mark_read();

        std::cout << buffer << std::flush;
    }
}

void async_socket(const std::shared_ptr<Reactor> &reactor) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8000);
    bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    RegisteredAsyncFd async_fd(reactor, fd);
    Task task = socket_loop(async_fd);
    while (!task.done()) {
        if (task.should_resume()) {
            task.resume();
        }
    }
}

enum ExampleType {
    SLEEP,
    SOCKET
};

constexpr ExampleType EXAMPLE_SLEEP = ExampleType::SOCKET;

int main() {
    auto reactor = create_reactor().value();
    std::thread reactor_thread = std::thread([reactor]() {
        reactor->run();
    });
    reactor_thread.detach();

    switch (EXAMPLE_SLEEP) {
        case ExampleType::SLEEP:
            async_sleep(reactor);
            break;
        case ExampleType::SOCKET:
            async_socket(reactor);
            break;
    }
}
