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

    set(_vd_src_root "${CMAKE_CURRENT_SOURCE_DIR}/..")

    # ── Qt Base extension ──────────────────────────────────────────────────────
    # Compiled sources and link requirements are folded directly into vd.
    if(VD_EXTENSION_QT_BASE)
        target_sources(vd PRIVATE
            "${_vd_src_root}/src/ext/qt/qtbase/vd_qstring.cxx"
        )
        target_link_libraries(vd PUBLIC Qt6::Core)
        target_compile_definitions(vd PUBLIC VD_ENABLE_EXTENSION_QT_BASE)
    endif()

    # ── Qt Widgets extension ───────────────────────────────────────────────────
    if(VD_EXTENSION_QT_WIDGETS)
        target_link_libraries(vd PUBLIC Qt6::Widgets)
        target_compile_definitions(vd PUBLIC VD_ENABLE_EXTENSION_QT_WIDGETS)
        # Qt Base is implicitly required by Widgets; make sure its define is set too.
        if(NOT VD_EXTENSION_QT_BASE)
            target_compile_definitions(vd PUBLIC VD_ENABLE_EXTENSION_QT_BASE)
        endif()
    endif()

    # ── Qt QML extension ──────────────────────────────────────────────────────
    if(VD_EXTENSION_QT_QML)
        target_link_libraries(vd PUBLIC Qt6::Qml Qt6::Quick)
        target_compile_definitions(vd PUBLIC VD_ENABLE_EXTENSION_QT_QML)
        if(NOT VD_EXTENSION_QT_BASE)
            target_compile_definitions(vd PUBLIC VD_ENABLE_EXTENSION_QT_BASE)
        endif()
    endif()
endif()
