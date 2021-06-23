#include "Task.h"
#include <coroutine>
#include <iostream>
#include <optional>
#include <future>
#include <functional>
#include <chrono>


auto wait_for(std::chrono::duration<double> duration) {
    struct awaitable {
        awaitable(std::chrono::duration<double> d) : duration(d) {}
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            std::thread thread([=] { std::this_thread::sleep_for(duration);  h.resume(); });
            thread.detach();
        }
        void await_resume() {}

        std::chrono::duration<double> duration;
    };
    return awaitable{ duration };
}

template<std::movable T>
class Task
{
public:
    struct promise_type {
        Task<T> get_return_object() {
            return Task{ Handle::from_promise(*this) };
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            if (onFinalSuspend) onFinalSuspend();
            return {};
        }

        void return_value(T&& value)
        {
            promise.set_value(std::move(value));
        }

        void return_value(const T& value)
        {
            promise.set_value(value);
        }

        //// Disallow co_await in generator coroutines.
        //void await_transform() = delete;
        [[noreturn]]
        static void unhandled_exception() {
            throw;
        }

        std::promise<T> promise;
        std::function<void(void)> onFinalSuspend;
    };

    using Handle = std::coroutine_handle<promise_type>;

    struct awaitable {
        explicit awaitable(const Handle coroutine) :
            m_coroutine{ coroutine }
        {}

        awaitable(const awaitable&) = delete;
        awaitable& operator=(const awaitable&) = delete;

        awaitable(awaitable&& other) noexcept :
            m_coroutine{ other.m_coroutine }
        {
            other.m_coroutine = {};
        }

        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            m_coroutine.promise().onFinalSuspend = [h]() { h.resume(); };
            m_coroutine.resume();
        }
        T&& await_resume() 
        {
            auto& promise = m_coroutine.promise().promise;
            _value = promise.get_future().get();
            m_coroutine.destroy();
            return std::move(*_value);
        }

        Handle m_coroutine;
        std::optional<T> _value;
    };


    

    explicit Task(const Handle coroutine) :
        m_coroutine{ coroutine }
    {}

    Task() = default;
    ~Task() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept :
        m_coroutine{ other.m_coroutine }
    {
        other.m_coroutine = {};
    }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    T&& Join()
    {
        m_coroutine.resume();
        auto& promise = m_coroutine.promise().promise;
        _value = promise.get_future().get();
        DestroyCoroutine();
        return std::move(*_value);
    }

    awaitable operator co_await()
    {
        auto c = m_coroutine;
        m_coroutine = {};
        return awaitable(c);
    }

private:
    void DestroyCoroutine()
    {
        if (!m_coroutine) return;
        m_coroutine.destroy();
        m_coroutine = {};
    }

    std::optional<T> _value;
    Handle m_coroutine;
};

Task<int> AsyncWork(int i) {
    using namespace std::chrono_literals;

    std::cout << "AsyncWork Start" << std::endl;
    for (int x = 0; x < i; x++) {
        co_await wait_for(1000ms);
        std::cout << "AsyncWork step " << x << std::endl;
    }
    
    co_return i;
}

Task<int> AsyncTask(int i) {
    std::cout << "Computing... " << i << std::endl;
    auto res = co_await AsyncWork(3);
    co_return i + res;
}

void TestTask() 
{
    auto a = AsyncTask(1);
    std::cout << a.Join();
}