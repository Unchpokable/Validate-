#pragma once

#ifndef VD_MACRO_HXX
#define VD_MACRO_HXX

#define VD_MEMBER(cls, field, checker) vd::member(#field, &cls::field, checker)
#define VD_FIELD(cls, field, checker)  vd::field(#field, &cls::field, checker)

#define VD_SMEMBER(cls, field, checker) vd::statics::member(&cls::field, checker)
#define VD_SFIELD(cls, field, checker)  vd::statics::field(&cls::field, checker)

#endif
