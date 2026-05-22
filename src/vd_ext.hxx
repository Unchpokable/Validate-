#pragma once

#ifndef VD_EXT_HXX
#define VD_EXT_HXX

#ifdef VD_ENABLE_EXTENSION_QT_BASE
#include "ext/qt/vd_qtbase.hxx"
#endif

#ifdef VD_ENABLE_EXTENSION_QT_WIDGETS
#include "ext/qt/vd_qtwidgets.hxx"
#endif

#ifdef VD_ENABLE_EXTENSION_QT_QML
#include "ext/qt/vd_qml.hxx"
#endif

#endif // VD_EXT_HXX
