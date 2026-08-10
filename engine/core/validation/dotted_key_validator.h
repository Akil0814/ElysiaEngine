#pragma once

#include <string>
#include <string_view>

namespace elysia::core
{
class DottedKeyValidator
{
public:
    [[nodiscard]] static bool validate_component(std::string_view component,std::string& error);
    [[nodiscard]] static bool validate_key(std::string_view key,std::string& error);
};
}
