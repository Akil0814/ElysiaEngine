# C++ 代码规范

## 代码命名规范

- 类、枚举、结构体使用 `NameName`（PascalCase）
- 成员变量使用 `_name_name`
- 局部变量使用 `name_name`
- 函数参数使用 `name_name`
- 函数、成员函数使用 `name_name`
- 文件名使用 `name_name`

正例：

```cpp
class PathManager
{
private:
    std::filesystem::path _root_path;
};

struct PlayerData
{
    int hp;
};

enum class SceneType
{
    MainMenu,
    BattleScene
};

void set_player_data(const PlayerData& player_data);
```

反例：

```cpp
class path_manager {};
struct playerData {};
enum class scene_type {};

std::filesystem::path RootPath;
void SetPlayerData(const PlayerData& playerData);
```

## auto 使用约定

- 使用自建类创建变量时，优先显式写出类型，尽量避免使用 `auto`
- 仅在右值类型一眼可知，且显式书写会明显降低可读性时，才允许谨慎使用 `auto`

正例：

```cpp
PathManager* path_manager = PathManager::instance();
std::filesystem::path asset_path = root_path / "assets";
std::optional<std::filesystem::path> root_path = find_project_root(start_path);
```

反例：

```cpp
auto path_manager = PathManager::instance();
auto asset_path = root_path / "assets";
auto root_path = find_project_root(start_path);
```

## 代码完整性要求

- 确保每次提交的源码必须处于可编译状态
- 未完成实现可以先写 `TODO`

正例：

```cpp
std::optional<std::filesystem::path> PathManager::find_project_root(const std::filesystem::path& start_path) const
{
    // TODO: implement project root detection.
    (void)start_path;
    return std::nullopt;
}
```

反例：

```cpp
std::optional<std::filesystem::path> PathManager::find_project_root(const std::filesystem::path& start_path) const
{
    std::filesystem::path current = std::filesystem::absolute(start_path)
}
```
