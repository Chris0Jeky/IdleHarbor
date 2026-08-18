#include <iostream>

#include "idleharbor/version.hpp"

int main() {
    if (idleharbor::kProductName != L"IdleHarbor") {
        std::cerr << "Unexpected product name\n";
        return 1;
    }
    if (idleharbor::kVersion.empty()) {
        std::cerr << "Version must not be empty\n";
        return 1;
    }
    return 0;
}
