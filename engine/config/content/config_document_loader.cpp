#include "config_document_loader.h"

#include "config_load_utils.h"
#include "../../io/json/strict_json.h"

namespace elysia::config
{
std::expected<ConfigDocument,ConfigLoadFailure> ConfigDocumentLoader::load(
    const ConfigManifestEntry& entry) const
{
    const auto parsed = elysia::io::load_strict_json(entry.document_path);
    const std::string source = config_project_relative(entry.document_path);
    if (!parsed)
    {
        const std::string duplicate = duplicate_config_property(parsed.error());
        if (!duplicate.empty())
        {
            ConfigOrigin origin{source,"/"+config_pointer_component(duplicate),entry.key_namespace,
                entry.key_namespace+"."+duplicate};
            return std::unexpected(make_config_load_failure(ConfigLoadError::DuplicateKey,
                parsed.error(),origin,origin));
        }
        return std::unexpected(make_config_load_failure(ConfigLoadError::OpenFailed,parsed.error(),entry.origin));
    }
    return ConfigDocument{entry.key_namespace,entry.document_path,*parsed,
        ConfigOrigin{source,"",entry.key_namespace,entry.key_namespace}};
}
}
