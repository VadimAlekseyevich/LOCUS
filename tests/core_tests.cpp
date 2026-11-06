// Core foundation smoke check (kept alongside established route/judge/editor tests).
// It remains independent of raylib and Windows APIs.
#include "core/foundation.h"

#include <cstring>
#include <iostream>

int main() {
    constexpr const char* expected =
        "LOCUS foundation built. Playable Windows game available.";
    const char* actual = locus::core::FoundationStatus();

    if (actual == nullptr || std::strcmp(actual, expected) != 0) {
        std::cerr << "CORE_FOUNDATION_TEST_FAIL: unexpected core status\n";
        return 1;
    }

    std::cout << "CORE_FOUNDATION_TEST_PASS\n";
    return 0;
}
