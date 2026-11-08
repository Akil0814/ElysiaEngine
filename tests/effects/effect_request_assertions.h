#pragma once

#include "engine/effects/effect_request_failure.h"
#include "tests/support/test_assertions.h"

#include <expected>

namespace elysia::tests
{
template<typename T>
void require_effect_failure(const std::expected<T, effects::EffectRequestFailure>& result,
    effects::EffectRequestError code)
{
    require(!result, "effect request must fail");
    require(result.error().code == code, "effect failure must report the expected error code");
    require(!result.error().diagnostic.message.empty(), "effect failure must explain its cause");
    require(result.error().diagnostic.origin.line() != 0, "effect failure must retain its source location");
}
}
