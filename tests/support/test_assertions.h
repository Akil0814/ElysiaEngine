#pragma once

#include <cstdlib>
#include <iostream>

namespace elysia::tests
{
inline void require(bool condition, const char* message)
{
    if (condition)
    {
        return;
    }

    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}
}
