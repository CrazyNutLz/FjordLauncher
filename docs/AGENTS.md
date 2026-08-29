# Project Instructions

This project is a custom GTNH server launcher based on Fjord Launcher.

## Project goals

- Windows x64 only.
- GTNH Java 17-25 compatibility must be preserved.
- Java 25 will be used to launch GTNH.
- The launcher will eventually use Nide8 authentication.
- The launcher will have a custom client auto-update system.
- This is a permanently forked version of Fjord Launcher.
- We do not need to maintain compatibility with future Fjord or Prism updates.

## Important constraints

- Do not break Prism/Fjord `mmc-pack.json` and patches version resolution.
- Do not change GTNH launch semantics unless explicitly requested.
- Preserve Java detection and launch infrastructure.
- Prefer existing Fjord networking/task infrastructure instead of introducing unnecessary new frameworks.
- Build target is Windows x64 MSVC.
- Avoid unnecessary architectural refactoring.
- Do not rewrite working Fjord/Prism core systems unless explicitly requested.
- Only modify code that is necessary for the requested task.
- After modifying C++ or Qt code, compile the project and fix compilation errors introduced by the modification before considering the task complete.

## Modification standards

### General rule

Use the smallest possible code change required to implement the requested behavior.

Prefer:

- Localized changes over cross-project refactoring.
- Small patches over large rewrites.
- Existing Fjord abstractions over introducing new abstractions.
- Existing classes and functions over replacing them.
- Preserving existing code whenever it does not interfere with the requested behavior.
- Disabling unwanted behavior instead of physically deleting its implementation.

Do not perform unrelated:

- Refactoring.
- Cleanup.
- Modernization.
- Formatting.
- Renaming.
- File movement.
- API redesign.

unless explicitly requested.

### Minimal modification policy

When disabling or removing an unwanted feature, always prefer the smallest possible modification.

If an existing function or method should no longer perform its original behavior, prefer an early return when it is safe.

For example:

```cpp
void SomeFeature::run()
{
    return;
}
```

For boolean availability checks, prefer:

```cpp
bool SomeFeature::isAvailable() const
{
    return false;
}
```

For functions returning objects or containers, prefer returning an appropriate safe empty value when possible instead of deleting the implementation.

The preferred order is:

1. Return early from the relevant method.
2. Return a safe neutral value.
3. Disable the action.
4. Hide the UI element.
5. Skip feature initialization.
6. Skip feature registration.
7. Add a small conditional branch.
8. Only physically delete code when necessary.

### Code deletion policy

Physical deletion of existing Fjord/Prism code should be treated as a last resort.

Do not delete large sections of source code merely because the customized launcher does not expose or currently use that functionality.

Before deleting existing code, first determine whether the same result can be achieved with:

- `return;`
- `return false;`
- An empty result.
- A disabled action.
- A hidden widget.
- Skipping initialization.
- Skipping registration.
- A small conditional check.
- Making the functionality unreachable from the customized UI.

Physical code deletion is appropriate only when:

- Keeping the code causes a real technical problem.
- Keeping the code causes an unavoidable runtime problem.
- Keeping the code creates an unwanted mandatory dependency.
- The code prevents the requested feature from being implemented cleanly.
- The user explicitly requests physical removal.
- A smaller disabling modification is not technically safe.

### UI feature removal

If a feature should no longer be accessible to users, prefer modifying the UI rather than deleting its backend implementation.

Prefer:

- `hide()`
- `setVisible(false)`
- `setEnabled(false)`
- Removing an action from a visible menu.
- Preventing navigation to a page.
- Returning early from the corresponding event handler or slot.

For example:

```cpp
ui->someButton->hide();
```

or:

```cpp
void MainWindow::onSomeActionTriggered()
{
    return;
}
```

Do not immediately delete:

- The complete backend implementation.
- Related task classes.
- Shared models.
- Network code.
- Shared utility classes.
- Supporting APIs.

unless their physical removal is specifically necessary.

### Preserve existing internal structure

Avoid unnecessary changes to the existing Fjord/Prism internal structure.

Do not unnecessarily:

- Rename existing classes.
- Rename internal namespaces.
- Rename internal APIs.
- Rename internal functions.
- Change function signatures.
- Move large groups of files.
- Change directory structures.
- Rewrite inheritance structures.
- Replace existing task systems.
- Replace existing network systems.
- Replace existing version/component systems.
- Remove compatibility code simply because it appears unused by the customized UI.

User-visible branding may be changed freely.

Internal Fjord/Prism implementation names should normally remain unchanged unless changing them is technically necessary.

### Scope control

Only modify files directly relevant to the requested task.

Before modifying shared or low-level code, check whether the requested behavior can instead be implemented in:

- UI code.
- A higher-level controller.
- Existing configuration.
- An isolated launcher-specific module.
- A small conditional branch.
- An existing extension point.

Prefer higher-level modifications over changes to shared infrastructure.

Do not modify unrelated files while implementing a feature.

### Feature disabling example

If the customized launcher does not need a generic Fjord feature such as:

- Generic instance creation.
- Modrinth integration.
- CurseForge integration.
- FTB integration.
- Technic integration.
- Generic account options.
- Generic launcher actions.

first disable or hide the user-facing entry point.

For example:

```cpp
void MainWindow::on_actionCreateInstance_triggered()
{
    return;
}
```

or:

```cpp
ui->actionCreateInstance->setVisible(false);
```

Do not immediately delete the underlying implementation unless there is a concrete technical reason to do so.

The objective is to make unwanted functionality unavailable with the minimum possible impact on the existing codebase.

## Core systems protection

The following systems are considered sensitive core infrastructure.

Changes to these systems must be minimal and should only be made when the requested feature specifically requires them:

- Minecraft version resolution.
- Prism/Fjord component resolution.
- `mmc-pack.json` handling.
- Patch JSON handling.
- Library resolution.
- Classpath construction.
- Java detection.
- Java runtime selection.
- Minecraft launch argument construction.
- Minecraft launch task infrastructure.
- RetroFuturaBootstrap compatibility.
- LWJGL3ify compatibility.
- GTNH Java 17-25 compatibility.

Do not refactor these systems for cosmetic reasons or general cleanup.

If a requested feature can be implemented without changing these systems, leave them unchanged.

## Authentication changes

The launcher will eventually use Nide8 authentication.

When implementing authentication changes:

- Prefer extending or adapting the existing Fjord authentication infrastructure.
- Avoid replacing unrelated account infrastructure.
- Preserve account/session structures that are still useful.
- Make the minimum changes required for Nide8 integration.
- Do not modify Minecraft launch internals unless Nide8 technically requires additional launch arguments or Java agents.

## Client updater changes

The launcher will eventually include a custom GTNH client auto-update system.

When implementing the updater:

- Prefer existing Fjord networking infrastructure.
- Prefer existing task/job abstractions.
- Keep client updating separate from Minecraft version/component resolution.
- Do not modify `mmc-pack.json` or patches handling merely to implement file updates.
- Use SHA-256 or another explicitly approved strong integrity check for managed files.
- Keep server-controlled configuration outside compiled C++ code when practical.
- Prefer external manifests for frequently changing data.

Examples of data that should preferably remain external:

- Client version.
- File lists.
- File hashes.
- Download URLs.
- Server addresses.
- Update channels.
- Announcements.

## Build and validation standards

After meaningful C++ or Qt source changes, build the project using:

```powershell
cmake --build --preset windows_msvc --config Debug
```

Requirements:

- Fix compilation errors introduced by the current modification.
- Fix linker errors introduced by the current modification.
- Do not perform unrelated warning cleanup.
- Pre-existing warnings do not need to be fixed unless they directly affect the requested task.
- Prefer incremental builds.
- Do not perform a clean rebuild unless technically necessary.
- Documentation-only changes do not require a build.
- External configuration-only changes do not require recompiling the launcher unless those files are compiled into Qt resources.

## Development environment

### Build command

```powershell
cmake --build --preset windows_msvc --config Debug
```

### Qt

```text
F:\QT\6.11.2\msvc2022_64
```

### vcpkg

```text
D:\PCL\vcpkg
```

### Platform

```text
Windows x64
MSVC 2022
```

## Final modification principle

When multiple implementations can achieve the requested result, choose the implementation that:

1. Changes the fewest existing lines.
2. Touches the fewest existing files.
3. Preserves the most existing Fjord/Prism code.
4. Avoids unnecessary deletion.
5. Avoids unnecessary refactoring.
6. Does not disturb GTNH launch compatibility.
7. Is easy to understand and revert.

For unwanted existing functionality:

**Disable first, hide second, bypass third, delete only when necessary.**