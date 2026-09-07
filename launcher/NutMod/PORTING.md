# NutMod 迁移说明

目标是把 `launcher/NutMod` 整体复制到新版 Fjord 后，只对原版保留少量接入修改。迁移时先复制本目录，再按以下顺序让 AI 对照新版代码接入并编译。

## 1. 根 CMake 配置

在根 `CMakeLists.txt` 定义 Fjord 更新器地址之前：

```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/launcher/NutMod/config/NutModConfig.cmake")
```

把启动器更新地址设为：

```cmake
set(Launcher_UPDATER_GITHUB_REPO "${NUTMOD_LAUNCHER_UPDATE_MANIFEST_URL}" CACHE STRING "Update manifest URL." FORCE)
```

Windows 构建必须提供非空 `Launcher_BUILD_ARTIFACT`，以启用外部更新器。

## 2. Launcher 构建目标

NutMod 按功能分为 `config/`、`bootstrap/`、`ui/`、`notices/`、`launcher_update/`、`client_update/` 和 `resources/`。复制时保留目录结构。

原版接入文件的头文件路径使用 `NutMod/bootstrap/NutModBootstrap.h`、`NutMod/ui/NutModUi.h` 或 `NutMod/launcher_update/LauncherUpdateManifest.h`。跨模块调用接口未改变，仅更新 include 路径。

`NutMod.cmake` 从 `config/NutModConfig.h.in` 生成构建目录中的 `NutMod/config/NutModConfig.h`；源代码通过此完整路径引用配置头。

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
- `backupAppDir()` 必须读取新更新包目录中的 `manifest.txt`，不能根据旧安装目录猜测备份范围，否则 Quick 增量包会误删 Qt 运行库和插件。
- `PrismExternalUpdater` 在子进程协议中读取 `Mandatory`，将标题、版本和强制状态传给更新窗口。
- `ExternalUpdater`/`PrismExternalUpdater` 将本次检查是否发现更新通知给 `MainWindow`；主工具栏更新按钮初始隐藏，仅在确实存在新版本时显示。
- `PrismExternalUpdater` 调用 `NutMod::alwaysCheckLauncherUpdatesOnStartup()`，确保每次启动都检查，并通过 `NutMod::allowSkippingLauncherUpdates()` 禁止旧的跳过记录生效。
- `UpdateAvailableDialog` 调用 `NutMod::customizeLauncherUpdateDialog()`：始终隐藏“跳过该版本”；强制更新时再隐藏“稍后提醒”和标题栏关闭按钮，同时保留 `reject()` 拦截。

## 7. 服务器公告

- 公告地址由 `NUTMOD_NOTICE_MANIFEST_URL` 统一配置，当前使用 `notice.json`。
- `Application.cpp` 在创建外部更新器后监听 `ExternalUpdater::startupCheckFinished()`；只有启动器更新检查结束且未开始安装时，才调用 `NutMod::checkServerNotices()`。
- `ExternalUpdater` 增加 `startupCheckFinished(bool)` 信号，`PrismExternalUpdater` 的自动检查完成后发出该信号。
- `MainWindow.cpp` 调用 `NutMod::addServerNoticeAction()`，在帮助菜单的“关于”前加入手动“服务器公告”入口。
- 公告解析、网络请求、弹窗和已读状态都位于 `NutMod/notices/Notice*`，原版代码不保存公告 API 或服务器文案。

公告已读状态使用 `NutModSeenNoticeVersions` 字符串列表保存，支持多条 `once` 公告；手动入口会忽略已读状态并显示接口中的所有公告。

## 8. 客户端更新（11.0.3.5）

11.0.3.10：ClientUpdateService 帮助菜单以“检查启动器更新”替换原说明入口，调用 Application::triggerUpdateCheck，检查前保留游戏和更新操作互斥；完成提示不再引用旧入口。

11.0.3.9：MainWindow 底部三段内容置于统一水平布局，设置内边距/段间距和随主题变化的分隔线；实例、时长原有显示逻辑不变。

11.0.3.8：Task 记录是否实际确认，Service 在成功且有变更时据此弹一次完成提示；手动检查不再额外弹成功提示。Service 保留结束状态，MainWindow 底部更新标签不随 busy=false 隐藏，并显示下载大小与安装完成数。

11.0.3.7：每条规则也支持 force/notice。Engine 的 planReady 在差异扫描完成后向 Task 传递本批有效 force；仅待更新规则参与 force、silent 和公告汇总，不能再仅按顶层 force 设置 UI。ZIP 使用当前规则元数据，成功后 state.notice 保存本次完整摘要。长确认列表由对话框详细信息完整展示。

11.0.3.6 补充：`ClientUpdateManifest` 支持逐规则 silent（可选、继承顶层）和顶层 force。Engine 按实际变更聚合非静默确认列表，ZIP 索引重用时从当前规则取得 silent。Task 读取清单后在 UI 线程设置强制取消策略，确认框拦截关闭/Esc，Service 监听 abortStatusChanged 更新取消菜单；超时和网络失败不受 force 限制。

- `Application::performMainStartupAction()` 先显示主窗口，调用 `ClientUpdateService::automatic()`；完成后继续原启动流程。普通启动、命令行启动均检查一次，不依赖实例绑定。API 路径以 `Application::dataRoot()` 为根。
- `Application::launch()` 统一拦截更新中的启动请求，在排队控制器前调用 `ClientUpdateService::reserveLaunch()`，实例和游戏目录的锁保留到控制器结束。`Application` 提供专用的客户端更新计数入口，与启动器自更新互斥。
- `LaunchController::executeTask()` 不再重复发起更新；最终 `readyForLaunch()` 记录 Java 进程 PID/启动时间，放行后释放准备阶段门禁，数据目录与实例锁保留到游戏结束。统一更新先检查所有已加载实例的锁和进程记录，识别启动器异常退出后仍在运行的游戏。
- `MainWindow` 添加手动检查/修复、说明和取消菜单，无启用开关；监听服务信号显示进度并禁用启动。关闭、退出快捷键及删除/复制/重命名入口在更新时被拦截。更新失败或取消后后端继续阻止启动，修复成功才放行。
- `InstanceWindow` 在更新时禁用编辑面板、暂停保存/关闭并禁用启动。`Application::ShowGlobalSettings()` 防止设置重载使更新中的实例对象失效。
- `NetRequest` 增加可选 `RedactUrl` 标志，客户端更新请求隐藏状态、调试日志和错误响应中的签名 URL；`Download::makeFile()` 同时隐藏 Task 名称中的 URL。其他请求保持原来的显示。
- `ClientUpdateTask` 复用 NetJob、Download；清单使用 HTTPS，文件/ZIP 从 11.0.3.4 起允许 HTTP/HTTPS 及两者间跳转，要求 HTTP 状态 200、流式 SHA-256 校验、大小限制、重试和超时。ZIP 使用项目已经链接的 libarchive，在本模块中逐项安全解压，避免修改通用解压器接口。
- `scripts/build-client-update-manifest.ps1` 使用 `-LauncherDirectory` 计算发布清单哈希；`package-client-release.ps1` 清除旧绑定标记并排除根目录及实例本地更新状态。新版本示例见 `client_update/ClientUpdateInfo.example.json`。
- 测试注册在 `tests/CMakeLists.txt`，运行 `ctest --test-dir build -C Debug -R '^ClientUpdate$' --output-on-failure`。

## 9. 接入点识别

当前原版文件中的新增挂载位置尽量使用以下注释：

```cpp
// NUTMOD INTEGRATION POINT: ...
```

迁移前可使用：

```powershell
rg -n "NUTMOD INTEGRATION POINT|NutMod::|NUTMOD_" CMakeLists.txt buildconfig launcher
```

迁移完成后必须执行 Windows MSVC Debug 编译，处理新版 Fjord 的接口变化。
