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
#include <chrono>
#include <fstream>
#include <sstream>
#include <regex>

namespace mt {

// --- 1. GLOBALS & COLORS ---
inline bool use_colors = true;
inline constexpr std::string_view default_suite_name = "ModernTest";

inline std::string GREEN() { return use_colors ? "\033[32m" : ""; }
inline std::string RED()   { return use_colors ? "\033[31m" : ""; }
inline std::string YELLOW(){ return use_colors ? "\033[33m" : ""; }
inline std::string GRAY()  { return use_colors ? "\033[90m" : ""; }
inline std::string BOLD()  { return use_colors ? "\033[1m" : ""; }
inline std::string RESET() { return use_colors ? "\033[0m" : ""; }

inline bool current_test_failed = false;
inline std::string current_test_file;
inline std::vector<std::string> failure_messages;

// --- 2. REGISTRY ---
enum class TestStatus { NORMAL, SKIP, ONLY };

struct TestCase {
    std::string_view name;
    std::function<void()> func;
    TestStatus status = TestStatus::NORMAL;
    std::string file;
    int line = 0;
};

struct TestResult {
    std::string name;
    std::string file;
    int line = 0;
    bool passed = true;
    bool skipped = false;
    double duration_ms = 0.0;
    std::vector<std::string> failures;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline std::vector<TestResult>& get_results() {
    static std::vector<TestResult> results;
    return results;
}

// Filter pattern for --mt_filter or --gtest_filter
inline std::string test_filter_pattern;
inline std::string xml_output_path;

struct Registrar {
    Registrar(std::string_view name, std::function<void()> func, TestStatus status,
              std::source_location loc = std::source_location::current()) {
        get_tests().push_back({name, func, status, loc.file_name(), static_cast<int>(loc.line())});
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
            std::ostringstream oss;
            oss << (inverted ? "Expected NOT " : "Expected ")
                << "[" << val << "] " << op << " [" << rhs << "]";
            
            // IDE-clickable format: file:line: error: message
            std::cout << "\t" << loc.file_name() << ":" << loc.line() << ": " 
                      << RED() << "error: " << RESET() << oss.str() << "\n";
            
            failure_messages.push_back(
                std::string(loc.file_name()) + ":" + std::to_string(loc.line()) + ": " + oss.str()
            );
            current_test_failed = true;
        }
    }

    void fail(std::string_view msg) {
        // IDE-clickable format: file:line: error: message
        std::cout << "\t" << loc.file_name() << ":" << loc.line() << ": " 
                  << RED() << "error: " << RESET() << std::string(msg) << "\n";
        
        failure_messages.push_back(
            std::string(loc.file_name()) + ":" + std::to_string(loc.line()) + ": " + std::string(msg)
        );
        current_test_failed = true;
    }
};

template <typename T>
Expectation<T> expect(T value, std::source_location loc = std::source_location::current()) {
    return Expectation<T>(value, loc);
}

// --- 5. UTILITIES ---
inline bool matches_filter(std::string_view name, const std::string& pattern) {
    if (pattern.empty()) return true;
    
    // Convert glob pattern to regex: * -> .*, ? -> .
    std::string regex_pattern;
    for (char c : pattern) {
        switch (c) {
            case '*': regex_pattern += ".*"; break;
            case '?': regex_pattern += "."; break;
            case '.': case '(': case ')': case '[': case ']':
            case '{': case '}': case '^': case '$': case '+':
            case '|': case '\\': 
                regex_pattern += '\\'; 
                regex_pattern += c; 
                break;
            default: regex_pattern += c;
        }
    }
    
    try {
        std::regex re(regex_pattern, std::regex::icase);
        return std::regex_search(name.begin(), name.end(), re);
    } catch (...) {
        return name.find(pattern) != std::string_view::npos;
    }
}

inline bool matches_test_filters(const TestCase& test) {
    if (matches_filter(test.name, test_filter_pattern)) {
        return true;
    }
    std::string prefixed = std::string(default_suite_name) + "." + std::string(test.name);
    return matches_filter(prefixed, test_filter_pattern);
}

inline std::string escape_xml(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c;
        }
    }
    return result;
}

inline void write_junit_xml(const std::string& path, double total_time_ms) {
    auto& results = get_results();
    std::ofstream out(path);
    if (!out) return;
    
    int failures = 0, skipped = 0;
    for (const auto& r : results) {
        if (r.skipped) skipped++;
        else if (!r.passed) failures++;
    }
    
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<testsuites tests=\"" << results.size() 
        << "\" failures=\"" << failures 
        << "\" skipped=\"" << skipped
        << "\" time=\"" << (total_time_ms / 1000.0) << "\">\n";
    out << "  <testsuite name=\"ModernTest\" tests=\"" << results.size() 
        << "\" failures=\"" << failures 
        << "\" skipped=\"" << skipped
        << "\" time=\"" << (total_time_ms / 1000.0) << "\">\n";
    
    for (const auto& r : results) {
        out << "    <testcase name=\"" << escape_xml(r.name) 
            << "\" file=\"" << escape_xml(r.file) 
            << "\" line=\"" << r.line
            << "\" time=\"" << (r.duration_ms / 1000.0) << "\"";
        
        if (r.skipped) {
            out << ">\n      <skipped/>\n    </testcase>\n";
        } else if (!r.passed) {
            out << ">\n";
            for (const auto& f : r.failures) {
                out << "      <failure message=\"" << escape_xml(f) << "\"/>\n";
            }
            out << "    </testcase>\n";
        } else {
            out << "/>\n";
        }
    }
    
    out << "  </testsuite>\n</testsuites>\n";
}

inline bool show_help_only = false;

inline void parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Support both --mt_filter and --gtest_filter for compatibility
        if (arg.starts_with("--mt_filter=")) {
            test_filter_pattern = arg.substr(12);
        } else if (arg.starts_with("--gtest_filter=")) {
            test_filter_pattern = arg.substr(15);
        } else if (arg.starts_with("--mt_output=xml:")) {
            xml_output_path = arg.substr(16);
        } else if (arg.starts_with("--gtest_output=xml:")) {
            xml_output_path = arg.substr(19);
        } else if (arg == "--mt_no_color" || arg == "--gtest_color=no") {
            use_colors = false;
        } else if (arg == "--mt_list_tests" || arg == "--gtest_list_tests") {
            // List all tests in Google Test format for CMake's gtest_discover_tests
            // Format: SuiteName.\n  TestName\n
            std::cout << default_suite_name << ".\n";
            for (const auto& t : get_tests()) {
                std::cout << "  " << t.name << "\n";
            }
            show_help_only = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "ModernTest Options:\n"
                      << "  --mt_filter=PATTERN      Run tests matching glob pattern\n"
                      << "  --gtest_filter=PATTERN   (alias for --mt_filter)\n"
                      << "  --mt_output=xml:FILE     Write JUnit XML results to FILE\n"
                      << "  --gtest_output=xml:FILE  (alias for --mt_output)\n"
                      << "  --mt_no_color            Disable colored output\n"
                      << "  --gtest_color=no         (alias for --mt_no_color)\n"
                      << "  --mt_list_tests          List all tests without running\n"
                      << "  --gtest_list_tests       (alias for --mt_list_tests)\n"
                      << "  --help, -h               Show this help\n";
            show_help_only = true;
        }
    }
}

// --- 6. RUNNER ---
inline int run_all_tests(int argc = 0, char* argv[] = nullptr) {
    if (argc > 0 && argv) {
        parse_args(argc, argv);
        if (show_help_only) return 0;
    }
    
    auto& tests = get_tests();
    auto& results = get_results();
    results.clear();
    
    // Check for ONLY
    bool has_only = std::any_of(tests.begin(), tests.end(), 
        [](const auto& t) { return t.status == TestStatus::ONLY; });

    // Count tests that will actually run
    int tests_to_run = 0;
    for (const auto& t : tests) {
        if (t.status != TestStatus::SKIP && (!has_only || t.status == TestStatus::ONLY)) {
            if (matches_test_filters(t)) {
                tests_to_run++;
            }
        }
    }

    std::cout << GREEN() << "[==========]" << RESET() 
              << " Running " << tests_to_run << " test(s) from " << tests.size() << " registered.\n";
    
    auto suite_start = std::chrono::high_resolution_clock::now();
    int passed = 0, failed = 0, skipped = 0;
    std::vector<std::string> failed_tests;

    for (const auto& test : tests) {
        // Check filter
        if (!matches_test_filters(test)) {
            continue;
        }
        
        TestResult result;
        result.name = std::string(test.name);
        result.file = test.file;
        result.line = test.line;
        
        if (test.status == TestStatus::SKIP || (has_only && test.status != TestStatus::ONLY)) {
            skipped++;
            result.skipped = true;
            std::cout << YELLOW() << "[ SKIPPED  ]" << RESET() << " " << test.name << "\n";
            results.push_back(result);
            continue;
        }

        current_test_failed = false;
        failure_messages.clear();
        current_test_file = test.file;
        
        std::cout << "[ RUN      ] " << test.name << "\n";
        
        auto test_start = std::chrono::high_resolution_clock::now();
        
        try {
            test.func();
        } catch (const std::exception& e) {
            std::cout << "\t" << test.file << ":" << test.line << ": " 
                      << RED() << "error: " << RESET() 
                      << "Unhandled exception: " << e.what() << "\n";
            failure_messages.push_back(std::string("Unhandled exception: ") + e.what());
            current_test_failed = true;
        } catch (...) {
            std::cout << "\t" << test.file << ":" << test.line << ": " 
                      << RED() << "error: " << RESET() 
                      << "Unknown exception thrown\n";
            failure_messages.push_back("Unknown exception thrown");
            current_test_failed = true;
        }
        
        auto test_end = std::chrono::high_resolution_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(test_end - test_start).count();
        
        result.duration_ms = duration_ms;
        result.failures = failure_messages;

        if (current_test_failed) {
            result.passed = false;
            std::cout << RED() << "[   FAILED ]" << RESET() 
                      << " " << test.name 
                      << GRAY() << " (" << static_cast<int>(duration_ms) << " ms)" << RESET() << "\n";
            failed_tests.push_back(std::string(test.name));
            failed++;
        } else {
            result.passed = true;
            std::cout << GREEN() << "[       OK ]" << RESET() 
                      << " " << test.name 
                      << GRAY() << " (" << static_cast<int>(duration_ms) << " ms)" << RESET() << "\n";
            passed++;
        }
        
        results.push_back(result);
    }

    auto suite_end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(suite_end - suite_start).count();

    std::cout << GREEN() << "[==========]" << RESET() 
              << " " << (passed + failed) << " test(s) ran. "
              << GRAY() << "(" << static_cast<int>(total_ms) << " ms total)" << RESET() << "\n";
    
    if (passed > 0) {
        std::cout << GREEN() << "[  PASSED  ]" << RESET() << " " << passed << " test(s).\n";
    }
    if (skipped > 0) {
        std::cout << YELLOW() << "[ SKIPPED  ]" << RESET() << " " << skipped << " test(s).\n";
    }
    if (failed > 0) {
        std::cout << RED() << "[  FAILED  ]" << RESET() << " " << failed << " test(s):\n";
        for (const auto& name : failed_tests) {
            std::cout << RED() << "             " << name << RESET() << "\n";
        }
    }

    // Write JUnit XML if requested
    if (!xml_output_path.empty()) {
        write_junit_xml(xml_output_path, total_ms);
        std::cout << GRAY() << "[   INFO   ] XML results written to: " << xml_output_path << RESET() << "\n";
    }

    return failed > 0 ? 1 : 0;
}

} // namespace mt

// --- 6. MACROS ---
#define MT_CONCAT_IMPL(x, y) x##y
#define MT_CONCAT(x, y) MT_CONCAT_IMPL(x, y)

#define TEST(name, ...) static mt::Registrar MT_CONCAT(_reg_, __COUNTER__)(name, __VA_ARGS__, mt::TestStatus::NORMAL)
#define TEST_SKIP(name, ...) static mt::Registrar MT_CONCAT(_reg_, __COUNTER__)(name, __VA_ARGS__, mt::TestStatus::SKIP)
#define TEST_ONLY(name, ...) static mt::Registrar MT_CONCAT(_reg_, __COUNTER__)(name, __VA_ARGS__, mt::TestStatus::ONLY)