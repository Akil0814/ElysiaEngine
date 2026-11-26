#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

namespace elysia::effects
{
enum class EffectRegistrationError
{
    InvalidKey,
    InvalidAnimationKey,
    InvalidSize,
    MissingAnimation,
    InvalidParticleParameters,
    MissingTexture,
    CreationFailed
};

struct EffectRegistrationFailure
{
    EffectRegistrationError code = EffectRegistrationError::InvalidKey;
    elysia::core::FailureDiagnostic diagnostic;
    // Optional field name for callers that can attach a configuration location.
    std::string_view field{};
};
}
