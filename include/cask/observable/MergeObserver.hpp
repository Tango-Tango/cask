//          Copyright Tango Tango, Inc. 2020 - 2022.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MERGE_OBSERVER_H_
#define _CASK_MERGE_OBSERVER_H_

#include "../Observer.hpp"
#include "../Resource.hpp"
#include "../MVar.hpp"

namespace cask::observable {

template <class T, class E>
class MergeObserver final
    : public Observer<ObservableConstRef<T,E>,E>
    , public std::enable_shared_from_this<MergeObserver<T,E>>
{
public:
    MergeObserver(const std::shared_ptr<Observer<T,E>>& downstream, const std::shared_ptr<Scheduler>& sched);
    Task<Ack,None> onNext(const ObservableConstRef<T,E>& upstream) override;
    Task<Ack,None> onNext(const T& value, uint64_t id);
    Task<None,None> onError(const E& error) override;
    Task<None,None> onError(const E& error, uint64_t id);
    Task<None,None> onComplete() override;
    Task<None,None> onComplete(uint64_t id);
    Task<None,None> onCancel() override;
    Task<None,None> onCancel(uint64_t id);
private:
    std::shared_ptr<Observer<T,E>> downstream;
    std::shared_ptr<Scheduler> sched;
    std::uint64_t next_id;
    std::map<uint64_t,std::shared_ptr<Fiber<None,None>>> running_fibers;
    std::map<uint64_t,std::shared_ptr<Fiber<None,None>>> all_fibers;
    bool upstream_completed;
    bool awaiting_cancel;
    std::atomic_bool stopped;
    std::optional<E> cached_error;
    MVarRef<None,None> sync_ref;
    PromiseRef<None,None> completed_promise;

    void cancelFibers();
    Task<None,None> errorShutdown();
    Task<PromiseRef<None,None>,None> completeDownstream();
    Task<None,None> errorDownstream(const E& error);
    Task<None,None> cancelDownstream();
    Resource<std::shared_ptr<MergeObserver<T,E>>,None> synchronize();
};

template <class T, class E>
MergeObserver<T,E>::MergeObserver(const std::shared_ptr<Observer<T,E>>& downstream, const std::shared_ptr<Scheduler>& sched)
    : downstream(downstream)
    , sched(sched)
    , next_id(0)
    , running_fibers()
    , upstream_completed(false)
    , awaiting_cancel(false)
    , stopped(false)
    , cached_error()
    , sync_ref(MVar<None,None>::create(sched, None()))
    , completed_promise(Promise<None,None>::create(sched))
{}

template <class T, class E>
Task<Ack,None> MergeObserver<T,E>::onNext(const ObservableConstRef<T,E>& upstream) {
    return synchronize().template use<Ack>([upstream](auto self) {
        if (self->awaiting_cancel || self->stopped) {
            return Task<Ack,None>::pure(Stop);
        } else {
            auto id = self->next_id++;

            auto fiber = upstream->subscribeHandlers(
                self->sched,
                [id, self](auto value) { return self->onNext(value, id); },
                [id, self](auto error) { return self->onError(error, id); },
                [id, self]() { return self->onComplete(id); },
                [id, self]() { return self->onCancel(id); }
            );

            self->running_fibers[id] = fiber;
            self->all_fibers[id] = fiber;

            return Task<Ack,None>::pure(Continue);
        }
    });
}

template <class T, class E>
Task<Ack,None> MergeObserver<T,E>::onNext(const T& value, uint64_t) {
    return synchronize().template use<Ack>([value](auto self) {
        if (self->awaiting_cancel || self->stopped) {
                return Task<Ack,None>::pure(Stop);
        } else {
            return self->downstream->onNext(value)
                .template map<Ack>([self](auto ack) {
                    if (ack == Stop && !self->stopped.exchange(true)) {
                        self->completed_promise->success(None());
                    }
                    return ack;
                });
        }
    });
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onError(const E& error) {
    return synchronize().template use<None>([error](auto self) {
        self->cached_error = error;
        return self->errorShutdown();
    });
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onError(const E& error, uint64_t id) {
    return synchronize().template use<None>([id, error](auto self) {
        self->running_fibers.erase(id);
        self->cached_error = error;
        return self->errorShutdown();
    });
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onComplete() {
    return synchronize()
        .template use<PromiseRef<None,None>>([](auto self) {
            std::cout << "onComplete" << std::endl;
            self->upstream_completed = true;

            if (self->running_fibers.empty()) {
                return self->completeDownstream();
            } else {
                return Task<PromiseRef<None,None>,None>::pure(self->completed_promise);
            }
        })
        .template flatMap<None>([](auto promise) {
            return Task<None,None>::forPromise(promise);
        });
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onComplete(uint64_t id) {
    return synchronize().template use<None>([id](auto self) {
        std::cout << "onComplete(" << id << ")" << std::endl;
        self->running_fibers.erase(id);

        if (self->running_fibers.empty() && self->upstream_completed) {
            return self->completeDownstream().template map<None>([](auto){ return None(); });
        } else {
            return Task<None,None>::none();
        }
    });
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onCancel() {
    return synchronize().template use<None>([](auto self) {
        return self->errorShutdown();
    });
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onCancel(uint64_t id) {
    return synchronize().template use<None>([id](auto self) {
        self->running_fibers.erase(id);
        return self->errorShutdown();
    });
}

template <class T, class E>
void MergeObserver<T,E>::cancelFibers() {
    if (!awaiting_cancel) {
        awaiting_cancel = true;

        std::vector<uint64_t> fibers_to_erase;

        for (const auto& [id, fiber] : running_fibers) {
            fiber->cancel();

            if (fiber->isCanceled()) {
                fibers_to_erase.push_back(id);
            }
        }

        for (const auto& id : fibers_to_erase) {
            running_fibers.erase(id);
        }
    }
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::errorShutdown() {
    auto self = this->shared_from_this();

    if (running_fibers.empty()) {
        if (cached_error.has_value()) {
            return errorDownstream(*cached_error);
        } else {
            return cancelDownstream();
        }
    } else {
        cancelFibers();
        if (running_fibers.empty()) {
            if (cached_error.has_value()) {
                return errorDownstream(*cached_error);
            } else {
                return cancelDownstream();
            }
        } else {
            return Task<None,None>::none();
        }
    }
}

template <class T, class E>
Task<PromiseRef<None,None>,None> MergeObserver<T,E>::completeDownstream() {
    if (stopped.exchange(true)) {
        std::cout << "completeDownstream (skip)" << std::endl;
        return Task<PromiseRef<None,None>,None>::pure(completed_promise);
    } else {
        std::cout << "completeDownstream (transmitting)" << std::endl;
        return downstream->onComplete()
            .template map<PromiseRef<None,None>>([p = completed_promise](auto){
                p->success(None());
                return p;
            });
    }
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::errorDownstream(const E& error) {
    if (stopped.exchange(true)) {
        std::cout << "errorDownstream (skip)" << std::endl;
        return Task<None,None>::none();
    } else {
        std::cout << "errorDownstream (transmitting)" << std::endl;
        return downstream->onError(error)
            .template map<None>([p = completed_promise](auto){
                p->success(None());
                return None();
            });
    }
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::cancelDownstream() {
    if (stopped.exchange(true)) {
        std::cout << "cancelDownstream (skip)" << std::endl;
        return Task<None,None>::none();
    } else {
        std::cout << "cancelDownstream (transmitting)" << std::endl;
        return downstream->onCancel()
            .template map<None>([p = completed_promise](auto){
                p->success(None());
                return None();
            });
    }
}

template <class T, class E>
Resource<std::shared_ptr<MergeObserver<T,E>>,None> MergeObserver<T,E>::synchronize() {
    auto sync_resource = Resource<None,None>::make(
        sync_ref->take(),
        [sync_ref = sync_ref](auto v) { return sync_ref->put(v); }
    );

    return sync_resource.template map<std::shared_ptr<MergeObserver<T,E>>>(
        [self = this->shared_from_this()](const None&) {
            return self;
        }
    );
}

} // namespace cask::observable

#endif
