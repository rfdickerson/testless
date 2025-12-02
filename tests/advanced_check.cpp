#include "ModernTest.hpp"

using namespace mt;

TEST("String hashing", [] {
    std::string base = "ModernTest";
    std::hash<std::string> hasher;

    auto h1 = hasher(base);
    auto h2 = hasher(base + "X");

    expect(h1).Not() == h2;
});

TEST("Floating math", [] {
    double value = std::sin(0.5) * std::cos(0.25);
    expect(std::abs(value)) < 1.0;
});

