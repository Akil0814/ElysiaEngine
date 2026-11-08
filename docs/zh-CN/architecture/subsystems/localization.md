# 本地化与文字缓存架构

本文解释翻译、字体解析与文字纹理的协作；游戏接入见[本地化指南](../../user-guide/systems/localization.md)。本地化在应用初始化阶段就绪，早于项目内容加载；项目字体加载完成后再激活，见[启动全流程](../startup.md)。

<a id="localization-service-architecture"></a>
## 本地化服务架构

服务统一翻译、测量和文字纹理入口；Manager 持有翻译表及缓存，并借用应用的 FontResolver。

```mermaid
flowchart TB
  Caller["游戏 / UI 文本控件"]
  Service["LocalizationService / ELYSIA_LOCALIZATION"]
  App["Application<br/>初始化与关闭"]
  Manager["LocalizationManager<br/>当前语言、翻译查询与文字生成"]
  Tables["I18nManifest / TranslationTable<br/>当前语言与默认语言回退"]
  Cache["TextTextureCache<br/>语言、样式、字体代次组成缓存键"]
  Resolver["FontResolver<br/>字体来源与代次"]
  Texture["SDL_ttf / SDL_Renderer<br/>文字测量与纹理生成"]
  Caller -->|翻译、测量、文字纹理与语言切换| Service
  Service -->|转发| Manager
  App -->|initialize / shutdown| Manager
  Manager -->|持有已加载翻译表| Tables
  Manager -->|持有缓存；语言切换成功后清空| Cache
  Manager -.->|借用解析器；获取字体与代次| Resolver
  Manager -->|缓存未命中或显式不缓存时创建| Texture
  Cache -.->|返回借用纹理；清理后失效| Caller
```

缓存纹理返回借用；成功切换语言、清缓存或关闭后旧缓存指针失效。字体代次参与缓存键，避免复用旧字体纹理。create_uncached_raw_text_texture 返回调用方拥有的 CachedTexturePtr，与缓存借用不同；tr 的 string_view 也不延长翻译表寿命。

源码：[localization_service.cpp](../../../../engine/localization/localization_service.cpp) · [localization_manager.h](../../../../engine/localization/localization_manager.h) · [text_texture_cache.h](../../../../engine/localization/text_texture_cache.h)

相关入口：[服务架构总览](../services.md) · [架构首页](../README.md)。
