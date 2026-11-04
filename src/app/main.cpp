#include "core/foundation.h"

#include <iostream>

// F-01 only: prove the production app links to its pure C++ core.
// F-05 will introduce window initialization after F-02 pins raylib.
int main() {
    std::cout << locus::core::FoundationStatus() << '\n';
    return 0;
}
