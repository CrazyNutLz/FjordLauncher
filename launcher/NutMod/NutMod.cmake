# NutMod source registration. The parent launcher only needs to include this file
# and append the exported source/resource lists to its existing targets.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/NutMod/config")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/NutMod/config/NutModConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NutMod/config/NutModConfig.h"
    @ONLY
)

set(NUTMOD_LOGIC_SOURCES
    NutMod/client_update/ClientUpdateManifest.h
    NutMod/client_update/ClientUpdateManifest.cpp
    NutMod/client_update/ClientUpdateEngine.h
    NutMod/client_update/ClientUpdateEngine.cpp
    NutMod/client_update/ClientUpdateTask.h
    NutMod/client_update/ClientUpdateTask.cpp
    NutMod/client_update/ClientUpdateService.h
    NutMod/client_update/ClientUpdateService.cpp
    NutMod/bootstrap/NutModBootstrap.h
    NutMod/bootstrap/NutModBootstrap.cpp
    NutMod/notices/NoticeDialog.h
    NutMod/notices/NoticeDialog.cpp
    NutMod/notices/NoticeManifest.h
    NutMod/notices/NoticeManifest.cpp
    NutMod/notices/NoticeService.h
    NutMod/notices/NoticeService.cpp
    NutMod/ui/NutModUi.h
    NutMod/ui/NutModUi.cpp
)

set(NUTMOD_UPDATER_SOURCES
    NutMod/launcher_update/LauncherUpdateManifest.h
    NutMod/launcher_update/LauncherUpdateManifest.cpp
)

set(NUTMOD_RESOURCES
    NutMod/resources/nutmod_translations.qrc
)
