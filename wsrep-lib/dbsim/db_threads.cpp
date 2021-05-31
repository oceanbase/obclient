/*
 * Copyright (C) 2019 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "db_threads.hpp"
#include "wsrep/compiler.hpp"
#include "wsrep/logger.hpp"

#include <cassert>
#include <pthread.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <ostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{
    struct ti_obj
    {
    };
    enum ti_opcode
    {
        oc_thread_create,
        oc_thread_destroy,
        oc_mutex_create,
        oc_mutex_destroy,
        oc_mutex_lock,
        oc_mutex_trylock,
        oc_mutex_unlock,
        oc_cond_create,
        oc_cond_destroy,
        oc_cond_wait,
        oc_cond_timedwait,
        oc_cond_signal,
        oc_cond_broadcast,
        oc_max // must be the last
    };

    static const char* ti_opstring(enum ti_opcode op)
    {
        switch (op)
        {
        case oc_thread_create: return "thread_create";
        case oc_thread_destroy: return "thread_destroy";
        case oc_mutex_create: return "mutex_create";
        case oc_mutex_destroy: return "mutex_destroy";
        case oc_mutex_lock: return "mutex_lock";
        case oc_mutex_trylock: return "mutex_trylock";
        case oc_mutex_unlock: return "mutex_unlock";
        case oc_cond_create: return "cond_create";
        case oc_cond_destroy: return "cond_destroy";
        case oc_cond_wait: return "cond_wait";
        case oc_cond_timedwait: return "cond_timedwait";
        case oc_cond_signal: return "cond_signal";
        case oc_cond_broadcast: return "cond_broadcast";
        default: return "unknown";
        }
    }

    static std::vector<std::string> key_vec;
    static std::atomic<int> key_cnt;
    static std::vector<std::vector<size_t>> ops_map;
    static std::vector<std::mutex*> ops_map_sync;
    static struct ops_map_sync_deleter
    {
        ~ops_map_sync_deleter()
        {
            std::for_each(ops_map_sync.begin(), ops_map_sync.end(),
                          [](auto entry) { delete entry; });
        }
    } ops_map_sync_deleter;
    static std::array<std::atomic<size_t>, oc_max> total_ops;
    static std::atomic<size_t> total_allocations;
    static std::atomic<size_t> mutex_contention;
    static std::unordered_map<std::string, size_t> mutex_contention_counts;
    static int op_level;
    // Check correct condition variable usage:
    // - Associated mutex must be locked when waiting for cond
    // - There must be at least one waiter when signalling for condition
    static bool cond_checks;
    static inline void cond_check(bool condition, const char* name,
                                  const char* message)
    {
        if (cond_checks && !condition)
        {
            wsrep::log_error() << "Condition variable check failed for '"
                               << name << "': " << message;
            ::abort();
        }
    }
    static inline int append_key(const char* name, const char* type)
    {

        key_vec.push_back(std::string(name) + "_" + type);
        wsrep::log_info() << "Register key " << name << "_" << type
                          << " with index " << (key_cnt + 1);
        ops_map.push_back(std::vector<size_t>());
        ops_map_sync.push_back(new std::mutex());
        ops_map.back().resize(oc_max);
        return ++key_cnt;
    }

    template <class Key> static inline size_t get_key_index(const Key* key)
    {
        size_t index(reinterpret_cast<const size_t>(key) - 1);
        assert(index < key_vec.size());
        return index;
    }

    template <class Key>
    static inline const char* get_key_name(const Key* key)
    {
        return key_vec[get_key_index(key)].c_str();
    }

    static inline const std::string& get_key_name_by_index(size_t index)
    {
        assert(index < key_vec.size());
        return key_vec[index];
    }

    // Note: Do not refer the obj pointer in this function, it may
    // have been deleted before the call.
    template <class Key>
    static inline void update_ops(const ti_obj* obj,
                                  const Key* key,
                                  enum ti_opcode op)
    {
        if (op_level < 1)
            return;
        total_ops[op] += 1;
        if (op_level < 2)
            return;
        if (false && op == oc_mutex_destroy)
        {
            wsrep::log_info() << "thread: " << std::this_thread::get_id()
                              << " object: " << obj
                              << ": name: " << get_key_name(key)
                              << " op: " << ti_opstring(op);
        }

        std::lock_guard<std::mutex> lock(*ops_map_sync[get_key_index(key)]);
        ops_map[get_key_index(key)][op] += 1;
    }

    struct thread_args
    {
        void* this_thread;
        void* (*fn)(void*);
        void* args;
    };

    pthread_key_t this_thread_key;
    struct this_thread_key_initializer
    {
        this_thread_key_initializer()
        {
            pthread_key_create(&this_thread_key, nullptr);
        }

        ~this_thread_key_initializer()
        {
            pthread_key_delete(this_thread_key);
        }
    };

    void* thread_start_fn(void* args_ptr);

    class ti_thread : public ti_obj
    {
    public:
        ti_thread(const wsrep::thread_service::thread_key* key)
            : key_(key)
            , th_()
            , retval_()
            , detached_()
        {
            update_ops(this, key_, oc_thread_create);
        }
        ~ti_thread()
        {
            update_ops(this, key_, oc_thread_destroy);
        }

        ti_thread(const ti_thread&) = delete;
        ti_thread& operator=(const ti_thread&) = delete;
        int run(void* (*fn)(void *), void* args)
        {
            auto ta(new thread_args{this, fn, args});
            return pthread_create(&th_, nullptr, thread_start_fn, ta);
        }

        int detach()
        {
            detached_ = true;
            return pthread_detach(th_);
        }

        int join(void** retval)
        {
            return pthread_join(th_, retval);
        }

        bool detached() const { return detached_; }

        void retval(void* retval) { retval_ = retval; }

        static ti_thread* self()
        {
            return reinterpret_cast<ti_thread*>(
                pthread_getspecific(this_thread_key));
        }

        int setschedparam(int policy, const struct sched_param* param)
        {
            return pthread_setschedparam(th_, policy, param);
        }

        int getschedparam(int* policy, struct sched_param* param)
        {
            return pthread_getschedparam(th_, policy, param);
        }

        int equal(ti_thread* other)
        {
            return pthread_equal(th_, other->th_);
        }
    private:
        const wsrep::thread_service::thread_key* key_;
        pthread_t th_;
        void* retval_;
        bool detached_;
    };

    void* thread_start_fn(void* args_ptr)
    {
        thread_args* ta(reinterpret_cast<thread_args*>(args_ptr));
        ti_thread* thread = reinterpret_cast<ti_thread*>(ta->this_thread);
        pthread_setspecific(this_thread_key, thread);
        void* (*fn)(void*) = ta->fn;
        void* args = ta->args;
        delete ta;
        void* ret((*fn)(args));
        pthread_setspecific(this_thread_key, nullptr);

        // If we end here the thread returned instead of calling
        // pthread_exit()
        if (thread->detached())
            delete thread;
        return ret;
    }

    class ti_mutex : public ti_obj
    {
    public:
        ti_mutex(const wsrep::thread_service::mutex_key* key, bool inplace)
            : mutex_(PTHREAD_MUTEX_INITIALIZER)
            , key_(key)
            , inplace_(inplace)
#ifndef NDEBUG
            , locked_()
            , owner_()
#endif // ! NDEBUG
        {
            update_ops(this, key_, oc_mutex_create);
            if (not inplace) total_allocations++;
        }

        ~ti_mutex() { update_ops(this, key_, oc_mutex_destroy); }

        ti_mutex& operator=(const ti_mutex&) = delete;
        ti_mutex(const ti_mutex&) = delete;

        int lock()
        {
            update_ops(this, key_, oc_mutex_lock);
            int ret(pthread_mutex_trylock(&mutex_));
            if (ret == EBUSY)
            {
                mutex_contention++;
                {
                    std::lock_guard<std::mutex> lock(*ops_map_sync[get_key_index(key_)]);
                    mutex_contention_counts[get_key_name(key_)] += 1;
                }
                ret = pthread_mutex_lock(&mutex_);
            }
#ifndef NDEBUG
            if (ret == 0)
            {
                assert(owner_ == std::thread::id());
                locked_ = true;
                owner_ = std::this_thread::get_id();
            }
#endif // ! NDEBUG
            return ret;
        }
        int trylock()
        {
            update_ops(this, key_, oc_mutex_trylock);
            int ret(pthread_mutex_trylock(&mutex_));
#ifndef NDEBUG
            if (ret == 0)
            {
                assert(owner_ == std::thread::id());
                locked_ = true;
                owner_ = std::this_thread::get_id();
            }
#endif // ! NDEBUG
            return ret;
        }

        int unlock()
        {
            assert(locked_);
#ifndef NDEBUG
            assert(owner_ == std::this_thread::get_id());
            owner_ = std::thread::id();
#endif // ! NDEBUG
            // Use temporary object. After mutex is unlocked it may be
            // destroyed before this update_ops() finishes.
            auto key(key_);
            int ret(pthread_mutex_unlock(&mutex_));
            update_ops(this, key, oc_mutex_unlock);
            return ret;
        }

        struct condwait_context
        {
#ifndef NDEBUG
            bool locked;
            std::thread::id owner;
#endif // ! NDEBUG
        };

        condwait_context save_for_condwait()
        {
#ifndef NDEBUG
            return condwait_context{ locked_, owner_ };
#else
            return condwait_context{};
#endif // ! NDEBUG
        }

        void reset()
        {
#ifndef NDEBUG
            locked_ = false;
            owner_ = std::thread::id();
#endif // ! NDEBUG
        }

        void restore_from_condwait(const condwait_context& ctx WSREP_UNUSED)
        {
#ifndef NDEBUG
            locked_ = ctx.locked;
            owner_ = ctx.owner;
#endif // ! NDEBUG
        }

        pthread_mutex_t* native_handle() { return &mutex_; }
        const wsrep::thread_service::mutex_key* key() const { return key_; }

        bool inplace() const { return inplace_; }
    private:
        pthread_mutex_t mutex_;
        const wsrep::thread_service::mutex_key* key_;
        const bool inplace_;
#ifndef NDEBUG
        bool locked_;
        std::atomic<std::thread::id> owner_;
#endif // ! NDEBU
    };

    class ti_cond : public ti_obj
    {
    public:
        ti_cond(const wsrep::thread_service::cond_key* key, bool inplace)
            : cond_(PTHREAD_COND_INITIALIZER)
            , key_(key)
            , inplace_(inplace)
            , waiter_()
        {
            update_ops(this, key_, oc_cond_create);
            if (not inplace) total_allocations++;
        }

        ~ti_cond() { update_ops(this, key_, oc_cond_destroy); }

        ti_cond& operator=(const ti_cond&) = delete;
        ti_cond(const ti_cond&) = delete;

        int wait(ti_mutex& mutex)
        {
            cond_check(pthread_mutex_trylock(mutex.native_handle()),
                       get_key_name(key_), "Mutex not locked in cond wait");
            waiter_ = true;
            update_ops(this, key_, oc_cond_wait);
            // update_ops(&mutex, mutex.key(), oc_mutex_unlock);
            auto condwait_ctx(mutex.save_for_condwait());
            mutex.reset();
            int ret(pthread_cond_wait(&cond_, mutex.native_handle()));
            // update_ops(&mutex, mutex.key(), oc_mutex_lock);
            mutex.restore_from_condwait(condwait_ctx);
            waiter_ = false;
            return ret;
        }

        int timedwait(ti_mutex& mutex, const struct timespec* ts)
        {
            cond_check(pthread_mutex_trylock(mutex.native_handle()),
                       get_key_name(key_), "Mutex not locked in cond wait");
            waiter_ = true;
            update_ops(this, key_, oc_cond_timedwait);
            // update_ops(&mutex, mutex.key(), oc_mutex_unlock);
            auto condwait_ctx(mutex.save_for_condwait());
            mutex.reset();
            int ret(pthread_cond_timedwait(&cond_, mutex.native_handle(), ts));
            // update_ops(&mutex, mutex.key(), oc_mutex_lock);
            mutex.restore_from_condwait(condwait_ctx);
            waiter_ = false;
            return ret;
        }

        int signal()
        {
            update_ops(this, key_, oc_cond_signal);
            cond_check(waiter_, get_key_name(key_),
                       "Signalling condition variable without waiter");
            return pthread_cond_signal(&cond_);
        }

        int broadcast()
        {
            update_ops(this, key_, oc_cond_broadcast);
            return pthread_cond_broadcast(&cond_);
        }

        bool inplace() const { return inplace_; }
    private:
        pthread_cond_t cond_;
        const wsrep::thread_service::cond_key* key_;
        const bool inplace_;
        bool waiter_;
    };
}

int db::ti::before_init()
{
    wsrep::log_info() << "db::ti::before_init()";
    return 0;
}

int db::ti::after_init()
{
    wsrep::log_info() << "db::ti::after_init()";
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//                               Thread                                     //
//////////////////////////////////////////////////////////////////////////////

const wsrep::thread_service::thread_key*
db::ti::create_thread_key(const char* name) WSREP_NOEXCEPT
{
    assert(name);
    return reinterpret_cast<const wsrep::thread_service::thread_key*>(
        append_key(name, "thread"));
}

int db::ti::create_thread(const wsrep::thread_service::thread_key* key,
                          wsrep::thread_service::thread** thread,
                          void* (*fn)(void*), void* args) WSREP_NOEXCEPT
{
    auto pit(new ti_thread(key));
    total_allocations++;
    int ret;
    if ((ret = pit->run(fn, args)))
    {
        delete pit;
    }
    else
    {
        *thread = reinterpret_cast<wsrep::thread_service::thread*>(pit);
    }
    return ret;
}

int db::ti::detach(wsrep::thread_service::thread* thread) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_thread*>(thread)->detach();
}

void db::ti::exit(wsrep::thread_service::thread* thread, void* retval) WSREP_NOEXCEPT
{
    ti_thread* th(reinterpret_cast<ti_thread*>(thread));
    th->retval(retval);
    if (th->detached())
        delete th;
    pthread_exit(retval);
}

int db::ti::equal(wsrep::thread_service::thread* thread_1,
                   wsrep::thread_service::thread* thread_2) WSREP_NOEXCEPT
{
    return (reinterpret_cast<ti_thread*>(thread_1)->equal(
                reinterpret_cast<ti_thread*>(thread_2)));
}

int db::ti::join(wsrep::thread_service::thread* thread, void** retval) WSREP_NOEXCEPT
{
    ti_thread* th(reinterpret_cast<ti_thread*>(thread));
    int ret(th->join(retval));
    if (not th->detached())
    {
        delete th;
    }
    return ret;
}

wsrep::thread_service::thread* db::ti::self() WSREP_NOEXCEPT
{
    return reinterpret_cast<wsrep::thread_service::thread*>(ti_thread::self());
}

int db::ti::setschedparam(wsrep::thread_service::thread* thread,
                          int policy, const struct sched_param* param) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_thread*>(thread)->setschedparam(policy, param);
}

int db::ti::getschedparam(wsrep::thread_service::thread* thread,
                          int* policy, struct sched_param* param) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_thread*>(thread)->getschedparam(policy, param);
}

//////////////////////////////////////////////////////////////////////////////
//                                Mutex                                     //
//////////////////////////////////////////////////////////////////////////////

const wsrep::thread_service::mutex_key*
db::ti::create_mutex_key(const char* name) WSREP_NOEXCEPT
{
    assert(name);
    return reinterpret_cast<const wsrep::thread_service::mutex_key*>(
        append_key(name, "mutex"));
}

wsrep::thread_service::mutex*
db::ti::init_mutex(const wsrep::thread_service::mutex_key* key, void* memblock,
                   size_t memblock_size) WSREP_NOEXCEPT
{
    return reinterpret_cast<wsrep::thread_service::mutex*>(
        memblock_size >= sizeof(ti_mutex) ? new (memblock) ti_mutex(key, true)
                                          : new ti_mutex(key, false));
}

int db::ti::destroy(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT
{
    ti_mutex* m(reinterpret_cast<ti_mutex*>(mutex));
    if (m->inplace())
    {
        m->~ti_mutex();
    }
    else
    {
        delete m;
    }
    return 0;
}

int db::ti::lock(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_mutex*>(mutex)->lock();
}

int db::ti::trylock(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_mutex*>(mutex)->trylock();
}

int db::ti::unlock(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_mutex*>(mutex)->unlock();
}

//////////////////////////////////////////////////////////////////////////////
//                                Cond                                      //
//////////////////////////////////////////////////////////////////////////////

const wsrep::thread_service::cond_key* db::ti::create_cond_key(const char* name) WSREP_NOEXCEPT
{
    assert(name);
    return reinterpret_cast<const wsrep::thread_service::cond_key*>(
        append_key(name, "cond"));
}

wsrep::thread_service::cond*
db::ti::init_cond(const wsrep::thread_service::cond_key* key, void* memblock,
                  size_t memblock_size) WSREP_NOEXCEPT
{
    return reinterpret_cast<wsrep::thread_service::cond*>(
        memblock_size >= sizeof(ti_cond) ? new (memblock) ti_cond(key, true)
                                         : new ti_cond(key, false));
}

int db::ti::destroy(wsrep::thread_service::cond* cond) WSREP_NOEXCEPT
{
    ti_cond* c(reinterpret_cast<ti_cond*>(cond));
    if (c->inplace())
    {
        c->~ti_cond();
    }
    else
    {
        delete c;
    }
    return 0;
}

int db::ti::wait(wsrep::thread_service::cond* cond,
                 wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_cond*>(cond)->wait(
        *reinterpret_cast<ti_mutex*>(mutex));
}

int db::ti::timedwait(wsrep::thread_service::cond* cond,
                      wsrep::thread_service::mutex* mutex,
                      const struct timespec* ts) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_cond*>(cond)->timedwait(
        *reinterpret_cast<ti_mutex*>(mutex), ts);
}

int db::ti::signal(wsrep::thread_service::cond* cond) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_cond*>(cond)->signal();
}

int db::ti::broadcast(wsrep::thread_service::cond* cond) WSREP_NOEXCEPT
{
    return reinterpret_cast<ti_cond*>(cond)->broadcast();
}

void db::ti::level(int level)
{
    ::op_level = level;
}

void db::ti::cond_checks(bool cond_checks)
{
    if (cond_checks)
        wsrep::log_info() << "Enabling condition variable checking";
    ::cond_checks = cond_checks;
}

std::string db::ti::stats()
{
    std::ostringstream os;
    os << "Totals:\n";
    for (size_t i(0); i < total_ops.size(); ++i)
    {
        if (total_ops[i] > 0)
        {
            os << "  " << ti_opstring(static_cast<enum ti_opcode>(i)) << ": "
               << total_ops[i] << "\n";
        }
    }
    os << "Total allocations: " << total_allocations << "\n";
    os << "Mutex contention: " << mutex_contention << "\n";
    for (auto i : mutex_contention_counts)
    {
        os << "  " << i.first << ": " << i.second << "\n";
    }
    os << "Per key:\n";
    std::map<std::string, std::vector<size_t>> sorted;
    for (size_t i(0); i < ops_map.size(); ++i)
    {
        sorted.insert(std::make_pair(get_key_name_by_index(i), ops_map[i]));
    }
    for (auto i : sorted)
    {
        for (size_t j(0); j < i.second.size(); ++j)
        {
            if (i.second[j])
            {
                os << "  " << i.first << ": "
                   << ti_opstring(static_cast<enum ti_opcode>(j)) << ": "
                   << i.second[j] << "\n";
            }
        }
    }
    return os.str();
}
