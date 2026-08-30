# NutMod 迁移说明

目标是把 `launcher/NutMod` 整体复制到新版 Fjord 后，只对原版保留少量接入修改。迁移时先复制本目录，再按以下顺序让 AI 对照新版代码接入并编译。

## 1. 根 CMake 配置

在根 `CMakeLists.txt` 定义 Fjord 更新器地址之前：

```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/launcher/NutMod/NutModConfig.cmake")
```

把启动器更新地址设为：

```cmake
set(Launcher_UPDATER_GITHUB_REPO "${NUTMOD_LAUNCHER_UPDATE_MANIFEST_URL}" CACHE STRING "Update manifest URL." FORCE)
```

Windows 构建必须提供非空 `Launcher_BUILD_ARTIFACT`，以启用外部更新器。

## 2. Launcher 构建目标

在 `launcher/CMakeLists.txt` 包含：

```cmake
include(NutMod/NutMod.cmake)
```

分别将以下列表加入原目标：

```cmake
${NUTMOD_LOGIC_SOURCES}
${NUTMOD_UPDATER_SOURCES}
${NUTMOD_RESOURCES}
```

## 3. 应用初始化

在 `Application.cpp`：

- 注册原版 `IconTheme`、`ApplicationTheme`、`Language` 后调用 `NutMod::applyDefaultSettings()`。
- 用 `NutMod::showMicrosoftLoginWizard()` 控制 Microsoft 登录向导。
- 用 `NutMod::fetchCurseForgeKeyOnStartup()` 控制 CurseForge API Key 提示。
- 用 `NutMod::allowLegacyDataMigration()` 控制旧 Prism/Fjord 数据迁移提示。

在 `main.cpp` 创建 `Application` 前调用：

```cpp
Q_INIT_RESOURCE(nutmod_translations);
```

在 `BuildConfig.cpp.in` 用 `@NUTMOD_VERSION_CHANNEL@` 覆盖显示通道。

## 4. 本地简体中文

`TranslationsModel.cpp` 需要三个接入点：

- 构造模型时，通过 `NutMod::useBundledChineseTranslation()` 注册 `zh`。
- 加载 PO 时，通过 `NutMod::bundledTranslationPath()` 读取 Qt 资源。
- 下载翻译索引前，通过 `NutMod::allowRemoteTranslationUpdates()` 禁止远程覆盖。

翻译文件本体位于 `NutMod/resources/zh.po`。

## 5. 账号与界面

- `AccountListPage.cpp` 在 `listView->setModel()` 和表头配置完成后调用 `NutMod::customizeAccountPage()`，并从 `NutModBootstrap` 获取空页面文案、登录文案和正版账号限制策略。必须把 `WideBar*` 原样传入，不能转换成 `QToolBar*`。
- `AuthlibInjectorLoginDialog.cpp` 调用 `NutMod::customizeAuthlibLoginDialog()`，验证链使用 `NutMod::authServerUrl()`。
- `AccountList.cpp` 用 `NutMod::accountTypeDisplayName()` 显示“雕版账号”。
- `AboutDialog.cpp` 在 `setupUi()` 后调用 `NutMod::decorateAboutPage()`。

这些定制不要求修改原版 `.ui` 文件。

## 6. 启动器更新器

外部更新器仍复用 Fjord 的下载、备份、覆盖和重启流程。新版迁移时需要让 AI 对照当前版本移植以下小型扩展：

- `GitHubReleaseAsset` 增加 `sha256`，`GitHubRelease` 增加 `mandatory`。
- `PrismUpdater` 对非 GitHub 地址读取单个 NutMod JSON，并调用 `NutMod::parseLauncherUpdateManifest()`。
- 下载后进行 SHA-256 校验。
- 解压前清理临时目录，并要求 ZIP 根目录包含更新器与 `manifest.txt`。
- `PrismExternalUpdater` 在子进程协议中读取 `Mandatory`，将标题、版本和强制状态传给更新窗口。
- `PrismExternalUpdater` 调用 `NutMod::alwaysCheckLauncherUpdatesOnStartup()`，确保每次启动都检查，并通过 `NutMod::allowSkippingLauncherUpdates()` 禁止旧的跳过记录生效。
- `UpdateAvailableDialog` 调用 `NutMod::customizeLauncherUpdateDialog()`：始终隐藏“跳过该版本”；强制更新时再隐藏“稍后提醒”和标题栏关闭按钮，同时保留 `reject()` 拦截。

## 7. 服务器公告

- 公告地址由 `NUTMOD_NOTICE_MANIFEST_URL` 统一配置，当前使用 `notice.json`。
- `Application.cpp` 在创建外部更新器后监听 `ExternalUpdater::startupCheckFinished()`；只有启动器更新检查结束且未开始安装时，才调用 `NutMod::checkServerNotices()`。
- `ExternalUpdater` 增加 `startupCheckFinished(bool)` 信号，`PrismExternalUpdater` 的自动检查完成后发出该信号。
- `MainWindow.cpp` 调用 `NutMod::addServerNoticeAction()`，在帮助菜单的“关于”前加入手动“服务器公告”入口。
- 公告解析、网络请求、弹窗和已读状态都位于 `NutMod/Notice*`，原版代码不保存公告 API 或服务器文案。

公告已读状态使用 `NutModSeenNoticeVersions` 字符串列表保存，支持多条 `once` 公告；手动入口会忽略已读状态并显示接口中的所有公告。

## 8. 接入点识别

当前原版文件中的新增挂载位置尽量使用以下注释：

```cpp
// NUTMOD INTEGRATION POINT: ...
```

迁移前可使用：

```powershell
rg -n "NUTMOD INTEGRATION POINT|NutMod::|NUTMOD_" CMakeLists.txt buildconfig launcher
```

迁移完成后必须执行 Windows MSVC Debug 编译，处理新版 Fjord 的接口变化。
