# Fjord 魔改项目脚本说明

所有脚本都应从源码仓库中运行，不要再复制到 `install` 目录。脚本会根据自身位置自动定位仓库根目录，因此源码放在哪个磁盘都可以。

## 脚本分类

| 脚本 | 原名称 | 用途 | 输出 |
| --- | --- | --- | --- |
| `dev-env.ps1` | `dev-env.ps1` | 查找并载入 MSVC、Qt、vcpkg 和 JDK 17 开发环境 | 修改当前 PowerShell 会话环境 |
| `build-and-run-debug.ps1` | `test.ps1` | 增量编译 Debug 版并启动测试 | `build/Debug/fjordlauncher.exe` |
| `build-full-launcher.ps1` | `repack.ps1` | 构建可独立运行的完整 Release 启动器运行时 | `dist/FjordLauncher/` |
| `build-update-bundle.ps1` | 同名 | 构建启动器自动更新包，支持 Quick 和 Full | `dist/fjordlauncher-update-*/` 和 ZIP |
| `package-client-release.ps1` | `package-release.ps1` | 从一个已经可运行的客户端目录清理账号、日志、存档等用户数据并制作分发 ZIP | `dist/大雕GTNH客户端_Java25_yyyyMMdd.zip` |
| `package-client-release.bat` | `package-release.bat` | 双击运行完整客户端发布脚本；命令行参数会原样传给 PS1 | 同上 |

## 常用命令

在仓库根目录打开 PowerShell：

```powershell
# 编译并启动 Debug 版
.\scripts\build-and-run-debug.ps1

# 生成完整的启动器运行目录
.\scripts\build-full-launcher.ps1

# 日常只更新两个 EXE 的启动器更新包
.\scripts\build-update-bundle.ps1 -PackageMode Quick

# 首次部署或运行库发生变化时使用的完整更新包
.\scripts\build-update-bundle.ps1 -PackageMode Full

# 打包仓库默认 install 目录中的完整 GTNH 客户端
.\scripts\package-client-release.ps1

# 打包仓库外已经测试好的客户端
.\scripts\package-client-release.ps1 -SourceDirectory "G:\MC\fjord\install"

# 不分发 Minecraft 服务器列表
.\scripts\package-client-release.ps1 -SourceDirectory "G:\MC\fjord\install" -RemoveServerList
```

`package-client-release.ps1` 默认输出到仓库的 `dist` 目录。可以用 `-OutputDirectory` 指定其他位置。

## Quick 与 Full 更新包

- `Quick` 只包含 `fjordlauncher.exe`、`fjordlauncher_updater.exe` 和 `manifest.txt`，适用于普通代码与界面更新。
- `Full` 包含 Qt DLL、插件、JAR 和其他完整运行时，适用于首次部署、Qt/依赖更新，或修复旧版更新器导致的运行库缺失。
- 只有已经安装过修复版更新器的用户才能安全使用 Quick 包；旧版用户应先接收一次 Full 包。

## 每台电脑的本地路径

`dev-env.ps1` 会优先读取环境变量，然后自动寻找常见安装目录。自动发现失败时：

1. 将 `dev-env.local.example.ps1` 复制为 `dev-env.local.ps1`。
2. 修改其中四个路径。
3. 不要提交 `dev-env.local.ps1`，它已加入 `.gitignore`。

也可以在当前 PowerShell 会话中直接设置 `FJORD_VS_INSTALL_PATH`、`FJORD_QT_DIR`、`VCPKG_ROOT` 和 `JAVA_HOME`。

完整的新电脑部署步骤见 [`docs/Windows开发与打包环境.md`](../docs/Windows开发与打包环境.md)。
