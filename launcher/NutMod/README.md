# NutMod

这里集中存放飞翔的大雕服务器启动器的自定义功能，便于与 Fjord 原始代码区分。

## API 与品牌配置

所有服务端地址、验证链、官网、默认语言/主题和版本通道集中在 `config/NutModConfig.cmake`。迁移域名时，通常只需要修改：

```cmake
set(NUTMOD_API_BASE_URL "https://example.com/software/fjrod-java25")
```

当前规划的接口：

- `launcher.json`：启动器本体更新。
- `ClientUpdateInfo.json`：客户端文件自动更新，使用新协议。
- `notice.json`：自定义公告，已实现。

## 模块边界

自定义协议解析和新功能实现放在本目录。Fjord 原文件只保留必要的初始化、界面和调用接入点。

按功能组织：

```text
NutMod/
├── config/           # API、品牌配置与生成头文件模板
├── bootstrap/        # 默认设置、功能策略与初始化协调
├── ui/               # 账号页、登录页、关于页等公共界面定制
├── notices/          # 公告解析、请求服务与公告弹窗
├── launcher_update/  # 启动器自身更新清单解析
├── client_update/    # 客户端文件、删除规则、ZIP、备份恢复与启动门禁
├── resources/        # 内置简体中文翻译与 Qt 资源
├── NutMod.cmake      # 构建入口、源文件注册
├── README.md
└── PORTING.md        # 迁移接入清单
```

客户端更新使用方法见 [client_update/README.md](client_update/README.md)，版本记录见 [CHANGELOG.md](CHANGELOG.md)。功能专属 UI（如公告弹窗）跟随对应功能目录；多个功能共用的界面定制放在 `ui/`。

跨目录引用使用 `NutMod/<功能目录>/<文件>.h`，同目录引用保留文件名。生成配置头位于构建目录的 `NutMod/config/NutModConfig.h`，不提交生成文件。现有类名、命名空间和函数接口保持不变。
