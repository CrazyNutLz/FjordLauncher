# ModNut 更新与公告功能移植说明

## 1. 文档用途

本文用于指导另一个 AI，将当前 PCL 魔改版中的以下功能迁移到另一个启动器：

1. 启动器自身自动更新。
2. 服务端自定义公告。
3. 客户端文件自动更新，包括新增、替换、删除和压缩包解包。

本文描述现有程序的实现思路、调用流程、服务端 JSON 格式和新启动器需要提供的对接能力。不要机械照搬 PCL 专用类名，应根据目标启动器已有的网络、下载、UI、设置和日志系统进行替换。

## 2. 总体结构

现有实现把服务端交互集中在 `ModNut` 模块中，并统一保存服务端域名和当前启动器版本：

```vb
Public Const ServerDomain As String = "nutnode.top"
Public VersionNumber As Single = 214.0F
```

三个主要接口地址由同一个域名拼接：

```text
https://{ServerDomain}/software/server/pclinfo.json
https://{ServerDomain}/software/server/notice17.json
https://{ServerDomain}/software/server/ClientUpdateInfo2.json
```

建议目标启动器也集中保存以下配置：

- 服务端域名或服务端基础 URL。
- 当前启动器版本。
- 三个接口的相对路径。
- 公告版本持久化键名。

## 3. 启动时的调用顺序

当前程序在主窗口完成基础初始化后，启动一个后台线程，按以下顺序调用：

```text
基础环境和 Java 初始化
    ↓
CheckPCLUpdate()     检查启动器自身更新
    ↓
CheckFileUpdate()    检查客户端文件更新
    ↓
GetNotice()          获取并显示公告
    ↓
继续其余启动器初始化
```

在目标启动器中，建议将这三项放到窗口可用、网络组件已初始化之后执行。涉及弹窗或界面提示时，需要切回 UI 线程。

另外，手动点击“检查更新”按钮时，也直接调用 `CheckPCLUpdate()`。

## 4. 启动器自身更新

### 4.1 检查更新接口

请求：

```text
GET https://nutnode.top/software/server/pclinfo.json
Accept: application/json, text/javascript, */*; q=0.01
编码: UTF-8
```

JSON 对象需要包含：

```json
{
  "version": "214",
  "title": "发现启动器更新",
  "newtxt": "本次更新内容",
  "downloadurl": "https://example.com/launcher-update.zip"
}
```

字段含义：

| 字段 | 用途 |
| --- | --- |
| `version` | 服务端最新版本号，与本地 `VersionNumber` 比较 |
| `title` | 更新确认弹窗标题 |
| `newtxt` | 更新说明正文 |
| `downloadurl` | 启动器更新 ZIP 下载地址 |

### 4.2 检查流程

使用两个状态避免重复更新：

- `IsCheckUpdate`：当前是否正在检查更新。
- `IsUpdateStarted`：是否已经开始下载安装更新。

逻辑流程：

```text
检查两个状态
    ↓
标记正在检查
    ↓
下载并解析 pclinfo.json
    ↓
读取 version、title、newtxt、downloadurl
    ↓
服务端版本 <= 本地版本 → 记录“已是最新版”
    ↓
服务端版本 > 本地版本 → 显示确认弹窗
    ↓
用户确认 → 调用启动器更新安装函数 UpdateStart(downloadurl)
    ↓
检查结束后恢复 IsCheckUpdate
```

弹窗正文会在服务端更新说明前附加：

```text
当前启动器版本: 本地版本 ----> 最新版本: 服务端版本
```

### 4.3 下载和安装更新包

现有 `UpdateStart` 位于启动器原有更新模块中，主要流程如下：

1. 将 `IsUpdateStarted` 设为 `True`，避免重复启动。
2. 将下载 URL 中的 `{KEY}` 替换为 `Public`，以兼容带渠道占位符的下载地址。
3. 创建启动器自己的下载任务，把更新包保存到临时目录的 `Update.zip`。
4. 当前下载器对更新包设置了约 1 MB 的最低大小检查。
5. 下载完成后，将 ZIP 解压到启动器根目录下的 `PCL` 临时目录。
6. 更新包中预期存在 `PCL\Plain Craft Launcher 2.exe`。
7. 删除下载完成的 `Update.zip`。
8. 动态生成一个批处理文件 `Update{时间戳}.bat`。
9. 启动批处理文件，由它结束当前启动器进程。
10. 批处理多次尝试把临时目录中的新 EXE 覆盖到当前启动器 EXE 路径。
11. 删除临时的新 EXE，重新启动覆盖后的启动器。
12. 删除更新批处理自身。

伪代码：

```text
UpdateStart(downloadUrl):
    if updateAlreadyStarted:
        return

    updateAlreadyStarted = true
    realUrl = downloadUrl.replace("{KEY}", "Public")

    tasks = [
        Download(realUrl, temp/Update.zip),
        InstallTask:
            delete staging/PCL/launcher.exe
            unzip temp/Update.zip to staging/PCL/
            delete temp/Update.zip
            create replacement batch script
            start replacement batch script hidden
    ]

    start tasks as one combined loader
    show it in the launcher's download/task UI
```

目标启动器需要根据自己的目录结构调整：

- 当前运行 EXE 的完整路径。
- 更新包临时目录。
- ZIP 内新启动器 EXE 的位置和名称。
- 进程退出与文件覆盖方式。
- 更新后重新启动的命令。

启动器下次启动时，可以顺手删除上一次更新留下的临时 EXE。

## 5. 自定义公告

### 5.1 公告接口

请求：

```text
GET https://nutnode.top/software/server/notice17.json
Accept: application/json, text/javascript, */*; q=0.01
编码: UTF-8
```

接口返回 JSON 数组，每个元素是一条公告：

```json
[
  {
    "title": "公告标题",
    "notice": "公告正文",
    "version": "2026083001",
    "btn1": "确定",
    "btn2": "取消",
    "showtime": "once",
    "opt": "openurl",
    "opttxt": "https://example.com"
  }
]
```

### 5.2 字段说明

| 字段 | 用途 |
| --- | --- |
| `title` | 公告弹窗标题 |
| `notice` | 公告正文 |
| `version` | 公告唯一版本，用于判断是否已经显示 |
| `btn1` | 第一个按钮文字 |
| `btn2` | 第二个按钮文字 |
| `showtime` | 显示方式，支持 `always` 或 `once` |
| `opt` | 用户点击第一个按钮后执行的操作 |
| `opttxt` | 操作参数，例如网址或待复制文字 |

### 5.3 显示规则

启动器设置存储中使用 `NoticeVersion` 保存已经处理过的公告版本。

- `showtime = "always"`：每次获取到公告都显示。
- `showtime = "once"`：仅当公告的 `version` 与本地 `NoticeVersion` 不同时显示。

处理每条公告时：

1. 读取本地 `NoticeVersion`。
2. 如果读取失败，则初始化为 `0`。
3. 把当前公告的 `version` 写入 `NoticeVersion`。
4. 根据 `showtime` 决定是否弹窗。
5. 记录用户点击了哪个按钮。
6. 只有用户点击第一个按钮时，才执行 `opt` 指定的操作。

### 5.4 公告操作

现有实现支持：

| `opt` 值 | 行为 | `opttxt` 的含义 |
| --- | --- | --- |
| `openurl` | 使用系统浏览器打开网址 | URL |
| `clipbord` | 把内容复制到剪贴板 | 待复制文本 |
| `exit` | 立即退出启动器 | 可留空 |

注意：现有服务端协议中复制操作拼写为 `clipbord`，不是 `clipboard`。迁移时必须保持该拼写，除非同步修改服务端数据。

`opt` 为空字符串时，只显示公告，不执行附加操作。

## 6. 客户端文件自动更新

这里的“客户端文件”是启动器目录或其子目录中的模组、配置文件、资源文件等，不是启动器 EXE 自身。

### 6.1 更新规则接口

请求：

```text
GET https://nutnode.top/software/server/ClientUpdateInfo2.json
Accept: application/json, text/javascript, */*; q=0.01
编码: UTF-8
```

顶层格式：

```json
{
  "updatefile": [
    {
      "floderpath": "Game\\mods",
      "oldname": "ExampleMod-old.jar",
      "oldmd5": "旧文件的 MD5",
      "newname": "ExampleMod-new.jar",
      "newmd5": "新文件的 MD5",
      "downloadpath": "https://example.com/ExampleMod-new.jar",
      "shownotice": "true",
      "notice": "检测到模组更新",
      "opt": "replace",
      "enable": "true"
    }
  ]
}
```

注意：现有协议字段名是 `floderpath`，不是 `folderpath`。目标实现必须保持这个拼写，除非服务端也同步修改。

### 6.2 规则字段

| 字段 | 用途 |
| --- | --- |
| `floderpath` | 相对于启动器运行目录的目标文件夹 |
| `oldname` | 旧文件名或用于包含匹配的名称片段；在 `unpack` 中作为下载的 ZIP 文件名 |
| `oldmd5` | 旧文件 MD5，可为空 |
| `newname` | 下载后的新文件名；在 `unpack` 中作为版本记录键 |
| `newmd5` | 新文件 MD5，亦用于判断是否已经更新 |
| `downloadpath` | 文件或压缩包下载地址 |
| `shownotice` | 字符串 `true`/`false`，是否先询问用户 |
| `notice` | 更新提示正文 |
| `opt` | `add`、`replace`、`delete` 或 `unpack` |
| `enable` | 字符串 `true`/`false`，是否启用该规则 |

所有 MD5 和操作名称在读取后转为小写，并去除首尾空白。

### 6.3 通用处理流程

```text
下载并解析 ClientUpdateInfo2.json
    ↓
遍历 updatefile 数组
    ↓
enable = false → 跳过
    ↓
opt = unpack → 交给整包更新流程
    ↓
根据 floderpath 计算目标文件夹
    ↓
按照 add / replace / delete 执行规则
    ↓
输出规则数、解包数、下载数、删除数和已是最新的跳过数
```

同一次检查中使用一个不区分大小写的字典缓存文件 MD5，避免同一文件被不同规则重复计算。

### 6.4 `add` 操作

用途：目标文件不存在时下载新文件。

流程：

1. 如果目标目录不存在，则创建目录。
2. 直接检查 `目标目录 + newname` 是否存在。
3. 如果文件存在且 `newmd5` 为空，认为已经是最新版并跳过。
4. 如果文件存在且实际 MD5 等于 `newmd5`，认为已经是最新版并跳过。
5. 如果需要提示，显示“自动更新 / 打开下载链接 / 取消更新”三个选项。
6. 选择自动更新时，将 `downloadpath` 下载到目标目录并保存为 `newname`。
7. 选择打开下载链接时，使用系统浏览器打开 `downloadpath`。

### 6.5 `replace` 操作

用途：下载新文件，并替换一个旧文件。

查找旧文件的顺序：

1. 优先检查 `目标目录 + oldname` 这个精确路径。
2. 精确路径不存在时，在目标目录顶层查找文件名中包含 `oldname` 的文件。
3. 如果配置了 `oldmd5`，包含匹配的候选文件还需要 MD5 相同。
4. 如果没有配置 `oldname`、但配置了 `oldmd5`，扫描目标目录顶层并寻找 MD5 相同的文件。
5. 只选择找到的第一个旧文件。
6. 如果没有找到旧文件，把本条规则按 `add` 处理。

确定需要更新后：

1. 根据 `shownotice` 决定是否询问用户。
2. 下载 `downloadpath`，保存为 `newname`。
3. 如果旧文件仍然存在，并且旧文件名与 `newname` 不同，则删除旧文件。

### 6.6 `delete` 操作

用途：删除符合名称或 MD5 的文件。

匹配规则是“名称命中或 MD5 命中”，满足任意一个即可：

- 名称匹配：目标目录顶层文件名包含 `oldname`。
- MD5 匹配：目标目录顶层文件 MD5 等于 `oldmd5`。

兼容逻辑：如果 `oldmd5` 为空、`newmd5` 不为空，则把 `newmd5` 当作 `oldmd5` 使用。

所有命中的路径先放入一个不区分大小写的集合中去重。如果 `shownotice = "true"`，先询问用户是否删除；确认后逐个删除。

### 6.7 `unpack` 操作

用途：下载一个 ZIP 更新包并覆盖解压到指定目录。

版本状态保存在：

```text
启动器目录/PCL/PackUpdateInfo.ini
```

INI 格式：

```ini
[PackUpdate]
资源包名称=对应的 newmd5
```

执行流程：

1. 如果目标目录不存在，则创建。
2. 如果 `PackUpdateInfo.ini` 不存在，则创建并写入 `[PackUpdate]`。
3. 读取 INI 中以 `newname` 为键的记录。
4. 如果记录值已经等于 `newmd5`，跳过本次更新。
5. 根据 `shownotice` 决定是否询问用户。
6. 把 `downloadpath` 下载到启动器运行目录，临时文件名使用 `oldname`。
7. 使用 ZIP API 打开临时包。
8. 遍历压缩包内容，在目标目录中创建子目录并覆盖解压文件。
9. 删除临时 ZIP。
10. 把 `newname=newmd5` 写回 `PackUpdateInfo.ini`。
11. 显示“更新包解压完成”提示。

## 7. 文件下载如何接入启动器下载系统

`ModNut` 本身不直接保存普通更新文件，而是发出一个自定义“下载文件”事件：

```text
事件类型：下载文件
事件参数：Url|FileName|Folder
```

事件处理器执行以下工作：

1. 校验 URL 必须以 `http://` 或 `https://` 开头。
2. 切换到 UI 线程。
3. 调用启动器已有的自定义下载入口。
4. 创建目标文件夹并检查写入权限。
5. 创建下载任务和组合任务。
6. 启动任务并加入启动器下载任务栏。
7. 刷新下载按钮和下载状态 UI。
8. 下载完成、失败或取消时显示相应提示。

目标启动器没有“自定义事件”系统时，可以直接实现一个等价方法：

```text
QueueDownload(url, fileName, folder)
```

只要它能复用目标启动器现有下载器、显示任务状态，并把文件保存到指定目录即可。

下载地址中包含 `seafile` 时，当前实现会先发送一次 HTTP 请求，取得最终重定向后的响应 URL，再把最终 URL 交给下载器。

## 8. 目标启动器需要提供的能力

另一个 AI 在迁移时，需要先在目标项目中找到下列等价能力：

| 当前 PCL 能力 | 目标项目需要的替代能力 |
| --- | --- |
| `NetRequestByClientRetry` | 带重试的 UTF-8 HTTP GET |
| Newtonsoft `JObject/JArray` | 任意 JSON 反序列化库 |
| `MyMsgBox` | 支持自定义按钮文字并返回点击结果的弹窗 |
| `Hint` | 非阻塞提示或 Toast |
| `Setup.Get/Set` | 可持久化的启动器设置存储 |
| `OpenWebsite` | 系统浏览器打开 URL |
| `ClipboardSet` | 写入系统剪贴板 |
| `LoaderDownload` | 支持目标路径的后台下载器 |
| `LoaderCombo` | 串联“下载 → 安装”的任务系统，或等价异步流程 |
| `RunInUi` | UI 线程调度 |
| `Log` | 日志记录 |

## 9. 建议的模块划分

迁移到新启动器时，可以按以下职责拆分，也可以继续放在一个模块中：

```text
RemoteConfig
├── ServerDomain
├── LauncherVersion
└── BuildServerUrl(relativePath)

LauncherUpdateService
├── CheckLauncherUpdate
└── DownloadAndInstallLauncherUpdate

NoticeService
├── FetchNotices
├── ShouldShowNotice
└── ExecuteNoticeAction

ClientFileUpdateService
├── CheckFileUpdates
├── AddFile
├── ReplaceFile
├── DeleteFiles
├── UpdatePackage
└── CalculateMd5
```

无论如何拆分，都要保持服务端 JSON 字段和现有协议兼容。

## 10. 给执行迁移的 AI 的任务摘要

可以把下面这段作为具体任务要求：

> 请先阅读目标启动器的项目结构，找到程序启动入口、网络请求、JSON 解析、下载任务、设置存储、弹窗、日志、UI 线程调度以及退出重启相关实现。然后按照本文协议加入启动器自更新、自定义公告和客户端文件规则更新功能。服务端域名集中定义为可修改常量，接口路径保持不变。必须兼容 `pclinfo.json`、`notice17.json` 和 `ClientUpdateInfo2.json` 的现有字段，特别注意 `floderpath` 与 `clipbord` 是服务端正在使用的原始拼写。优先复用目标启动器已有基础设施，不要直接复制 PCL 专用类名。完成后构建项目，并说明修改文件、启动调用位置和服务端 JSON 的使用方式。

