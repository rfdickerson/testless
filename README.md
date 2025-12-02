# ModernTest
> The "Suckless" Unit Testing Framework for C++20

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
[![CMake on multiple platforms](https://github.com/rfdickerson/testless/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/rfdickerson/testless/actions/workflows/cmake-multi-platform.yml)

Header-only • Zero boilerplate • IDE-native • Fast as hell

ModernTest is a tiny, expressive, C++20-first testing library. It abandons the legacy class-per-test style of GoogleTest in favor of lambdas, concepts, and std::source_location.

It compiles in milliseconds, requires no setup, and even impersonates the GoogleTest protocol—so CLion, VS Code, and CTest just work out of the box.

## ⚡ Quick Start

### 1. Add to CMake (no installation needed)
```cmake
include(FetchContent)
FetchContent_Declare(
    moderntest
    GIT_REPOSITORY https://github.com/your-username/ModernTest.git
    GIT_TAG main
)
FetchContent_MakeAvailable(moderntest)

# Runner includes main()
add_executable(unit_tests tests/my_tests.cpp)
target_link_libraries(unit_tests PRIVATE ModernTest::Runner)

# Auto-register tests with CTest
include(GoogleTest)
gtest_discover_tests(unit_tests)
```

### 2. The Syntax

Forget ASSERT_EQ, ASSERT_NE, or macro jungles.
Just write C++.
```cpp
#include "ModernTest.hpp"

TEST("Basic Math", [] {
    int x = 21;
    expect(x * 2) == 42;
    expect(x) != 100;
});

TEST("Fluent Negation", [] {
    expect(1 + 1).not() == 5;
});

TEST("Containers", [] {
    std::vector<int> v = {1, 2, 3};

    expect(v).to_contain(2);
    expect(v).not().is_empty();
    expect(v).has_size(3);
});
```

## 🆚 Why ModernTest?

Because boilerplate is a tax.

| Feature | GoogleTest | Catch2 | ModernTest |
|---------|-----------|--------|------------|
| Boilerplate | High (fixtures, classes, macros) | Medium | Zero (lambdas) |
| Assertions | ASSERT_EQ(a, b) | REQUIRE(a == b) | expect(a) == b |
| Negation | ASSERT_NE(a, b) | REQUIRE(a != b) | expect(a).not() == b |
| Mocking | Heavy (inheritance) | External | Functional mocks built-in |
| IDE Integration | Native | Plugins | Native (GTest protocol impersonation) |
| Compile Speed | Slow link step | Slow-ish | Milliseconds |

## 🧠 Advanced Features

### Functional Mocks

Mock functions—not classes.
```cpp
// Mock int(int)
auto square = mt::mock<int(int)>([](int x) { return x * x; });

square(5);

expect(square).to_have_been_called_times(1);
expect(square).to_have_been_called_with(5);
```

No inheritance. No virtual tables. Just lambdas.

### Data-Driven Tests (C++20 native)

No TEST_P, no bizarre macro expansions.
```cpp
TEST("String Truncation", [] {
    struct Case { std::string in; int len; std::string out; };
    std::vector<Case> cases = {
        {"hello", 5, "hello"},
        {"world", 1, "w"},
    };

    for (const auto& [in, len, out] : cases) {
        expect(truncate(in, len)) == out;
    }
});
```

### Game-Dev Ready (Vectors, Flags, Bitmasks)

Built with C++20 game engines in mind.
```cpp
// Fuzzy equality for any type with .x/.y/.z
expect(player.pos).to_be_approx(target.pos, 0.001f);

// Vulkan-style bitmask checking
expect(image_flags).to_have_flag(VK_IMAGE_USAGE_SAMPLED_BIT);
```

## 🛠️ IDE Integration

ModernTest implements the GoogleTest CLI protocol, so your IDE already understands it.

### CLion

Run → Edit Configurations → + → Google Test

- Target your unit_tests binary
- You get full test trees, clickable failures, and debugging.

### VS Code

Install C++ Test Mate

- It will auto-detect your tests
- Run and debug tests individually via the "Beaker" icon.

## 📜 License

MIT License. Hack freely.