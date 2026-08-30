#include <iostream>

#include "backtester/version.hpp"

int main() {
    std::cout << "Event-Driven Backtester v" << backtester::version() << '\n';
}

