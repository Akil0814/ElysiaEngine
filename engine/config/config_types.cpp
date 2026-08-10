#include "config_types.h"

namespace elysia::config
{
std::string ConfigOrigin::describe() const
{
    return config_path + "#" + json_pointer + " namespace=" + key_namespace + " key=" + full_key;
}
}
