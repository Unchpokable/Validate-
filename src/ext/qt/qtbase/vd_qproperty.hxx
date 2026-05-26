#pragma once

#ifndef VD_QTBASE_PROPERTY_HXX
#define VD_QTBASE_PROPERTY_HXX

#include <QObject>
#include <QString>
#include <QVariant>

#include "models/vd_rule.hxx"

namespace vd::qt
{
/// @brief Creates a vd::rule<T> that checks a QObject's property against a given checker.
/// @tparam T QObject-derived class being validated.
/// @tparam Checker Callable that accepts the property's value type and returns bool-convertible.
///         The property value type is deduced from the checker's first argument.
///         For generic lambdas or std::function, the deduction will fail — construct rule<T> directly.
/// @param prop_name Name of the Q_PROPERTY to read. Returns false if the property does not exist.
template<typename T, typename Checker>
auto qt_property(const QString& prop_name, Checker checker) -> rule<T>
{
    using PropT = typename detail::first_arg_of<Checker>::type;
    return rule<T>([prop_name = prop_name.toUtf8(), checker](const T& obj) -> bool {
        auto qobj = qobject_cast<const QObject*>(&obj);
        if(!qobj) {
            return false;
        }

        QVariant value = qobj->property(prop_name.data());
        if(!value.isValid()) {
            return false;
        }

        if(!value.canConvert<PropT>()) {
            return false;
        }

        auto converted_value = value.value<PropT>();

        return static_cast<bool>(checker(converted_value));
    });
}
} // namespace vd::qt
#endif
