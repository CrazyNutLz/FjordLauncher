# 客户端自动更新

自 `11.0.3.3` 起支持单文件更新、按名称删除、ZIP 更新和静默更新。业务代码位于本目录，网络复用 NetJob/Download，磁盘扫描和安装运行在后台线程。

## 部署

1. 将 [ClientUpdateInfo.example.json](ClientUpdateInfo.example.json) 作为新 API 起点，发布前核对路径、下载内容和 SHA-256。文件和 ZIP 地址可使用 HTTP 或 HTTPS，包括 AList 的协议间跳转。
2. 先上传客户端文件/ZIP，核对公开下载内容的 SHA-256，再将清单发布到 `https://nutnode.top/software/fjrod-java25/ClientUpdateInfo.json`。
3. 自 11.0.3.5 起，启动器打开后自动检查并应用更新，不需要任何实例绑定或启用开关。
4. 帮助菜单保留“检查客户端更新/修复”以便重试。更新期间禁止启动游戏；失败或取消后，修复成功才能启动。命令行启动游戏也会先等待此次检查。

不兼容旧的 `updatefile`/MD5 协议。服务端仍使用旧协议时，新版会提示清单格式错误，并阻止游戏启动。

## 实例和路径如何对应

API 直接按路径指定文件，不依赖选中实例。`files.path`、`remove.dir` 和 `archives.path` 全部相对于启动器数据目录（Application::dataRoot）。便携发行版通常就是启动器所在目录；如果使用 `--dir` 指定数据目录，则以指定目录为根。

例如启动器数据目录是 `D:/MC`：

```text
API path: instances/GT_New_Horizons_2.8.0_Java_17-25/.minecraft/config/defaultserverlist.cfg
实际路径: D:/MC/instances/GT_New_Horizons_2.8.0_Java_17-25/.minecraft/config/defaultserverlist.cfg
```

路径必须准确写出实际目录名，包括 `minecraft` 或 `.minecraft`；写错目录会按该路径创建文件，不会猜测实例。也可写 `launcher-assets/banner.png` 或根目录文件 `server-info.txt`。不支持磁盘绝对路径和 `../`，不在 JSON 下划线前加反斜杠。url 填纯 HTTP/HTTPS 地址，不使用 Markdown 链接；sha256 填 64 位哈希，不带空格。

## 清单字段

完整字段与示例见 [客户端自动更新设计方案](../../../docs/客户端自动更新设计方案.md)。顶层支持 `silent`、`force`、`notice`、`files`、`remove`、`archives`，不填写文件大小或版本号。

- 自 11.0.3.7 起，files/remove/archives 每条规则都支持 `silent`、`force`、`notice`，省略时分别继承顶层值（默认 true、false、空文本）。`notice` 拼写如此，不是 `notic`；填写空字符串可覆盖顶层说明。
- 只汇总本机实际需要变更的规则：说明去重，列出本批安装/替换和删除文件；历史已完成、seed 保留及删除未命中的条目不加入说明，也不触发强制更新。大列表可展开详细信息，成功后完整摘要保存在 `.nutmod-update/state.json`。自 11.0.3.10 起，原说明菜单替换为“检查启动器更新”，可手动测试启动器自更新。
- 待更新规则中只要有一条 `silent=false`，就弹一次汇总（包含本批所有待更新文件）；全部为 true 则静默。ZIP 展开文件继承所属 ZIP 规则。
- 待更新规则中只要有一条 `force=true`，本批事务就不可取消，确认框仅有确定，关闭/Esc/菜单取消均禁用；全部 false 才允许取消。已完成的强制规则不影响其他更新。
- 取消会取消本批全部变更，不拆分事务。强制策略在本地差异计划生成后生效；清单读取、文件扫描和 ZIP 索引准备期间尚可取消。网络错误/超时仍正常失败，不无限重试。
- 自 11.0.3.8 起，本次弹出更新确认且用户点了确定，安装成功后再弹一次完成提示（含更新文件数）。纯静默、无变更不弹成功提示，失败只弹错误。手动检查也遵循此规则。
- 所有更新都在主窗口底部（实例信息、总游玩时长同一行）显示状态：下载百分比和 KiB、解压目标、安装完成数。结束后保留完成/已是最新/未完成文字，完整状态可悬停查看，不会立即消失。
- `files` 默认 `policy=managed`，按 SHA-256 同步；`seed` 仅添加缺失文件。
- `remove` 的 `dir` 限定范围，`match=exact/contains` 对文件名不区分大小写匹配，`recursive` 默认 false；可选 sha256 与名称条件同时生效。
- `archives` 只接受 ZIP；`path` 是目标目录，`.` 表示启动器数据根目录；`source` 选择并剥离包内前缀；`policy` 与单文件一致。
- 文件和 ZIP 展开目标优先保留，不会被包含匹配误删。多个安装条目写入同一文件时报错，不靠排列顺序覆盖。
- 可更新启动器数据目录内的文件。路径不能越界或穿过链接；任意层级的 `.nutmod-*` 更新状态及锁文件禁止作为目标。正在被占用的文件可能导致更新失败并回滚，启动器 EXE/运行库升级仍建议使用已有的启动器自更新渠道。

files/archives 保持完整目标清单，删除规则保留到对应旧客户端不再使用。新版 ZIP 不再包含的文件不会自动删除，需要 remove 指定。

## 累计更新的维护方式

可以长期保留所有仍需维护的文件。旧客户端下载后会比较整个清单，一次补齐缺失或过期文件；已更新的用户只处理新增差异，不需要按历史版本逐次升级，也不会重复显示旧公告。

同一路径始终只保留最新目标的 URL/SHA-256/notice，不能把该文件十个历史版本同时列出。不同文件可以不断累加。模组改名时保留新文件，再用 remove 删除旧名字。多个 ZIP 或 ZIP 与 files 不能写入相同目标；重叠时合并为最新 ZIP 或分开目标。下载链接需保持可用，尤其是有有效期的 AList 签名。

例如某个 files 条目可配置：

```json
{
  "path": "instances/GT_New_Horizons_2.8.0_Java_17-25/.minecraft/config/defaultserverlist.cfg",
  "sha256": "F6299893F70258D246433DD55AE39A434AABC4A523667089AF38C0DB1F270C85",
  "url": "http://example.com/defaultserverlist.cfg",
  "policy": "managed",
  "silent": false,
  "force": true,
  "notice": "更新服务器列表，请使用新的服务器入口。"
}
```

示例下载地址需要替换，并重新核对实际下载内容的 SHA-256。

## 生成 SHA-256 清单

先编写包含 path/url 等字段的输入 JSON，sha256 可以先留空，脚本按本地发布文件填入哈希：

```powershell
.\scripts\build-client-update-manifest.ps1 `
  -InputManifest .\release\client-input.json `
  -LauncherDirectory 'D:\MC' `
  -OutputManifest .\dist\ClientUpdateInfo.json
```

有 ZIP 时再提供 URL 到本地 ZIP 的映射：

```powershell
-ArchiveFiles @{ 'https://example.com/client.zip' = 'D:\release\client.zip' }
```

脚本不上传文件，不替你改变服务器；客户端应用前还会执行完整协议和内容校验。删除规则若填写 sha256，必须使用需要删除的旧文件的真实哈希。

## 启动互斥与恢复

更新下载、解压、替换、删除、提交与恢复期间，当前启动器所有新启动请求都会被拦截。数据根目录及实例/游戏目录使用 `.nutmod-session.lock` 实现跨进程保护，游戏启动流程保持锁到游戏退出；统一更新前检查所有已加载实例的锁和游戏进程记录。游戏目录的 `.nutmod-game.json` 记录 PID 和进程启动时间，即使启动器异常退出，也能识别仍然运行的 Java 进程。启动按钮禁用并提示“请在更新完成后再启动游戏”，双击、菜单与快捷键也受后端门禁保护。

非强制更新下载可以通过菜单取消；强制更新及提交/恢复阶段不允许取消。更新期间禁止关闭主窗口或实例编辑窗口，编辑面板暂停操作。普通启动器启动和手动更新完成后不自动启动游戏；命令行明确请求启动游戏时，检查成功后继续原请求。

工作目录位于 **启动器数据根目录内**的 `.nutmod-update/`，目标路径不允许跨越链接：

| 路径 | 用途 |
| --- | --- |
| `manifest.json` | 最近下载的清单 |
| `cache/` | 按 SHA-256 保存并复核的下载缓存 |
| `indexes/` | ZIP 展开文件的本地索引，用于检查同包文件是否损坏 |
| `expanded/` | ZIP 暂存展开内容 |
| `transactions/<id>/backup/` | 覆盖或删除前的旧文件，成功后保留最近一次事务备份 |
| `journal.json` | 恢复日志；pending 表示必须恢复，committed 表示成功 |
| `state.json` | 最近成功事务和更新说明，不作为跳过文件验证的依据 |

普通安装失败会立即恢复；进程异常退出时，下次更新先恢复再联网。恢复时发现外部修改或备份缺失会停止，不盲目覆盖。请保留工作目录以便修复。

手动修复重建 ZIP 索引并完整检查文件；索引丢失时需要已有 ZIP 缓存或重新下载。校验通过的缓存可复用，因此同版损坏不一定需要重新联网下载文件。缓存不随成功备份一起删除，维护者可在启动器关闭且无 pending 事务时清理 cache/indexes/expanded 以回收空间。

当前限制：清单 4 MiB，最多 10000 条规则/操作；单文件或 ZIP 2 GiB，单次下载和展开内容分别最多 20 GiB。下载有限重试、清单 30 秒及文件 5 分钟超时，不无限等待。

## 完整客户端打包

`package-client-release.ps1` 不再写入实例绑定配置，并清除旧的 `NutModClientUpdateEnabled` 标记。脚本排除根目录和实例中的更新工作目录、进程记录与锁文件，不分发用户备份、缓存和事务日志。

## 验证

```powershell
. .\scripts\dev-env.ps1
cmake --build --preset windows_msvc --config Debug
ctest --test-dir build -C Debug -R '^ClientUpdate$' --output-on-failure
```

测试使用临时目录，覆盖协议拒绝、managed/seed、精确/包含/递归删除、新文件保护、下载失败与取消、ZIP 修复与越界、文件冲突、逐次写入故障回滚、异常中断恢复及跨进程锁。
