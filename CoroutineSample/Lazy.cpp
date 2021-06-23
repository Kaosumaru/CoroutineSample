#include <coroutine>
#include <iostream>
#include <optional>

template<std::movable T>
class Lazy {
public:
    struct promise_type {
        Lazy<T> get_return_object() {
            return Lazy{ Handle::from_promise(*this) };
        }
        static std::suspend_always initial_suspend() noexcept {
            return {};
        }
        static std::suspend_always final_suspend() noexcept {
            return {};
        }

        void return_value(T&& value)
        {
            current_value = std::move(value);
        }

        void return_value(const T& value)
        {
            current_value = value;
        }

        // Disallow co_await in generator coroutines.
        void await_transform() = delete;
        [[noreturn]]
        static void unhandled_exception() {
            throw;
        }

        std::optional<T> current_value;
    };

    using Handle = std::coroutine_handle<promise_type>;

    explicit Lazy(const Handle coroutine) :
        m_coroutine{ coroutine }
    {}

    Lazy() = default;
    ~Lazy() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }

    Lazy(const Lazy&) = delete;
    Lazy& operator=(const Lazy&) = delete;

    Lazy(Lazy&& other) noexcept :
        m_coroutine{ other.m_coroutine }
    {
        other.m_coroutine = {};
    }
    Lazy& operator=(Lazy&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    T& Get()
    {
        if (m_coroutine && !m_coroutine.done()) {
            m_coroutine.resume();
            m_current_value = std::move(m_coroutine.promise().current_value);
            m_coroutine.destroy();
            m_coroutine = {};
        }

        return *m_current_value;
    }
   

private:
    Handle m_coroutine;
    std::optional<T> m_current_value;
};

Lazy<int> LazyValue(int i) {
    std::cout << "Computing... " << i << std::endl;
    co_return i;
}

void TestLazy()
{
    auto a = LazyValue(1);
    auto b = LazyValue(2);

    std::cout << b.Get();
    std::cout << a.Get();
}