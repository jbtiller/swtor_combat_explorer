// -*- fil-column: 120; indent-tabs-mode: nil -*-
#pragma once

#include <coroutine>
#include <optional>
 
/**
 * Generator produces a new value in some sequence each time it's called
 *
 * A generator object encapsulates a coroutine handle necessary to suspend and resume a function that generates a new
 * value each time it's called. It uses a custom iterator that resumes the coroutine when they're accessed. 
 *
 * Example: Reading from a file using a range-for loop.
 *
 * \code{.cpp}
 * auto file_reader(std::ifstream& ifs) -> Generator<std::string> {
 *     std::string line;
 *     getline(ifs, line);
 *     while(ifs.good()) {
 *         co_yield line;
 *         getline(ifs, line);
 *     }
 * }
 *
 * // ...
 * 
 * for (auto l : file_reader(std::ifstream("coro_generator.cpp"))) {
 *     std::cout << l << std::endl;
 * }
 * 
 * // ...
 * 
 * auto gen = file_reader(std::ifstream("a_cool_file.txt"));
 * for (auto it = gen.begin(); it != gen.end(); it++) {
 *     std::cout << *it << std::endl;
 * }
 * \endcode
 * 
 *
 * This is the standard pattern:
 *
 * <ol>
 * <li> Coroutine returns Generator<T>.
 * <li> Caller usese 'begin()' to get an iterator. This triggers the resumption of the coroutine.
 * <li> Coroutine uses co_yield T to produce next value.
 * <li> Caller uses 'operator*' to retrieve the most recently produced value.
 * <li> Caller uses 'operator++' to resume the coroutine and produce the next value.
 * <li> Caller uses 'end()' to check if the iteration is complete.
 * <li> The last three steps are looped until the coroutine detects the end of the sequence.
 * <li> When coroutine exhausts all values, it runs off the end evaluating 'operator==' returns true.
 * <li> Coroutine ends and caller stops iterating.
 * <li> Generator goes out of scope and cleans up coroutine handle.
 * </ol>
 *
 * Lifecycle:
 *
 * <ol>
 * <li> The first call of the coroutine returns the Generator object that encapsulates the coroutine handle.
 * <li> The client calls begin() which triggers the coroutine to run to the first co_yield. The client gets back an
 *      iterator object.
 * <li> Evaluations of co_yield store the expression result in the promise.
 * <li> Calls to operator*() do not trigger the coroutine but only return the cached value in the promise.
 * <li> Calls to operator++ resume the coroutine but have no return value.
 * <li> The coroutine should ONLY use co_yield (never co_await).
 * <li> When co_yield is encountered, the expression is evaluated, the coroutine is suspended, and returns to the caller.
 * <li> The caller checks for the end of iteration by using operator==(end()).
 * <li> When the coroutine runs off the end, the sentinel value is produced (equivalent to .end()), iteration stops,
 *      and the coroutine is cleaned up, invalidating the handle.
 * </ol>
 *
 * @note A more robust implementation is provided in c++23's std::generator<T>, but that requires gcc-14 and I only have
 * 13.
 */
template<std::movable T>
class Generator {
public:
    // This allows the coroutine co_yield value to be cached in the
    // promise.
    struct promise_type {
        auto get_return_object() -> Generator<T> {
            return Generator{Handle::from_promise(*this)};
        }
        static auto initial_suspend() noexcept -> std::suspend_always {
            return {};
        }
        static auto final_suspend() noexcept -> std::suspend_always {
            return {};
        }
        // Cache the value from the coroutine.
        auto yield_value(T value) noexcept -> std::suspend_always {
            m_current_value = std::move(value);
            return {};
        }
        // Disallow co_await in generator coroutines.
        void await_transform() = delete;
        // Pass exceptions to the caller.
        [[noreturn]] static auto unhandled_exception() ->  void {
            throw;
        }
 
        // Until the coroutine's co_yield, there's no value.
        std::optional<T> m_current_value;
    };
 
    using Handle = std::coroutine_handle<promise_type>;
 
    explicit Generator(const Handle coroutine)
        : m_coroutine {coroutine} {
    }
 
    Generator() = default;

    ~Generator() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
 
    // Don't allow copying.
    Generator(const Generator&) = delete;
    auto operator=(const Generator&) -> Generator& = delete;
 
    // Do allow moving.
    Generator(Generator&& other) noexcept :
        m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    auto operator=(Generator&& other) noexcept -> Generator& {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
 
    // Range-based for loop support.
    class Iter {
    public:
        // Each time we iterate we allow the coroutine to run, which
        // should evaluate and store the next result in the promise.
        void operator++() {
            m_coroutine.resume();
        }
        // Forwarding - Deferencing the iterator grabs the current
        // value stored in the promise. It doesn't trigger any other
        // behavior.
        auto operator*() const -> const T& {
            return *m_coroutine.promise().m_current_value;
        }
        // How to ask if the iterator is at the end.
        auto operator==(std::default_sentinel_t) const -> bool {
            return !m_coroutine || m_coroutine.done();
        }
 
        // The iterator knows how to interact with the coroutine handle.
        explicit Iter(const Handle coroutine)
            : m_coroutine{coroutine} {
        }
 
    private:
        Handle m_coroutine;
    };
 
    // Requesting the begin iterator actually triggers the coroutine to run up to the first co_yield and produce a value
    // (or run off the end and terminate).
    auto begin() -> Iter {
        if (m_coroutine) {
            m_coroutine.resume();
        }
        return Iter {m_coroutine};
    }
 
    auto end() -> std::default_sentinel_t {
        return {};
    }
 
private:
    Handle m_coroutine;
};
