
#include "ModernTest.hpp"

using namespace mt;

TEST("Math works", [] {
    expect(1 + 1) == 2;
    expect(2 * 2).Not() == 5;
});

TEST("Vector matcher", [] {
    std::vector<int> v = {1, 2, 3};
    expect(v).to_contain(2);
    expect(v).Not().is_empty();
});

TEST("Mocking check", [] {
    auto m = mt::mock<int(int)>([](int x) { return x * x; });

    // Call it
    m(10);

    // Verify
    expect(m).to_have_been_called_times(1);
});