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
    bool stopped;
    std::optional<E> cached_error;
    MVarRef<None,None> sync_ref;

    void cancelFibers();
    Task<None,None> errorShutdown();
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
                    if (ack == Stop) {
                        self->stopped = true;
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
    return synchronize().template use<None>([](auto self) {
        self->upstream_completed = true;

        if (self->running_fibers.empty()) {
            self->stopped = true;
            return self->downstream->onComplete();
        } else {
            return Task<None,None>::none();
        }
    });
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onComplete(uint64_t id) {
    return synchronize().template use<None>([id](auto self) {
        self->running_fibers.erase(id);

        if (self->running_fibers.empty() && self->upstream_completed) {
            self->stopped = true;
            return self->downstream->onComplete();
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
    if (running_fibers.empty()) {
        stopped = true;
        if (cached_error.has_value()) {
            return downstream->onError(*cached_error);
        } else {
            return downstream->onCancel();
        }
    } else {
        cancelFibers();
        if (running_fibers.empty()) {
            stopped = true;
            if (cached_error.has_value()) {
                return downstream->onError(*cached_error);
            } else {
                return downstream->onCancel();
            }
        } else {
            return Task<None,None>::none();
        }
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

}

#endif
