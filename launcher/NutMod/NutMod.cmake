# NutMod source registration. The parent launcher only needs to include this file
# and append the exported source/resource lists to its existing targets.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/NutMod")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/NutMod/NutModConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NutMod/NutModConfig.h"
    @ONLY
)

set(NUTMOD_LOGIC_SOURCES
    NutMod/NutModBootstrap.h
    NutMod/NutModBootstrap.cpp
    NutMod/NoticeDialog.h
    NutMod/NoticeDialog.cpp
    NutMod/NoticeManifest.h
    NutMod/NoticeManifest.cpp
    NutMod/NoticeService.h
    NutMod/NoticeService.cpp
    NutMod/NutModUi.h
    NutMod/NutModUi.cpp
)

set(NUTMOD_UPDATER_SOURCES
    NutMod/LauncherUpdateManifest.h
    NutMod/LauncherUpdateManifest.cpp
)

set(NUTMOD_RESOURCES
    NutMod/resources/nutmod_translations.qrc
)
