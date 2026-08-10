#include "dotted_key_validator.h"

namespace elysia::core
{
bool DottedKeyValidator::validate_component(std::string_view component,std::string& error)
{
    if (component.empty()) { error = "key component is empty"; return false; }
    for (const unsigned char character : component)
    {
        const bool alpha = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';
        if (!(alpha || digit || character == '_'))
        { error = "invalid key component: " + std::string(component); return false; }
    }
    error.clear(); return true;
}

bool DottedKeyValidator::validate_key(std::string_view key,std::string& error)
{
    if (key.empty()) { error = "key is empty"; return false; }
    size_t begin = 0;
    while (begin <= key.size())
    {
        const size_t end = key.find('.',begin);
        const auto component = key.substr(begin,end == std::string_view::npos ? key.size() - begin : end - begin);
        if (!validate_component(component,error)) return false;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return true;
}
}
