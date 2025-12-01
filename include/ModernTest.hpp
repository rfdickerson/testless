#pragma once

#include <iostream>
#include <vector>
#include <functional>
#include <source_location>
#include <string_view>
#include <tuple>
#include <concepts>
#include <algorithm>
#include <ranges>
#include <cmath>
#include <string>

namespace mt {

// --- 1. GLOBALS & COLORS ---
const std::string GREEN = "\033[32m";
const std::string RED   = "\033[31m";
const std::string GRAY  = "\033[90m";
const std::string RESET = "\033[0m";

inline bool current_test_failed = false;

// --- 2. REGISTRY ---
enum class TestStatus { NORMAL, SKIP, ONLY };

struct TestCase {
    std::string_view name;
    std::function<void()> func;
    TestStatus status = TestStatus::NORMAL;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string_view name, std::function<void()> func, TestStatus status) {
        get_tests().push_back({name, func, status});
    }
};

// --- 3. MOCKING SYSTEM ---
template<typename Signature> struct Mock;

template<typename Ret, typename... Args>
struct Mock<Ret(Args...)> {
    using args_tuple = std::tuple<Args...>;
    std::vector<args_tuple> calls;
    std::function<Ret(Args...)> behavior;

    Mock() = default;
    Mock(std::function<Ret(Args...)> f) : behavior(f) {}

    Ret operator()(Args... args) {
        calls.push_back(std::make_tuple(args...));
        if (behavior) return behavior(args...);
        if constexpr (!std::is_void_v<Ret>) return Ret{};
    }
};

template<typename Signature>
Mock<Signature> mock(std::function<Signature> f = {}) { return Mock<Signature>(f); }

// --- 4. ASSERTIONS ---
template <typename T>
struct Expectation {
    T val;
    std::source_location loc;
    bool inverted = false;

    Expectation(T v, std::source_location l) : val(v), loc(l) {}

    // Fluent Negation
    Expectation& Not() { inverted = !inverted; return *this; }

    // --- Core Matchers ---
    template <typename U> void operator==(const U& rhs) { check(rhs, "==", val == rhs); }
    template <typename U> void operator!=(const U& rhs) { check(rhs, "!=", val != rhs); }
    template <typename U> void operator>(const U& rhs)  { check(rhs, ">", val > rhs); }
    template <typename U> void operator<(const U& rhs)  { check(rhs, "<", val < rhs); }

    // --- Container Matchers ---
    template <typename E>
    void to_contain(const E& element) requires std::ranges::range<T> {
        auto it = std::find(val.begin(), val.end(), element);
        bool found = (it != val.end());
        
        if (inverted == found) {
            fail(inverted ? "Expected container NOT to contain element" 
                          : "Expected container to contain element");
        }
    }

    void is_empty() requires requires(T t) { t.empty(); } {
        bool empty = val.empty();
        if (inverted == empty) {
            fail(inverted ? "Expected container NOT to be empty" 
                          : "Expected container to be empty");
        }
    }

    // --- Mock Matchers ---
    void to_have_been_called_times(size_t n) {
        // We assume T is a Mock object with a .calls member
        bool match = (val.calls.size() == n);
        if (inverted == match) {
            fail("Mock call count mismatch. Actual: " + std::to_string(val.calls.size()));
        }
    }

private:
    template <typename U>
    void check(const U& rhs, std::string_view op, bool raw_result) {
        bool final_result = inverted ? !raw_result : raw_result;
        if (!final_result) {
             // Basic print (Improve with custom printer later)
            std::cout << "\t" << RED << "FAILED" << RESET 
                      << " at line " << loc.line() << ": "
                      << (inverted ? "Expected NOT " : "Expected ")
                      << "[" << val << "] " << op << " [" << rhs << "]\n";
            current_test_failed = true;
        }
    }

    void fail(std::string_view msg) {
        std::cout << "\t" << RED << "FAILED" << RESET 
                  << " at line " << loc.line() << ": " << msg << "\n";
        current_test_failed = true;
    }
};

template <typename T>
Expectation<T> expect(T value, std::source_location loc = std::source_location::current()) {
    return Expectation<T>(value, loc);
}

// --- 5. RUNNER ---
inline int run_all_tests() {
    auto& tests = get_tests();
    
    // Check for ONLY
    bool has_only = false;
    for (const auto& t : tests) if (t.status == TestStatus::ONLY) has_only = true;

    std::cout << "\nRunning " << tests.size() << " tests...\n\n";
    int passed = 0, failed = 0, skipped = 0;

    for (const auto& test : tests) {
        if (test.status == TestStatus::SKIP || (has_only && test.status != TestStatus::ONLY)) {
            skipped++;
            std::cout << GRAY << "[ SKIP     ] " << test.name << RESET << "\n";
            continue;
        }

        current_test_failed = false;
        std::cout << "[ RUN      ] " << test.name << "\n";
        
        try {
            test.func();
        } catch (const std::exception& e) {
            std::cout << "\t" << RED << "UNHANDLED EXCEPTION" << RESET << ": " << e.what() << "\n";
            current_test_failed = true;
        }

        if (current_test_failed) {
            std::cout << RED << "[     FAIL ] " << test.name << RESET << "\n";
            failed++;
        } else {
            std::cout << GREEN << "[       OK ] " << test.name << RESET << "\n";
            passed++;
        }
    }

    std::cout << "\nResult: " << passed << " passed, " << failed << " failed, " << skipped << " skipped.\n";
    return failed > 0 ? 1 : 0;
}

} // namespace mt

// --- 6. MACROS ---
#define MT_CONCAT_IMPL(x, y) x##y
#define MT_CONCAT(x, y) MT_CONCAT_IMPL(x, y)

#define TEST(name, ...) static mt::Registrar MT_CONCAT(_reg_, __COUNTER__)(name, __VA_ARGS__, mt::TestStatus::NORMAL)
#define TEST_SKIP(name, ...) static mt::Registrar MT_CONCAT(_reg_, __COUNTER__)(name, __VA_ARGS__, mt::TestStatus::SKIP)
#define TEST_ONLY(name, ...) static mt::Registrar MT_CONCAT(_reg_, __COUNTER__)(name, __VA_ARGS__, mt::TestStatus::ONLY)