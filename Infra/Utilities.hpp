#pragma once

#include <Infra/StringT.hpp>
#include <Infra/TypeList.hpp>

#include <type_traits>

namespace Infra {
namespace TL = TypeList;

/* Utility types and templates for the Infra Library */
// Each tool is declared below with documentation following the declaration. Implementations follow at bottom of file

template<typename Container, typename Extractor, typename=void>
struct ExtractTypeIfPresent;
// Use Extractor to extract a type from Container.
// If Extractor::Extract<Container> is a type, yields that type. Otherwise, yields typename Extractor::Default.
// The beauty of this is that the Extractor needs no SFINAE: Extract<T> can be ill-formed, and ExtractTypeIfPresent
// handles the switching.
// Also defines a contant static member, Found, which is true if the extracted type was present and false otherwise.

namespace Impl {
template<typename Str, char Delimiter, typename Current, typename Result>
struct SplitStringImpl;
}

template<typename Str, char Delimiter>
using SplitStringT = typename Impl::SplitStringImpl<Str, Delimiter, StringT<>, TL::List<>>::Type;
// Split a StringT into a TypeList of smaller StringTs on the provided delimiter character.

template<char... Str1, char... Str2>
constexpr bool operator==(StringT<Str1...>, StringT<Str2...>);
template<char... Str1, char... Str2>
constexpr bool operator!=(StringT<Str1...> s1, StringT<Str2...> s2) { return !(s1 == s2); }

// END OF DECLARATIONS -- IMPLEMENTATIONS FOLLOW

template<typename Container, typename Extractor, typename>
struct ExtractTypeIfPresent {
    using Type = typename Extractor::Default;
    constexpr static bool Found = false;
};
template<typename Container, typename Extractor>
struct ExtractTypeIfPresent<Container, Extractor, std::void_t<typename Extractor::template Extract<Container>>> {
    using Type = typename Extractor::template Extract<Container>;
    constexpr static bool Found = true;
};

namespace Impl {
template<typename Str, char Delimiter, typename Current, typename Result>
struct SplitStringImpl;
template<char First, char... Rest, char Delimiter, char Current1, char... CurrentN, typename Result>
struct SplitStringImpl<StringT<First, Rest...>, Delimiter, StringT<Current1, CurrentN...>, Result>
: public std::conditional_t<First == Delimiter,
                            SplitStringImpl<StringT<Rest...>, Delimiter, StringT<>,
                                            TL::append<Result, StringT<Current1, CurrentN...>>>,
                            SplitStringImpl<StringT<Rest...>, Delimiter, StringT<Current1, CurrentN..., First>,
                                            Result>> {};
template<char First, char... Rest, char Delimiter, typename Result>
struct SplitStringImpl<StringT<First, Rest...>, Delimiter, StringT<>, Result>
: public std::conditional_t<First == Delimiter,
                            SplitStringImpl<StringT<Rest...>, Delimiter, StringT<>, Result>,
                            SplitStringImpl<StringT<Rest...>, Delimiter, StringT<First>, Result>> {};
template<char Delimiter, typename Current, typename Result>
struct SplitStringImpl<StringT<>, Delimiter, Current, Result> {
    using Type = std::conditional_t<Current::size() != 0, TL::append<Result, Current>, Result>;
};
}

template<char... Str1, char... Str2>
constexpr bool operator==(StringT<Str1...>, StringT<Str2...>) {
    if constexpr (sizeof...(Str1) != sizeof...(Str2))
        return false;
    else
        return std::conjunction_v<std::bool_constant<Str1 == Str2>...>;
}

} // namespace Infra
