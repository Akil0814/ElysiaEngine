#pragma once

#include <cstdlib>
#include <iostream>

namespace elysia::tests
{
template<typename Condition>
inline void require(Condition&& condition, const char* message)
{
    if (static_cast<bool>(condition))
    {
        return;
    }

    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}
}
