# Windows 开发与打包环境

本文面向本项目的维护者和在新电脑上接手任务的 Codex。目标环境固定为 Windows x64、Visual Studio 2022 MSVC 和 Qt 6，不使用 MinGW。

## 1. 工具与版本

| 组件 | 要求 | 用途 |
| --- | --- | --- |
| Windows | Windows 10/11 x64 | 项目目标平台 |
| Git | 当前稳定版 | 获取源码及子模块 |
| Visual Studio 2022 / Build Tools 2022 | “使用 C++ 的桌面开发”、MSVC x64/x86、Windows SDK | C/C++ 编译器与链接器 |
| CMake | 3.28 或更高 | `CMakePresets.json` 的最低要求为 3.28 |
| Ninja | 当前稳定版 | 项目使用 `Ninja Multi-Config` |
| Qt | 6.11.2，`msvc2022_64` | 当前验证版本 |
| Qt 模块 | Qt Base、Qt Image Formats、Qt Network Authorization | 启动器编译和运行所需 |
| JDK | JDK 17 x64 | 编译启动器内的 Java 辅助组件 |
| vcpkg | x64-windows | 自动安装 `libarchive`、`cmark`、`tomlplusplus` 等 C++ 依赖 |
| 7-Zip | 可选 | 完整客户端 ZIP；没有时脚本会回退到 Windows tar 或 `Compress-Archive` |

注意：JDK 17 是启动器的**编译环境**。GTNH 客户端实际启动时使用的 Java 25 由启动器或玩家配置，两者不要混为一谈。

官方参考：

- [Microsoft：安装并使用 MSVC 命令行工具](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170)
- [Qt：Windows 支持的编译器与工具](https://doc.qt.io/qt-6/windows-building.html)
- [Qt：让 CMake 找到 Qt](https://doc.qt.io/qt-6/cmake-making-qt-available.html)
- [Microsoft：安装和引导 vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-msbuild)

## 2. 安装基础工具

可以使用 Windows 程序包管理器安装常用工具：

```powershell
winget install --exact --id Git.Git
winget install --exact --id Kitware.CMake
winget install --exact --id Ninja-build.Ninja
winget install --exact --id EclipseAdoptium.Temurin.17.JDK
winget install --exact --id 7zip.7zip
```

安装 Visual Studio 2022 Community 或 Build Tools 2022，在安装器中选择：

- 使用 C++ 的桌面开发；
- MSVC v143 x64/x86 生成工具；
- Windows 10 或 Windows 11 SDK；
- CMake tools for Windows（可选，但建议安装）。

Visual Studio、Qt Online Installer 可能需要图形界面确认许可或登录。如果 Codex 无法代替用户完成该步骤，应让用户完成安装器交互，再继续自动配置和验证。

## 3. 安装 Qt 6.11.2

使用 Qt Online Installer 安装到例如 `C:\Qt`，选择：

```text
Qt 6.11.2
  MSVC 2022 64-bit
  Qt Image Formats
  Qt Network Authorization
```

最终应存在：

```text
C:\Qt\6.11.2\msvc2022_64\bin\Qt6Core.dll
C:\Qt\6.11.2\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake
```

项目的 `dev-env.ps1` 会自动搜索常见的 `C:\Qt`、`D:\Qt`、`F:\Qt`，也可以通过 `FJORD_QT_DIR` 指定准确路径。

## 4. 安装 vcpkg

推荐把 vcpkg 放在源码仓库旁边：

```powershell
New-Item -ItemType Directory -Path C:\Dev -Force
git clone https://github.com/microsoft/vcpkg.git C:\Dev\vcpkg
& C:\Dev\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

本项目 `vcpkg.json` 没有固定 baseline。若未来最新版 vcpkg 出现兼容问题，当前已验证过的提交为：

```text
df25fb4f73c1c3bf7d019fc50742fc90902f8c60
```

可以执行：

```powershell
git -C C:\Dev\vcpkg checkout df25fb4f73c1c3bf7d019fc50742fc90902f8c60
& C:\Dev\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

无需执行全局 `vcpkg integrate install`；CMake 预设会通过 `VCPKG_ROOT` 使用工具链文件。

## 5. 获取项目

```powershell
Set-Location C:\Dev
git clone --recurse-submodules https://github.com/CrazyNutLz/FjordLauncher.git
Set-Location .\FjordLauncher
git submodule update --init --recursive
```

必须包含 `libraries/libnbtplusplus` 子模块，否则配置或编译可能失败。

## 6. 配置本机路径

先尝试自动检测：

```powershell
. .\scripts\dev-env.ps1
```

成功后会打印仓库、Visual Studio、Qt、vcpkg 和 JDK 17 的实际路径。如果失败，创建仅本机使用的配置：

```powershell
Copy-Item .\scripts\dev-env.local.example.ps1 .\scripts\dev-env.local.ps1
notepad .\scripts\dev-env.local.ps1
```

`dev-env.local.ps1` 已被 Git 忽略，不应提交。也可以在调用时显式传参：

```powershell
. .\scripts\dev-env.ps1 `
    -VisualStudioPath "C:\Program Files\Microsoft Visual Studio\2022\BuildTools" `
    -QtDirectory "C:\Qt\6.11.2\msvc2022_64" `
    -VcpkgDirectory "C:\Dev\vcpkg" `
    -JavaDirectory "C:\Program Files\Eclipse Adoptium\jdk-17"
```

## 7. 首次配置与验证

```powershell
. .\scripts\dev-env.ps1

cmake --preset windows_msvc `
    -DLauncher_BUILD_ARTIFACT=windows-x64 `
    -DLauncher_BUILD_PLATFORM=windows-x64

cmake --build --preset windows_msvc --config Debug --target FjordLauncher
```

成功后启动：

```powershell
.\build\Debug\fjordlauncher.exe
```

第一次配置时 vcpkg 会下载并编译依赖，耗时较长属于正常现象。不要因为暂时没有输出就删除 `build` 或中断进程。

## 8. 日常开发与发布

脚本用途和命令统一记录在 [`scripts/README.md`](../scripts/README.md)。所有生成物进入 `build`、`install` 或 `dist`，这些目录均不应提交。

常用流程：

```powershell
# 修改代码后编译并运行 Debug
.\scripts\build-and-run-debug.ps1

# 构建完整启动器运行目录
.\scripts\build-full-launcher.ps1

# 构建自动更新包
.\scripts\build-update-bundle.ps1 -PackageMode Quick

# 从已经验证可运行的客户端目录制作玩家分发包
.\scripts\package-client-release.ps1 -SourceDirectory "G:\MC\fjord\install"
```

修改启动器版本号、更新地址、公告地址等内容时，先阅读：

- `docs/AGENTS.md`
- `launcher/NutMod/PORTING.md`
- `launcher/NutMod/NutModConfig.cmake`

## 9. 交给新电脑上的 Codex

建议直接提出以下任务：

> 请先完整阅读 `docs/AGENTS.md`、`docs/Windows开发与打包环境.md` 和 `scripts/README.md`。检查本机是否安装 Windows x64 MSVC 2022、CMake 3.28+、Ninja、Qt 6.11.2 msvc2022_64（含 qtimageformats、qtnetworkauth）、JDK 17 和 vcpkg。缺少的组件按官方方式安装；需要管理员权限、许可确认或账号登录时暂停并告诉我。随后创建 `scripts/dev-env.local.ps1`，运行环境脚本、CMake 配置和 Debug 编译，修复仅由本机环境差异引起的问题。不要修改 Minecraft/GTNH 启动核心逻辑。

Codex 完成部署后至少应报告：

- 每个依赖的版本和实际路径；
- `dev-env.ps1` 是否成功；
- CMake 配置是否成功；
- Debug 编译是否成功；
- 是否存在仅本机使用、且已被 Git 忽略的配置文件。
