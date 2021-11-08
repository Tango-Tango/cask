#include <cask/Scheduler.hpp>
#include <cask/Task.hpp>

int main(int, char **) {
    auto sched = cask::Scheduler::global();

    std::function<cask::Task<int>(const int&)> recurse = [&recurse](auto counter) {
        return cask::Task<int>::defer([counter, &recurse]() {
            if(counter > 1 * 60 * 10) {
                return cask::Task<int>::pure(counter);
            } else {
                return recurse(counter + 1).delay(100);
            }
            
        });
    };

    auto task = cask::Task<int>::pure(0).flatMap(recurse).asyncBoundary();
    task.run(sched)->await();
    return 0;
}