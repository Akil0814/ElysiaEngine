#requires -Version 7.0

[CmdletBinding(DefaultParameterSetName = 'Report')]
param(
    [Parameter(ParameterSetName = 'Update')]
    [switch]$Update,

    [Parameter(ParameterSetName = 'Check')]
    [switch]$Check,

    [Parameter(Mandatory, ParameterSetName = 'Feature')]
    [ValidateNotNullOrEmpty()]
    [string]$Feature,

    [Parameter(ParameterSetName = 'SelfTest')]
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepositoryRoot = Split-Path -Parent $PSScriptRoot
$script:DocumentPath = Join-Path $script:RepositoryRoot 'docs/development/cpp-language-features.md'
$script:GeneratedBegin = '<!-- BEGIN GENERATED: cpp-language-feature-stats -->'
$script:GeneratedEnd = '<!-- END GENERATED: cpp-language-feature-stats -->'
$script:SourceRoots = @('engine', 'game', 'tests')

function New-FeatureDefinition {
    param(
        [Parameter(Mandatory)][string]$Id,
        [Parameter(Mandatory)][ValidateSet('C++11', 'C++14', 'C++17', 'C++20', 'C++23')][string]$Standard,
        [Parameter(Mandatory)][ValidateSet('Language', 'Library')][string]$Kind,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Pattern,
        [string]$Example = '',
        [string[]]$Symbols = @(),
        [switch]$CommonUnused
    )

    [pscustomobject]@{
        Id = $Id
        Standard = $Standard
        Kind = $Kind
        Name = $Name
        Pattern = $Pattern
        Example = $Example
        Symbols = @($Symbols)
        CommonUnused = [bool]$CommonUnused
    }
}

$script:Features = @(
    New-FeatureDefinition -Id 'cpp11-auto' -Standard 'C++11' -Kind 'Language' -Name '`auto` 类型推导' -Pattern '\b(?:(?:static|inline|constexpr|const)\s+)*auto(?:\s*[*&])?\s+[A-Za-z_]\w*\s*(?==|\{|;|:)' -Example 'game/scene/main_menu_scene.cpp'
    New-FeatureDefinition -Id 'cpp11-lambda' -Standard 'C++11' -Kind 'Language' -Name 'Lambda 表达式' -Pattern '\[[^\]]*\]\s*(?:\([^)]*\))?\s*(?:mutable\s*)?(?:noexcept\s*)?(?:->[^\{]+)?\{' -Example 'game/scene/demo/engine_feature_lab_scene.cpp'
    New-FeatureDefinition -Id 'cpp11-range-for' -Standard 'C++11' -Kind 'Language' -Name '基于范围的 `for`' -Pattern '(?s:\bfor\s*\((?:(?!;).)*?:(?:(?!;).)*?\))' -Example 'game/scene/demo/ui_component_gallery_content.cpp'
    New-FeatureDefinition -Id 'cpp11-nullptr' -Standard 'C++11' -Kind 'Language' -Name '`nullptr`' -Pattern '\bnullptr\b' -Example 'engine/scene/scene_manager.h'
    New-FeatureDefinition -Id 'cpp11-enum-class' -Standard 'C++11' -Kind 'Language' -Name '有作用域枚举' -Pattern '\benum\s+class\b' -Example 'engine/animation/animation_registration_failure.h'
    New-FeatureDefinition -Id 'cpp11-override' -Standard 'C++11' -Kind 'Language' -Name '`override`' -Pattern '\boverride\b' -Example 'game/scene/demo/ui_component_gallery_scene.h'
    New-FeatureDefinition -Id 'cpp11-final' -Standard 'C++11' -Kind 'Language' -Name '`final`' -Pattern '\bfinal\b' -Example 'game/scene/demo/ui_component_gallery_scene.h'
    New-FeatureDefinition -Id 'cpp11-constexpr' -Standard 'C++11' -Kind 'Language' -Name '`constexpr`' -Pattern '\bconstexpr\b' -Example 'game/scene/demo/engine_feature_lab_scene.cpp'
    New-FeatureDefinition -Id 'cpp11-noexcept' -Standard 'C++11' -Kind 'Language' -Name '`noexcept`' -Pattern '\bnoexcept\b' -Example 'engine/scene/scene_manager.h'
    New-FeatureDefinition -Id 'cpp11-static-assert' -Standard 'C++11' -Kind 'Language' -Name '`static_assert`' -Pattern '\bstatic_assert\b' -Example 'engine/scene/scene_manager.h'
    New-FeatureDefinition -Id 'cpp11-defaulted-functions' -Standard 'C++11' -Kind 'Language' -Name '显式默认函数' -Pattern '=\s*default\s*;' -Example 'game/scene/main_menu_scene.h'
    New-FeatureDefinition -Id 'cpp11-deleted-functions' -Standard 'C++11' -Kind 'Language' -Name '显式删除函数' -Pattern '=\s*delete\s*;' -Example 'engine/camera/camera_manager.h'
    New-FeatureDefinition -Id 'cpp11-decltype' -Standard 'C++11' -Kind 'Language' -Name '`decltype`' -Pattern '\bdecltype\s*\(' -Example 'tests/camera/camera_manager_tests.cpp'
    New-FeatureDefinition -Id 'cpp11-type-alias' -Standard 'C++11' -Kind 'Language' -Name '`using` 类型别名' -Pattern '\busing\s+[A-Za-z_]\w*\s*=' -Example 'engine/animation/animation.h'
    New-FeatureDefinition -Id 'cpp11-variadic-templates' -Standard 'C++11' -Kind 'Language' -Name '可变参数模板' -Pattern '\b(?:class|typename)\s*\.\.\.|template\s*<[^>]*\.\.\.' -Example 'engine/ui/core/ui_child_host.h'
    New-FeatureDefinition -Id 'cpp11-move-forward' -Standard 'C++11' -Kind 'Library' -Name '移动与完美转发工具' -Pattern '\bstd::(?:move|forward|make_move_iterator)\s*\(' -Example 'game/scene/main_menu_scene.cpp' -Symbols @('move', 'forward', 'make_move_iterator')
    New-FeatureDefinition -Id 'cpp11-smart-pointers' -Standard 'C++11' -Kind 'Library' -Name '智能指针与 `std::make_shared`' -Pattern '\bstd::(?:unique_ptr|shared_ptr|weak_ptr|make_shared)\b' -Example 'engine/ui/core/ui_child_host.h' -Symbols @('unique_ptr', 'shared_ptr', 'weak_ptr', 'make_shared')
    New-FeatureDefinition -Id 'cpp11-array' -Standard 'C++11' -Kind 'Library' -Name '`std::array`' -Pattern '\bstd::array\b' -Example 'game/scene/demo/ui_component_gallery_scene.h' -Symbols @('array')
    New-FeatureDefinition -Id 'cpp11-function' -Standard 'C++11' -Kind 'Library' -Name '`std::function`' -Pattern '\bstd::function\b' -Example 'engine/animation/animation.h' -Symbols @('function')
    New-FeatureDefinition -Id 'cpp11-tuple' -Standard 'C++11' -Kind 'Library' -Name '元组工具' -Pattern '\bstd::(?:tuple|make_tuple|tie)\b' -Example 'engine/ui/core/ui_child_host.h' -Symbols @('tuple', 'make_tuple', 'tie')
    New-FeatureDefinition -Id 'cpp11-chrono' -Standard 'C++11' -Kind 'Library' -Name '`std::chrono`' -Pattern '\bstd::chrono(?:::|_)' -Example 'tests/audio/sound_playback_scheduler_tests.cpp' -Symbols @('chrono')
    New-FeatureDefinition -Id 'cpp11-random' -Standard 'C++11' -Kind 'Library' -Name '随机数工具' -Pattern '\bstd::(?:random_device|mt19937|mt19937_64|uniform_[A-Za-z_]+_distribution)\b' -Example 'engine/tools/random_generator.cpp' -Symbols @('random_device', 'mt19937', 'mt19937_64', 'uniform_real_distribution')
    New-FeatureDefinition -Id 'cpp11-concurrency' -Standard 'C++11' -Kind 'Library' -Name '线程、同步与原子操作' -Pattern '\bstd::(?:thread|this_thread|mutex|lock_guard|unique_lock|condition_variable|atomic|memory_order_[A-Za-z_]+)\b' -Example 'engine/tools/termination_manager.h' -Symbols @('thread', 'this_thread', 'mutex', 'lock_guard', 'unique_lock', 'condition_variable', 'atomic', 'memory_order_acq_rel', 'memory_order_acquire', 'memory_order_release')
    New-FeatureDefinition -Id 'cpp11-unordered-containers' -Standard 'C++11' -Kind 'Library' -Name '无序容器与哈希' -Pattern '\bstd::(?:unordered_map|unordered_set|hash)\b' -Example 'engine/builtin/resources/builtin_asset_cache.h' -Symbols @('unordered_map', 'unordered_set', 'hash')
    New-FeatureDefinition -Id 'cpp11-initializer-list' -Standard 'C++11' -Kind 'Library' -Name '`std::initializer_list`' -Pattern '\bstd::initializer_list\b' -Example 'engine/resources/atlas/atlas.h' -Symbols @('initializer_list')
    New-FeatureDefinition -Id 'cpp11-type-index' -Standard 'C++11' -Kind 'Library' -Name '`std::type_index`' -Pattern '\bstd::type_index\b' -Example 'engine/ui/style/ui_theme_style_resolver.h' -Symbols @('type_index')
    New-FeatureDefinition -Id 'cpp11-declval' -Standard 'C++11' -Kind 'Library' -Name '`std::declval`' -Pattern '\bstd::declval\b' -Example 'tests/camera/camera_manager_tests.cpp' -Symbols @('declval')
    New-FeatureDefinition -Id 'cpp11-new-algorithms' -Standard 'C++11' -Kind 'Library' -Name 'C++11 非修改序列算法' -Pattern '\bstd::(?:all_of|any_of|none_of|copy_n)\b' -Example 'engine/ui/window/ui_window.cpp' -Symbols @('all_of', 'any_of', 'none_of', 'copy_n')
    New-FeatureDefinition -Id 'cpp11-system-error' -Standard 'C++11' -Kind 'Library' -Name '系统错误码工具' -Pattern '\bstd::(?:error_code|errc)\b' -Example 'engine/builtin/resources/builtin_asset_catalog.cpp' -Symbols @('error_code', 'errc')
    New-FeatureDefinition -Id 'cpp11-fixed-width-integers' -Standard 'C++11' -Kind 'Library' -Name '定宽整数类型' -Pattern '\bstd::(?:u?int(?:8|16|32|64)_t|uintptr_t)\b' -Example 'engine/audio/sound_playback_types.h' -Symbols @('int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint32_t', 'uint64_t', 'uintptr_t')
    New-FeatureDefinition -Id 'cpp11-math-functions' -Standard 'C++11' -Kind 'Library' -Name 'C++11 数学函数' -Pattern '\bstd::(?:copysign|isfinite|lround|nan|nextafter|round)\b' -Example 'engine/application/application.cpp' -Symbols @('copysign', 'isfinite', 'lround', 'nan', 'nextafter', 'round')
    New-FeatureDefinition -Id 'cpp11-string-conversion' -Standard 'C++11' -Kind 'Library' -Name '`std::to_string`' -Pattern '\bstd::to_string\b' -Example 'game/demo/physics/demo_combat.cpp' -Symbols @('to_string')
    New-FeatureDefinition -Id 'cpp11-time-formatting' -Standard 'C++11' -Kind 'Library' -Name '`std::put_time`' -Pattern '\bstd::put_time\b' -Example 'engine/tools/logger.cpp' -Symbols @('put_time')

    New-FeatureDefinition -Id 'cpp14-generic-lambda' -Standard 'C++14' -Kind 'Language' -Name '泛型 Lambda' -Pattern '\[[^\]]*\]\s*\([^)]*\bauto(?:\s*[*&])?\b' -Example 'game/scene/demo/physics/physics_combat_demo_scene_base.cpp'
    New-FeatureDefinition -Id 'cpp14-init-capture' -Standard 'C++14' -Kind 'Language' -Name 'Lambda 初始化捕获' -Pattern '\[[^\]]*\b[A-Za-z_]\w*\s*=' -Example 'engine/ui/widgets/ui_button.cpp'
    New-FeatureDefinition -Id 'cpp14-make-unique' -Standard 'C++14' -Kind 'Library' -Name '`std::make_unique`' -Pattern '\bstd::make_unique\b' -Example 'engine/animation/animation_service.cpp' -Symbols @('make_unique')
    New-FeatureDefinition -Id 'cpp14-chrono-literals' -Standard 'C++14' -Kind 'Library' -Name '时间字面量' -Pattern '\bstd::chrono_literals\b' -Example 'tests/audio/sound_playback_scheduler_tests.cpp' -Symbols @('chrono_literals')
    New-FeatureDefinition -Id 'cpp14-type-trait-aliases' -Standard 'C++14' -Kind 'Library' -Name '类型特征 `_t` 别名' -Pattern '\bstd::(?:conditional|decay|enable_if|make_signed|make_unsigned|remove_const|remove_cv|remove_reference|tuple_element|underlying_type)_t\b' -Example 'engine/ui/core/ui_child_host.h' -Symbols @('conditional_t', 'decay_t', 'enable_if_t', 'make_signed_t', 'make_unsigned_t', 'remove_const_t', 'remove_cv_t', 'remove_reference_t', 'tuple_element_t', 'underlying_type_t')
    New-FeatureDefinition -Id 'cpp14-binary-literals' -Standard 'C++14' -Kind 'Language' -Name '二进制字面量' -Pattern '\b0[bB][01]+' -CommonUnused
    New-FeatureDefinition -Id 'cpp14-digit-separators' -Standard 'C++14' -Kind 'Language' -Name '数字分隔符' -Pattern '\b\d[\dA-Fa-fxXbB]*''[\dA-Fa-f]+' -CommonUnused
    New-FeatureDefinition -Id 'cpp14-exchange' -Standard 'C++14' -Kind 'Library' -Name '`std::exchange`' -Pattern '\bstd::exchange\b' -Symbols @('exchange') -CommonUnused

    New-FeatureDefinition -Id 'cpp17-structured-bindings' -Standard 'C++17' -Kind 'Language' -Name '结构化绑定' -Pattern '\b(?:const\s+)?auto(?:\s*&&?|\s+)?\s*\[[^\]]+\]' -Example 'game/demo/physics/demo_combat.cpp'
    New-FeatureDefinition -Id 'cpp17-if-constexpr' -Standard 'C++17' -Kind 'Language' -Name '`if constexpr`' -Pattern '\bif\s+constexpr\b' -Example 'engine/camera/camera_manager.cpp'
    New-FeatureDefinition -Id 'cpp17-if-initializer' -Standard 'C++17' -Kind 'Language' -Name '`if` 初始化语句' -Pattern '\bif\s*\(\s*(?:const\s+)?(?:auto(?:\s*[*&])?|[A-Za-z_][\w:<>]*\s*[*&]?)\s+[A-Za-z_]\w*\s*=[^;\r\n]+;' -Example 'engine/animation/runtime/animation_manager.cpp'
    New-FeatureDefinition -Id 'cpp17-nested-namespace' -Standard 'C++17' -Kind 'Language' -Name '嵌套命名空间定义' -Pattern '\bnamespace\s+[A-Za-z_]\w*(?:::[A-Za-z_]\w*)+' -Example 'engine/animation/animation.h'
    New-FeatureDefinition -Id 'cpp17-inline-variables' -Standard 'C++17' -Kind 'Language' -Name '内联变量' -Pattern '\binline\s+(?:static\s+)?constexpr\b' -Example 'engine/core/render/colors.h'
    New-FeatureDefinition -Id 'cpp17-ctad' -Standard 'C++17' -Kind 'Language' -Name '类模板实参推导' -Pattern '\bstd::array(?:\s+[A-Za-z_]\w*)?\s*\{' -Example 'tests/application/application_presentation_settings_tests.cpp'
    New-FeatureDefinition -Id 'cpp17-nodiscard' -Standard 'C++17' -Kind 'Language' -Name '`[[nodiscard]]`' -Pattern '\[\[\s*nodiscard\s*\]\]' -Example 'engine/animation/animation.h'
    New-FeatureDefinition -Id 'cpp17-maybe-unused' -Standard 'C++17' -Kind 'Language' -Name '`[[maybe_unused]]`' -Pattern '\[\[\s*maybe_unused\s*\]\]' -CommonUnused
    New-FeatureDefinition -Id 'cpp17-fallthrough' -Standard 'C++17' -Kind 'Language' -Name '`[[fallthrough]]`' -Pattern '\[\[\s*fallthrough\s*\]\]' -CommonUnused
    New-FeatureDefinition -Id 'cpp17-optional' -Standard 'C++17' -Kind 'Library' -Name '`std::optional` / `std::nullopt`' -Pattern '\bstd::(?:optional|nullopt)\b' -Example 'engine/animation/animation.h' -Symbols @('optional', 'nullopt')
    New-FeatureDefinition -Id 'cpp17-variant' -Standard 'C++17' -Kind 'Library' -Name '变体类型工具' -Pattern '\bstd::(?:variant|visit|get|get_if|holds_alternative|monostate)\b' -Example 'engine/ui/widgets/ui_button.cpp' -Symbols @('variant', 'visit', 'get', 'get_if', 'holds_alternative', 'monostate')
    New-FeatureDefinition -Id 'cpp17-any' -Standard 'C++17' -Kind 'Library' -Name '`std::any`' -Pattern '\bstd::(?:any|any_cast)\b' -Example 'tests/scene/scene_core_tests.cpp' -Symbols @('any', 'any_cast')
    New-FeatureDefinition -Id 'cpp17-filesystem' -Standard 'C++17' -Kind 'Library' -Name '`std::filesystem`' -Pattern '\bstd::filesystem\b' -Example 'engine/io/path/path_manager.h' -Symbols @('filesystem')
    New-FeatureDefinition -Id 'cpp17-string-view' -Standard 'C++17' -Kind 'Library' -Name '`std::string_view`' -Pattern '\bstd::string_view\b' -Example 'engine/animation/animation_service.h' -Symbols @('string_view')
    New-FeatureDefinition -Id 'cpp17-invoke' -Standard 'C++17' -Kind 'Library' -Name '`std::invoke` 与结果类型特征' -Pattern '\bstd::(?:invoke|invoke_result_t)\b' -Example 'engine/object_query/game_object_query_service.h' -Symbols @('invoke', 'invoke_result_t')
    New-FeatureDefinition -Id 'cpp17-as-const' -Standard 'C++17' -Kind 'Library' -Name '`std::as_const`' -Pattern '\bstd::as_const\b' -Example 'engine/object_query/game_object_query_service.h' -Symbols @('as_const')
    New-FeatureDefinition -Id 'cpp17-scoped-lock' -Standard 'C++17' -Kind 'Library' -Name '`std::scoped_lock`' -Pattern '\bstd::scoped_lock\b' -Example 'engine/config/config_service.cpp' -Symbols @('scoped_lock')
    New-FeatureDefinition -Id 'cpp17-clamp' -Standard 'C++17' -Kind 'Library' -Name '`std::clamp`' -Pattern '\bstd::clamp\b' -Example 'engine/camera/camera_controller.cpp' -Symbols @('clamp')
    New-FeatureDefinition -Id 'cpp17-size' -Standard 'C++17' -Kind 'Library' -Name '`std::size`' -Pattern '\bstd::size\b' -Example 'tests/loading/resource_load_plan_validator_tests.cpp' -Symbols @('size')
    New-FeatureDefinition -Id 'cpp17-type-trait-variables' -Standard 'C++17' -Kind 'Library' -Name '类型特征 `_v` 变量模板' -Pattern '\bstd::[A-Za-z_]\w*_v\b' -Example 'engine/camera/camera_manager.cpp' -Symbols @('is_abstract_v', 'is_base_of_v', 'is_const_v', 'is_copy_assignable_v', 'is_copy_constructible_v', 'is_default_constructible_v', 'is_move_assignable_v', 'is_move_constructible_v', 'is_same_v')

    New-FeatureDefinition -Id 'cpp20-designated-initializers' -Standard 'C++20' -Kind 'Language' -Name '指定初始化器' -Pattern '(?s:\{\s*\.[A-Za-z_]\w*\s*=)' -Example 'game/scene/main_menu_scene.cpp'
    New-FeatureDefinition -Id 'cpp20-three-way-comparison' -Standard 'C++20' -Kind 'Language' -Name '三路比较' -Pattern '<=>' -Example 'engine/physics/collision/collision_target.h'
    New-FeatureDefinition -Id 'cpp20-concepts' -Standard 'C++20' -Kind 'Language' -Name 'Concepts 与 `requires` 子句' -Pattern '\bconcept\s+[A-Za-z_]\w*\s*=|\brequires\s+(?:std::|[A-Za-z_]\w*\s*<)|\brequires\s*\{' -Example 'engine/save/save_data.h'
    New-FeatureDefinition -Id 'cpp20-span' -Standard 'C++20' -Kind 'Library' -Name '`std::span`' -Pattern '\bstd::span\b' -Example 'engine/physics/contracts/collider_provider.h' -Symbols @('span')
    New-FeatureDefinition -Id 'cpp20-ranges' -Standard 'C++20' -Kind 'Library' -Name 'Ranges 库' -Pattern '\bstd::ranges(?:::|_)' -Example 'engine/physics/physics_world.cpp' -Symbols @('ranges')
    New-FeatureDefinition -Id 'cpp20-standard-concepts' -Standard 'C++20' -Kind 'Library' -Name '标准库 Concepts' -Pattern '\bstd::(?:integral|same_as|predicate)\b' -Example 'engine/object_query/game_object_query_service.h' -Symbols @('integral', 'same_as', 'predicate')
    New-FeatureDefinition -Id 'cpp20-source-location' -Standard 'C++20' -Kind 'Library' -Name '`std::source_location`' -Pattern '\bstd::source_location\b' -Example 'engine/application/lifecycle/application_termination_logging.h' -Symbols @('source_location')
    New-FeatureDefinition -Id 'cpp20-erase' -Standard 'C++20' -Kind 'Library' -Name '`std::erase` / `std::erase_if`' -Pattern '\bstd::erase(?:_if)?\b' -Example 'engine/physics/physics_world.cpp' -Symbols @('erase', 'erase_if')
    New-FeatureDefinition -Id 'cpp20-string-prefix-suffix' -Standard 'C++20' -Kind 'Library' -Name '`starts_with` / `ends_with`' -Pattern '\.(?:starts_with|ends_with)\s*\(' -Example 'tests/save/save_service_tests.cpp'
    New-FeatureDefinition -Id 'cpp20-remove-cvref' -Standard 'C++20' -Kind 'Library' -Name '`std::remove_cvref_t`' -Pattern '\bstd::remove_cvref_t\b' -Example 'engine/save/save_service.h' -Symbols @('remove_cvref_t')
    New-FeatureDefinition -Id 'cpp20-bit-operations' -Standard 'C++20' -Kind 'Library' -Name '位操作工具' -Pattern '\bstd::has_single_bit\b' -Example 'engine/tools/debug_draw.cpp' -Symbols @('has_single_bit')
    New-FeatureDefinition -Id 'cpp20-ordering-types' -Standard 'C++20' -Kind 'Library' -Name '比较类别类型' -Pattern '\bstd::strong_ordering\b' -Example 'engine/physics/collision/collision_target.h' -Symbols @('strong_ordering')
    New-FeatureDefinition -Id 'cpp20-consteval' -Standard 'C++20' -Kind 'Language' -Name '`consteval`' -Pattern '\bconsteval\b' -CommonUnused
    New-FeatureDefinition -Id 'cpp20-constinit' -Standard 'C++20' -Kind 'Language' -Name '`constinit`' -Pattern '\bconstinit\b' -CommonUnused
    New-FeatureDefinition -Id 'cpp20-coroutines' -Standard 'C++20' -Kind 'Language' -Name '协程' -Pattern '\b(?:co_await|co_return|co_yield)\b' -CommonUnused
    New-FeatureDefinition -Id 'cpp20-bit-cast' -Standard 'C++20' -Kind 'Library' -Name '`std::bit_cast`' -Pattern '\bstd::bit_cast\b' -Symbols @('bit_cast') -CommonUnused
    New-FeatureDefinition -Id 'cpp20-jthread' -Standard 'C++20' -Kind 'Library' -Name '`std::jthread` / 停止令牌' -Pattern '\bstd::(?:jthread|stop_token)\b' -Symbols @('jthread', 'stop_token') -CommonUnused
    New-FeatureDefinition -Id 'cpp20-format' -Standard 'C++20' -Kind 'Library' -Name '`std::format`' -Pattern '\bstd::format\b' -Symbols @('format') -CommonUnused

    New-FeatureDefinition -Id 'cpp23-expected' -Standard 'C++23' -Kind 'Library' -Name '`std::expected` / `std::unexpected`' -Pattern '\bstd::(?:expected|unexpected)\b' -Example 'engine/bootstrap/bootstrapper.h' -Symbols @('expected', 'unexpected')
    New-FeatureDefinition -Id 'cpp23-if-consteval' -Standard 'C++23' -Kind 'Language' -Name '`if consteval`' -Pattern '\bif\s+consteval\b' -CommonUnused
    New-FeatureDefinition -Id 'cpp23-to-underlying' -Standard 'C++23' -Kind 'Library' -Name '`std::to_underlying`' -Pattern '\bstd::to_underlying\b' -Symbols @('to_underlying') -CommonUnused
    New-FeatureDefinition -Id 'cpp23-byteswap' -Standard 'C++23' -Kind 'Library' -Name '`std::byteswap`' -Pattern '\bstd::byteswap\b' -Symbols @('byteswap') -CommonUnused
    New-FeatureDefinition -Id 'cpp23-print' -Standard 'C++23' -Kind 'Library' -Name '`std::print` / `std::println`' -Pattern '\bstd::print(?:ln)?\b' -Symbols @('print', 'println') -CommonUnused
    New-FeatureDefinition -Id 'cpp23-mdspan' -Standard 'C++23' -Kind 'Library' -Name '`std::mdspan`' -Pattern '\bstd::mdspan\b' -Symbols @('mdspan') -CommonUnused
)

$script:BaselineStdSymbols = @(
    'abs', 'adjacent_find', 'back_inserter', 'ceil', 'cerr', 'clog', 'cos', 'count_if',
    'cout', 'deque', 'distance', 'exception', 'exit', 'fabs', 'find', 'find_if', 'fixed',
    'floor', 'fmod', 'getenv', 'greater', 'ifstream', 'invalid_argument', 'ios',
    'istreambuf_iterator', 'less', 'logic_error', 'map', 'max', 'memcpy', 'min',
    'numeric_limits', 'ofstream', 'ostream', 'ostringstream', 'pair', 'pow', 'ptrdiff_t',
    'remove', 'remove_if', 'runtime_error', 'set',
    'setfill', 'setprecision', 'setw', 'sin', 'size_t', 'sort', 'sqrt', 'stable_sort',
    'strcpy', 'streambuf', 'streamsize', 'string', 'swap', 'time', 'time_t', 'tm',
    'toupper', 'unique',
    'upper_bound', 'vector'
)

function Hide-CppNonCode {
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Text)

    $task_builder = [System.Text.StringBuilder]::new($Text.Length)
    $task_index = 0
    $task_state = 'Code'

    while ($task_index -lt $Text.Length) {
        $task_char = $Text[$task_index]
        $task_next = if ($task_index + 1 -lt $Text.Length) { $Text[$task_index + 1] } else { [char]0 }

        if ($task_state -eq 'Code') {
            if ($task_char -eq '/' -and $task_next -eq '/') {
                [void]$task_builder.Append('  ')
                $task_index += 2
                $task_state = 'LineComment'
                continue
            }
            if ($task_char -eq '/' -and $task_next -eq '*') {
                [void]$task_builder.Append('  ')
                $task_index += 2
                $task_state = 'BlockComment'
                continue
            }
            if ($task_char -eq 'R' -and $task_next -eq '"') {
                $task_open = $Text.IndexOf('(', $task_index + 2)
                if ($task_open -ge 0 -and ($task_open - ($task_index + 2)) -le 16) {
                    $task_delimiter = $Text.Substring($task_index + 2, $task_open - ($task_index + 2))
                    $task_terminator = ')' + $task_delimiter + '"'
                    $task_close = $Text.IndexOf($task_terminator, $task_open + 1, [System.StringComparison]::Ordinal)
                    if ($task_close -ge 0) {
                        $task_end = $task_close + $task_terminator.Length
                        while ($task_index -lt $task_end) {
                            $task_raw_char = $Text[$task_index]
                            [void]$task_builder.Append($(if ($task_raw_char -eq "`r" -or $task_raw_char -eq "`n") { $task_raw_char } else { ' ' }))
                            $task_index++
                        }
                        continue
                    }
                }
            }
            if ($task_char -eq '"') {
                [void]$task_builder.Append(' ')
                $task_index++
                $task_state = 'String'
                continue
            }
            if ($task_char -eq "'") {
                [void]$task_builder.Append(' ')
                $task_index++
                $task_state = 'Character'
                continue
            }

            [void]$task_builder.Append($task_char)
            $task_index++
            continue
        }

        if ($task_state -eq 'LineComment') {
            if ($task_char -eq "`r" -or $task_char -eq "`n") {
                [void]$task_builder.Append($task_char)
                $task_state = 'Code'
            } else {
                [void]$task_builder.Append(' ')
            }
            $task_index++
            continue
        }

        if ($task_state -eq 'BlockComment') {
            if ($task_char -eq '*' -and $task_next -eq '/') {
                [void]$task_builder.Append('  ')
                $task_index += 2
                $task_state = 'Code'
                continue
            }
            [void]$task_builder.Append($(if ($task_char -eq "`r" -or $task_char -eq "`n") { $task_char } else { ' ' }))
            $task_index++
            continue
        }

        if ($task_state -eq 'String' -or $task_state -eq 'Character') {
            $task_closing = if ($task_state -eq 'String') { '"' } else { "'" }
            if ($task_char -eq '\\' -and $task_index + 1 -lt $Text.Length) {
                [void]$task_builder.Append(' ')
                $task_index++
                $task_escaped = $Text[$task_index]
                [void]$task_builder.Append($(if ($task_escaped -eq "`r" -or $task_escaped -eq "`n") { $task_escaped } else { ' ' }))
                $task_index++
                continue
            }
            if ($task_char -eq $task_closing) {
                [void]$task_builder.Append(' ')
                $task_index++
                $task_state = 'Code'
                continue
            }
            [void]$task_builder.Append($(if ($task_char -eq "`r" -or $task_char -eq "`n") { $task_char } else { ' ' }))
            $task_index++
        }
    }

    $task_builder.ToString()
}

function Get-SourceRecords {
    $task_records = [System.Collections.Generic.List[object]]::new()
    foreach ($task_scope in $script:SourceRoots) {
        $task_root = Join-Path $script:RepositoryRoot $task_scope
        $task_files = Get-ChildItem -LiteralPath $task_root -Recurse -File |
            Where-Object { $_.Extension -in @('.h', '.hpp', '.cpp') } |
            Sort-Object FullName

        foreach ($task_file in $task_files) {
            $task_original = [System.IO.File]::ReadAllText($task_file.FullName)
            $task_relative = [System.IO.Path]::GetRelativePath($script:RepositoryRoot, $task_file.FullName).Replace('\', '/')
            $task_records.Add([pscustomobject]@{
                Scope = $task_scope
                Path = $task_relative
                Original = $task_original
                Code = Hide-CppNonCode -Text $task_original
                OriginalLines = @([regex]::Split($task_original, "`r?`n"))
            })
        }
    }
    @($task_records)
}

function Get-FeatureStatistics {
    param([Parameter(Mandatory)][object[]]$Sources)

    $task_statistics = [System.Collections.Generic.List[object]]::new()
    foreach ($task_feature in $script:Features) {
        $task_regex = [regex]::new($task_feature.Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
        $task_counts = @{ engine = 0; game = 0; tests = 0 }
        $task_files = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

        foreach ($task_source in $Sources) {
            $task_matches = $task_regex.Matches($task_source.Code)
            if ($task_matches.Count -gt 0) {
                $task_counts[$task_source.Scope] += $task_matches.Count
                [void]$task_files.Add($task_source.Path)
            }
        }

        $task_total = $task_counts.engine + $task_counts.game + $task_counts.tests
        $task_statistics.Add([pscustomobject]@{
            Definition = $task_feature
            Engine = $task_counts.engine
            Game = $task_counts.game
            Tests = $task_counts.tests
            Total = $task_total
            Files = $task_files.Count
        })
    }
    @($task_statistics)
}

function Get-UnclassifiedStdSymbols {
    param([Parameter(Mandatory)][object[]]$Sources)

    $task_known = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($task_symbol in $script:BaselineStdSymbols) { [void]$task_known.Add($task_symbol) }
    foreach ($task_feature in $script:Features) {
        foreach ($task_symbol in $task_feature.Symbols) { [void]$task_known.Add($task_symbol) }
    }

    $task_unknown = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $task_std_regex = [regex]::new('\bstd::([A-Za-z_]\w*)')
    foreach ($task_source in $Sources) {
        foreach ($task_match in $task_std_regex.Matches($task_source.Code)) {
            $task_symbol = $task_match.Groups[1].Value
            if (-not $task_known.Contains($task_symbol)) { [void]$task_unknown.Add($task_symbol) }
        }
    }
    @($task_unknown | Sort-Object)
}

function Assert-RegistryAndSources {
    param(
        [Parameter(Mandatory)][object[]]$Sources,
        [Parameter(Mandatory)][object[]]$Statistics
    )

    $task_duplicate_ids = $script:Features | Group-Object Id | Where-Object Count -gt 1
    if ($task_duplicate_ids) {
        throw "Duplicate feature ids: $($task_duplicate_ids.Name -join ', ')"
    }

    $task_unknown = @(Get-UnclassifiedStdSymbols -Sources $Sources)
    if ($task_unknown.Count -gt 0) {
        throw "Unclassified std:: symbols: $($task_unknown -join ', ')"
    }

    foreach ($task_statistic in $Statistics | Where-Object Total -gt 0) {
        $task_definition = $task_statistic.Definition
        if ([string]::IsNullOrWhiteSpace($task_definition.Example)) {
            throw "Used feature '$($task_definition.Id)' has no representative source."
        }
        $task_example = $Sources | Where-Object Path -eq $task_definition.Example | Select-Object -First 1
        if ($null -eq $task_example) {
            throw "Representative source '$($task_definition.Example)' for '$($task_definition.Id)' does not exist."
        }
        if (-not [regex]::IsMatch($task_example.Code, $task_definition.Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)) {
            throw "Representative source '$($task_definition.Example)' no longer matches '$($task_definition.Id)'."
        }
    }
}

function ConvertTo-DocumentLink {
    param([Parameter(Mandatory)][string]$RepositoryRelativePath)
    "../../$RepositoryRelativePath"
}

function New-GeneratedMarkdown {
    param(
        [Parameter(Mandatory)][object[]]$Sources,
        [Parameter(Mandatory)][object[]]$Statistics
    )

    $task_lines = [System.Collections.Generic.List[string]]::new()
    $task_lines.Add($script:GeneratedBegin)
    $task_lines.Add('')
    $task_lines.Add("本区块由 ``scripts/audit_cpp_language_features.ps1`` 根据当前工作树生成；共扫描 $($Sources.Count) 个第一方 C++ 源文件。")
    $task_lines.Add('“命中”表示过滤注释及字符串/字符字面量后的检测模式次数，不等同于运行时调用次数。')

    foreach ($task_standard in @('C++11', 'C++14', 'C++17', 'C++20', 'C++23')) {
        $task_used = @($Statistics | Where-Object { $_.Definition.Standard -eq $task_standard -and $_.Total -gt 0 } | Sort-Object { $_.Definition.Id })
        if ($task_used.Count -eq 0) { continue }

        $task_lines.Add('')
        $task_lines.Add("### $task_standard")
        $task_lines.Add('')
        $task_lines.Add('| ID | 分类 | 特性 | engine | game | tests | 总命中 | 文件数 | 代表性位置 |')
        $task_lines.Add('| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |')
        foreach ($task_statistic in $task_used) {
            $task_definition = $task_statistic.Definition
            $task_kind = if ($task_definition.Kind -eq 'Language') { '语言' } else { '标准库' }
            $task_example_link = ConvertTo-DocumentLink -RepositoryRelativePath $task_definition.Example
            $task_lines.Add("| ``$($task_definition.Id)`` | $task_kind | $($task_definition.Name) | $($task_statistic.Engine) | $($task_statistic.Game) | $($task_statistic.Tests) | $($task_statistic.Total) | $($task_statistic.Files) | [源码]($task_example_link) |")
        }
    }

    $task_unused = @($Statistics | Where-Object { $_.Definition.CommonUnused -and $_.Total -eq 0 } | Sort-Object { $_.Definition.Standard }, { $_.Definition.Id })
    if ($task_unused.Count -gt 0) {
        $task_lines.Add('')
        $task_lines.Add('### 常见但当前未检出')
        $task_lines.Add('')
        $task_lines.Add('| 标准 | ID | 分类 | 特性 |')
        $task_lines.Add('| --- | --- | --- | --- |')
        foreach ($task_statistic in $task_unused) {
            $task_definition = $task_statistic.Definition
            $task_kind = if ($task_definition.Kind -eq 'Language') { '语言' } else { '标准库' }
            $task_lines.Add("| $($task_definition.Standard) | ``$($task_definition.Id)`` | $task_kind | $($task_definition.Name) |")
        }
    }

    $task_lines.Add('')
    $task_lines.Add($script:GeneratedEnd)
    $task_lines -join "`n"
}

function Get-DocumentGeneratedBlock {
    param([Parameter(Mandatory)][string]$Document)

    $task_pattern = '(?s)' + [regex]::Escape($script:GeneratedBegin) + '.*?' + [regex]::Escape($script:GeneratedEnd)
    $task_match = [regex]::Match($Document, $task_pattern)
    if (-not $task_match.Success) {
        throw "Generated markers were not found in '$script:DocumentPath'."
    }
    $task_match.Value.Replace("`r`n", "`n")
}

function Update-DocumentGeneratedBlock {
    param([Parameter(Mandatory)][string]$GeneratedBlock)

    $task_document = [System.IO.File]::ReadAllText($script:DocumentPath)
    $task_newline = if ($task_document.Contains("`r`n")) { "`r`n" } else { "`n" }
    $task_replacement = $GeneratedBlock.Replace("`n", $task_newline)
    $task_pattern = '(?s)' + [regex]::Escape($script:GeneratedBegin) + '.*?' + [regex]::Escape($script:GeneratedEnd)
    if (-not [regex]::IsMatch($task_document, $task_pattern)) {
        throw "Generated markers were not found in '$script:DocumentPath'."
    }
    $task_updated = [regex]::Replace($task_document, $task_pattern, [System.Text.RegularExpressions.MatchEvaluator]{ param($task_match) $task_replacement }, 1)
    [System.IO.File]::WriteAllText($script:DocumentPath, $task_updated, [System.Text.UTF8Encoding]::new($false))
}

function Write-FeatureMatches {
    param(
        [Parameter(Mandatory)][object]$Definition,
        [Parameter(Mandatory)][object[]]$Sources
    )

    $task_regex = [regex]::new($Definition.Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    $task_total = 0
    foreach ($task_source in $Sources) {
        $task_matches = $task_regex.Matches($task_source.Code)
        if ($task_matches.Count -eq 0) { continue }

        $task_line = 1
        $task_cursor = 0
        foreach ($task_match in $task_matches) {
            while ($true) {
                $task_newline = $task_source.Code.IndexOf("`n", $task_cursor)
                if ($task_newline -lt 0 -or $task_newline -ge $task_match.Index) { break }
                $task_line++
                $task_cursor = $task_newline + 1
            }
            $task_original_line = if ($task_line -le $task_source.OriginalLines.Count) { $task_source.OriginalLines[$task_line - 1].Trim() } else { '' }
            Write-Output "$($task_source.Path):$task_line`: $task_original_line"
            $task_total++
        }
    }
    Write-Output "Total matches: $task_total"
}

function Invoke-SelfTest {
    $task_fixture = @'
// std::expected<int, int> hidden_in_comment;
const char* hidden = "std::span<int> hidden in string";
const char marker = 'x';
const char* raw = R"tag(std::optional<int> hidden in raw string)tag";
auto value = 1;
auto callback = [](auto input) { return input; };
auto [first, second] = pair;
std::span<int> values;
std::expected<int, int> result = 1;
constexpr auto order = left <=> right;
'@
    $task_code = Hide-CppNonCode -Text $task_fixture
    $task_expectations = @{
        'cpp11-auto' = 3
        'cpp11-lambda' = 1
        'cpp14-generic-lambda' = 1
        'cpp17-structured-bindings' = 1
        'cpp20-span' = 1
        'cpp20-three-way-comparison' = 1
        'cpp23-expected' = 1
        'cpp17-optional' = 0
    }
    foreach ($task_entry in $task_expectations.GetEnumerator()) {
        $task_definition = $script:Features | Where-Object Id -eq $task_entry.Key | Select-Object -First 1
        if ($null -eq $task_definition) { throw "Self-test references unknown feature '$($task_entry.Key)'." }
        $task_actual = [regex]::Matches($task_code, $task_definition.Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline).Count
        if ($task_actual -ne $task_entry.Value) {
            throw "Self-test failed for '$($task_entry.Key)': expected $($task_entry.Value), got $task_actual."
        }
    }

    $task_fake_source = [pscustomobject]@{ Code = 'std::definitely_unknown_symbol value;'; Path = 'fixture.cpp'; Scope = 'tests' }
    $task_unknown = @(Get-UnclassifiedStdSymbols -Sources @($task_fake_source))
    if ($task_unknown.Count -ne 1 -or $task_unknown[0] -ne 'definitely_unknown_symbol') {
        throw 'Self-test failed to detect an unclassified std:: symbol.'
    }

    Write-Output 'Self-test passed: filtering, feature detection, and symbol classification are working.'
}

if ($SelfTest) {
    Invoke-SelfTest
    exit 0
}

$task_sources = Get-SourceRecords
$task_statistics = Get-FeatureStatistics -Sources $task_sources
Assert-RegistryAndSources -Sources $task_sources -Statistics $task_statistics
$task_generated = New-GeneratedMarkdown -Sources $task_sources -Statistics $task_statistics

if ($Feature) {
    $task_definition = $script:Features | Where-Object Id -eq $Feature | Select-Object -First 1
    if ($null -eq $task_definition) {
        throw "Unknown feature id '$Feature'. Valid ids: $(($script:Features.Id | Sort-Object) -join ', ')"
    }
    Write-Output "$($task_definition.Id) - $($task_definition.Standard) - $($task_definition.Name)"
    Write-FeatureMatches -Definition $task_definition -Sources $task_sources
    exit 0
}

if ($Update) {
    Update-DocumentGeneratedBlock -GeneratedBlock $task_generated
    Write-Output "Updated generated statistics in '$script:DocumentPath'."
    exit 0
}

if ($Check) {
    $task_document = [System.IO.File]::ReadAllText($script:DocumentPath)
    $task_existing = Get-DocumentGeneratedBlock -Document $task_document
    if ($task_existing -cne $task_generated) {
        throw "Generated C++ feature statistics are stale. Run '$($MyInvocation.MyCommand.Path) -Update'."
    }
    Write-Output 'C++ feature statistics are current.'
    exit 0
}

Write-Output $task_generated
