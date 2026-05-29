#pragma once

#ifndef VD_MACRO_HXX
#define VD_MACRO_HXX

#define VD_MEMBER(cls, field, checker) vd::member(#field, &cls::field, checker)
#define VD_FIELD(cls, field, chcker)   vd::field(#field, &cls::field, checker)

#endif
