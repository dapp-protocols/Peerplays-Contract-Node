#pragma once

#include <Infra/Utilities.hpp>
#include <Infra/TypeList.hpp>
#include <Infra/StringT.hpp>

namespace Infra {

/* Fundamenals of Infra */
// Each tool is declared below with documentation following the declaration. Implementations follow at bottom of file

template<typename T>
struct DMarcFor;
// Template to get the DMarc for a given type. By default, this returns T::DMarc if it is defined, otherwise List<>.
// Types which are intended to be handled by Infra but do not contain a DMarc member type can specialize this template
// to define the DMarc that describes them.
// Template also contains a static constant member, Defined, which is true if a DMarc was defined; false otherwise.

template<typename Method, Method method>
struct MethodDemarcation;
// Type that describes a method of a class. Defines member types Module, ReturnType, ArgumentTypes, and Signature.
// Module is Class, ReturnType is Return, ArgumentTypes is List<Args...>, and Signature is a PTMF type for the method.

#define DEMARCATE(...) Infra::MethodDemarcation<decltype(&__VA_ARGS__), &__VA_ARGS__>
// Convenience macro to demarcate a method of a module. This saves having to type the fully qualified name twice. It
// is a variadic macro so that types with commas in their names (think template instantiations) still work!

template<typename T, typename Tag>
using DMarcTag = TypeList::Map::Lookup<typename DMarcFor<T>::Type, Tag, TypeList::List<>>;
// Template to read a given type's DMarc for a given Tag. If the type has a DMarc and the DMarc contains the provided
// tag, this is defined as the tagged type from the DMarc. Otherwise, the Type member type is defined as List<>.

/* END OF DECLARATIONS -- DEFINITIONS FOLLOW */

template<typename T>
struct DMarcFor {
    struct ExtractDMarc {
        using Default = TypeList::List<>;
        template<typename T1>
        using Extract = typename T1::DMarc;
    };

    using Type = typename ExtractTypeIfPresent<T, ExtractDMarc>::Type;
    constexpr static bool Defined = ExtractTypeIfPresent<T, ExtractDMarc>::Found;
};

template<typename Class, typename Return, typename... Args, Return (Class::*method)(Args...)>
struct MethodDemarcation<Return (Class::*)(Args...), method> {
    using Module = Class;
    using ReturnType = Return;
    using ArgumentTypes = TypeList::List<Args...>;
    using Signature = Return (Class::*)(Args...);

    constexpr static Signature Method = method;
    static ReturnType Invoke(Module& m, Args&&... args) { return (m.*Method)(std::forward<Args>(args)...); }
};
template<typename Class, typename Return, typename... Args, Return (Class::*method)(Args...) const>
struct MethodDemarcation<Return (Class::*)(Args...) const, method> {
    using Module = Class;
    using ReturnType = Return;
    using ArgumentTypes = TypeList::List<Args...>;
    using Signature = Return (Class::*)(Args...);

    constexpr static Signature Method = method;
    static ReturnType Invoke(Module& m, Args&&... args) { return (m.*Method)(std::forward<Args>(args)...); }
};

} // namespace Infra
