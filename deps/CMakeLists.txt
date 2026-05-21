# ── Qt6 path configuration ─────────────────────────────────────────────────────
# Defined unconditionally so it is always visible in cmake-gui / ccmake.
# Priority: VD_QT_DIR (explicit) > QTDIR (environment) > system search paths.
set(VD_QT_DIR "" CACHE PATH
    "Path to Qt6 installation directory. \
Falls back to the QTDIR environment variable when not set."
)

# ── Qt extension options ───────────────────────────────────────────────────────
option(VD_EXTENSION_QT_BASE    "Enable Qt Base (Core) extension"    OFF)
option(VD_EXTENSION_QT_WIDGETS "Enable Qt Widgets extension"        OFF)
option(VD_EXTENSION_QT_QML     "Enable Qt QML / Quick extension"    OFF)

# ── Qt6 discovery ──────────────────────────────────────────────────────────────
set(_vd_need_qt OFF)
if(VD_EXTENSION_QT_BASE OR VD_EXTENSION_QT_WIDGETS OR VD_EXTENSION_QT_QML)
    set(_vd_need_qt ON)
endif()

set(VALIDATE_QT_COMPONENTS_REQUIRED "" PARENT_SCOPE)

if(_vd_need_qt)
    if(VD_QT_DIR)
        list(PREPEND CMAKE_PREFIX_PATH "${VD_QT_DIR}")
    elseif(DEFINED ENV{QTDIR})
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
    endif()

    # Collect required Qt components
    set(_vd_qt_components Core)
    if(VD_EXTENSION_QT_WIDGETS)
        list(APPEND _vd_qt_components Widgets)
    endif()
    if(VD_EXTENSION_QT_QML)
        list(APPEND _vd_qt_components Qml Quick)
    endif()

    find_package(Qt6 REQUIRED COMPONENTS ${_vd_qt_components})

    # Propagate required components to the parent scope for package config generation
    set(VALIDATE_QT_COMPONENTS_REQUIRED "${_vd_qt_components}" PARENT_SCOPE)

    # ── Qt Base extension target ───────────────────────────────────────────────
    if(VD_EXTENSION_QT_BASE)
        add_library(vd_qt_base INTERFACE)
        add_library(Validate::vd_qt_base ALIAS vd_qt_base)
        target_link_libraries(vd_qt_base INTERFACE Validate::vd Qt6::Core)
        target_compile_definitions(vd_qt_base INTERFACE VD_ENABLE_EXTENSION_QT_BASE)
    endif()

    # ── Qt Widgets extension target ────────────────────────────────────────────
    if(VD_EXTENSION_QT_WIDGETS)
        add_library(vd_qt_widgets INTERFACE)
        add_library(Validate::vd_qt_widgets ALIAS vd_qt_widgets)
        # vd_qt_widgets pulls in qt_base automatically
        if(TARGET vd_qt_base)
            target_link_libraries(vd_qt_widgets INTERFACE Validate::vd_qt_base Qt6::Widgets)
        else()
            target_link_libraries(vd_qt_widgets INTERFACE Validate::vd Qt6::Core Qt6::Widgets)
            target_compile_definitions(vd_qt_widgets INTERFACE VD_ENABLE_EXTENSION_QT_BASE)
        endif()
        target_compile_definitions(vd_qt_widgets INTERFACE VD_ENABLE_EXTENSION_QT_WIDGETS)
    endif()

    # ── Qt QML extension target ────────────────────────────────────────────────
    if(VD_EXTENSION_QT_QML)
        add_library(vd_qt_qml INTERFACE)
        add_library(Validate::vd_qt_qml ALIAS vd_qt_qml)
        if(TARGET vd_qt_base)
            target_link_libraries(vd_qt_qml INTERFACE Validate::vd_qt_base Qt6::Qml Qt6::Quick)
        else()
            target_link_libraries(vd_qt_qml INTERFACE Validate::vd Qt6::Core Qt6::Qml Qt6::Quick)
            target_compile_definitions(vd_qt_qml INTERFACE VD_ENABLE_EXTENSION_QT_BASE)
        endif()
        target_compile_definitions(vd_qt_qml INTERFACE VD_ENABLE_EXTENSION_QT_QML)
    endif()
endif()
