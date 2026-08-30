# NutMod

这里集中存放飞翔的大雕服务器启动器的自定义功能，便于与 Fjord 原始代码区分。

## API 与品牌配置

所有服务端地址、验证链、官网、默认语言/主题和版本通道集中在 `NutModConfig.cmake`。迁移域名时，通常只需要修改：

```cmake
set(NUTMOD_API_BASE_URL "https://example.com/software/fjrod-java25")
```

当前规划的接口：

- `launcher.json`：启动器本体更新。
- `client.json`：客户端文件更新，尚未实现。
- `notice17.json`：自定义公告，尚未实现。

## 模块边界

自定义协议解析和新功能实现放在本目录。Fjord 原文件只保留必要的初始化、界面和调用接入点。

当前模块：

- `NutModBootstrap.*`：默认设置和功能开关策略。
- `NutModUi.*`：账号页、登录页和关于页的界面定制。
- `LauncherUpdateManifest.*`：自定义启动器更新清单解析。
- `resources/`：启动器内置简体中文翻译。
- `PORTING.md`：迁移到新版 Fjord 时的接入清单。
