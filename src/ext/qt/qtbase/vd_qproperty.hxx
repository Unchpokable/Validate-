#pragma once

#ifndef VD_QTBASE_PROPERTY_HXX
#define VD_QTBASE_PROPERTY_HXX

#include <format>

#include <QObject>
#include <QString>
#include <QVariant>

#include "models/vd_rule.hxx"

#include "core/vd_always_false.hxx"
#include "core/vd_result.hxx"

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
    return rule<T>([prop_name = prop_name.toUtf8(), checker](const T& obj) -> vd::result {
        auto qobj = qobject_cast<const QObject*>(&obj);
        if(!qobj) {
            return vd::result::failed({ "Object is not a QObject" });
        }

        QVariant value = qobj->property(prop_name.data());
        if(!value.isValid()) {
            return vd::result::failed({ "Property is not valid" });
        }

        if(!value.canConvert<PropT>()) {
            return vd::result::failed({ "Property cannot be converted to expected type" });
        }

        auto converted_value = value.value<PropT>();

        auto checker_result = checker(converted_value);
        if constexpr(std::same_as<decltype(checker_result), vd::result>) {
            return checker_result;
        }
        else if constexpr(std::convertible_to<decltype(checker_result), bool>) {
            return checker_result ? vd::result::ok()
                                  : vd::result::failed({ std::format("QProperty {} validation failed", prop_name.toStdString()) });
        }
        else {
            static_assert(vd::always_false_v<Checker>, "Checker must return bool or vd::result");
        }
    });
}
} // namespace vd::qt
#endif
