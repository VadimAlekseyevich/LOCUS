// F-04: minimal core-only CTest runner, independent of raylib and Windows APIs.
// Replace this placeholder with focused Chart/Route/Timing tests as those modules arrive.
#include "core/foundation.h"

#include <cstring>
#include <iostream>

int main() {
    constexpr const char* expected =
        "LOCUS foundation built. Game window is not implemented yet.";
    const char* actual = locus::core::FoundationStatus();

    if (actual == nullptr || std::strcmp(actual, expected) != 0) {
        std::cerr << "CORE_FOUNDATION_TEST_FAIL: unexpected core status\n";
        return 1;
    }

    std::cout << "CORE_FOUNDATION_TEST_PASS\n";
    return 0;
}
