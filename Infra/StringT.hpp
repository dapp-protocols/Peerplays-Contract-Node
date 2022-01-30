/*~
 * Adapted from George Makrydakis's 'typestring' library:
 * https://github.com/irrequietus/typestring/
 * License is inclued in the "StringT-License.txt' file.
 *
 * Copyright (C) 2015, 2016 George Makrydakis <george@irrequietus.eu>
 *
 * This is a single header C++ library for creating types to use as type
 * parameters in template instantiations, repository available at
 * https://github.com/irrequietus/typestring.
 *
 * File subject to the terms and conditions of the Mozilla Public License v 2.0.
 * If a copy of the MPLv2 license text was not distributed with this file, you
 * can obtain it at: http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace Infra {

/*~
 * @desc A class 'storing' strings into distinct, reusable compile-time types that
 *       can be used as type parameters in a template parameter list.
 * @tprm C... : char non-type parameter pack whose ordered sequence results
 *              into a specific string.
 * @note Could have wrapped up everything in a single class, eventually will,
 *       once some compilers fix their class scope lookups! I have added some
 *       utility functions because asides being a fun little project, it is of
 *       use in certain constructs related to template metaprogramming
 *       nonetheless.
 */
template<char... C>
struct StringT final {
private:
    static constexpr char const   vals[sizeof...(C)+1] = { C...,'\0' };
    static constexpr unsigned int sval = sizeof...(C);
public:

    static constexpr char const * data() noexcept
    { return &vals[0]; }

    static constexpr unsigned int size() noexcept
    { return sval; };

    static constexpr char const * cbegin() noexcept
    { return &vals[0]; }

    static constexpr char const * cend() noexcept
    { return &vals[sval]; }
};

template<typename T>
constexpr static bool IsStringT = false;
template<char... C>
constexpr static bool IsStringT<StringT<C...>> = true;

template<char... C>
constexpr char const StringT<C...>::vals[sizeof...(C)+1];

//*~ part 1: preparing the ground, because function templates are awesome.

/*~
 * @note While it is easy to resort to constexpr strings for use in constexpr
 *       metaprogramming, what we want is to convert compile time string in situ
 *       definitions into reusable, distinct types, for use in advanced template
 *       metaprogramming techniques. We want such features because this kind of
 *       metaprogramming constitutes a pure, non-strict, untyped functional
 *       programming language with pattern matching where declarative semantics
 *       can really shine.
 *
 *       Currently, there is no feature in C++ that offers the opportunity to
 *       use strings as type parameter types themselves, despite there are
 *       several, different library implementations. This implementation is a
 *       fast, short, single-header, stupid-proof solution that works with any
 *       C++11 compliant compiler and up, with the resulting type being easily
 *       reusable throughout the code.
 *
 * @usge Just include the header and enable -std=c++11 or -std=c++14 etc, use
 *       like in the following example:
 *
 *            StrT("Hello!")
 *
 *       is essentially identical to the following template instantiation:
 *
 *            Infra::StringT<'H', 'e', 'l', 'l', 'o', '!'>
 *
 *       By passing -DUSE_STRINGT=<power of 2> during compilation, you can
 *       set the maximum length of the 'StringT' from 1 to 1024 (2^0 to 2^10).
 *       Although all preprocessor implementations tested are capable of far
 *       more with this method, exceeding this limit may cause internal compiler
 *       errors in most, with at times rather hilarious results.
 */

template<int N, int M>
constexpr char tygrab(char const(&c)[M]) noexcept
{ return c[N < M ? N : M-1]; }

//*~ part2: Function template type signatures for type deduction purposes. In
//          other words, exploiting the functorial nature of parameter packs
//          while mixing them with an obvious catamorphism through pattern
//          matching galore (partial ordering in this case in C++ "parlance").

template<char... X>
auto typoke(StringT<X...>) // as is...
    -> StringT<X...>;

template<char... X, char... Y>
auto typoke(StringT<X...>, StringT<'\0'>, StringT<Y>...)
    -> StringT<X...>;

// This overload added by Nathaniel Hourt to make StrT("") be StringT<> rather than StringT<'\x00'>
template<char... Y>
auto typoke(StringT<'\0'>, StringT<'\0'>, StringT<Y>...)
    -> StringT<>;

template<char A, char... X, char... Y>
auto typoke(StringT<X...>, StringT<A>, StringT<Y>...)
    -> decltype(typoke(StringT<X...,A>(), StringT<Y>()...));

template<char... C>
auto typeek(StringT<C...>)
    -> decltype(typoke(StringT<C>()...));

auto typeek(StringT<'\0'>)
    -> StringT<>;

template<char... A, char... B, typename... X>
auto tycat_(StringT<A...>, StringT<B...>, X... x)
    -> decltype(tycat_(StringT<A..., B...>(), x...));

template<char... X>
auto tycat_(StringT<X...>)
    -> StringT<X...>;

/*
 * Some people actually using this header as is asked me to include
 * a StringT "cat" utility given that it is easy enough to implement.
 * I have added this functionality through the template alias below. For
 * the obvious implementation, nothing more to say. All T... must be
 * of course, "StringTs".
 */
template<typename... T>
using tycat
    = decltype(tycat_(T()...));

} /* Infra */


//*~ part3: some necessary code generation using preprocessor metaprogramming!
//          There is functional nature in preprocessor metaprogramming as well.

/*~
 * @note Code generation block. Undoubtedly, the preprocessor implementations
 *       of both clang++ and g++ are relatively competent in producing a
 *       relatively adequate amount of boilerplate for implementing features
 *       that the language itself will probably be having as features in a few
 *       years. At times, like herein, the preprocessor is able to generate
 *       boilerplate *extremely* fast, but over a certain limit the compiler is
 *       incapable of compiling it. For the record, only certain versions of
 *       g++ where capable of going beyond 4K, so I thought of going from base
 *       16 to base 2 for USE_STRINGT power base. For the record, it takes
 *       a few milliseconds to generate boilerplate for several thousands worth
 *       of "string" length through such an 'fmap' like procedure.
 */

/* 2^0 = 1 */
#define STRINGT1(n,x) Infra::tygrab<0x##n##0>(x)

/* 2^1 = 2 */
#define STRINGT2(n,x) Infra::tygrab<0x##n##0>(x), Infra::tygrab<0x##n##1>(x)

/* 2^2 = 2 */
#define STRINGT4(n,x) \
        Infra::tygrab<0x##n##0>(x), Infra::tygrab<0x##n##1>(x) \
      , Infra::tygrab<0x##n##2>(x), Infra::tygrab<0x##n##3>(x)

/* 2^3 = 8 */
#define STRINGT8(n,x) \
        Infra::tygrab<0x##n##0>(x), Infra::tygrab<0x##n##1>(x) \
      , Infra::tygrab<0x##n##2>(x), Infra::tygrab<0x##n##3>(x) \
      , Infra::tygrab<0x##n##4>(x), Infra::tygrab<0x##n##5>(x) \
      , Infra::tygrab<0x##n##6>(x), Infra::tygrab<0x##n##7>(x)

/* 2^4 = 16 */
#define STRINGT16(n,x) \
        Infra::tygrab<0x##n##0>(x), Infra::tygrab<0x##n##1>(x) \
      , Infra::tygrab<0x##n##2>(x), Infra::tygrab<0x##n##3>(x) \
      , Infra::tygrab<0x##n##4>(x), Infra::tygrab<0x##n##5>(x) \
      , Infra::tygrab<0x##n##6>(x), Infra::tygrab<0x##n##7>(x) \
      , Infra::tygrab<0x##n##8>(x), Infra::tygrab<0x##n##9>(x) \
      , Infra::tygrab<0x##n##A>(x), Infra::tygrab<0x##n##B>(x) \
      , Infra::tygrab<0x##n##C>(x), Infra::tygrab<0x##n##D>(x) \
      , Infra::tygrab<0x##n##E>(x), Infra::tygrab<0x##n##F>(x)

/* 2^5 = 32 */
#define STRINGT32(n,x) \
        STRINGT16(n##0,x),STRINGT16(n##1,x)

/* 2^6 = 64 */
#define STRINGT64(n,x) \
        STRINGT16(n##0,x), STRINGT16(n##1,x), STRINGT16(n##2,x) \
      , STRINGT16(n##3,x)

/* 2^7 = 128 */
#define STRINGT128(n,x) \
        STRINGT16(n##0,x), STRINGT16(n##1,x), STRINGT16(n##2,x) \
      , STRINGT16(n##3,x), STRINGT16(n##4,x), STRINGT16(n##5,x) \
      , STRINGT16(n##6,x), STRINGT16(n##7,x)

/* 2^8 = 256 */
#define STRINGT256(n,x) \
        STRINGT16(n##0,x), STRINGT16(n##1,x), STRINGT16(n##2,x) \
      , STRINGT16(n##3,x), STRINGT16(n##4,x), STRINGT16(n##5,x) \
      , STRINGT16(n##6,x), STRINGT16(n##7,x), STRINGT16(n##8,x) \
      , STRINGT16(n##9,x), STRINGT16(n##A,x), STRINGT16(n##B,x) \
      , STRINGT16(n##C,x), STRINGT16(n##D,x), STRINGT16(n##E,x) \
      , STRINGT16(n##F,x)

/* 2^9 = 512 */
#define STRINGT512(n,x) \
        STRINGT256(n##0,x), STRINGT256(n##1,x)

/* 2^10 = 1024 */
#define STRINGT1024(n,x) \
        STRINGT256(n##0,x), STRINGT256(n##1,x), STRINGT256(n##2,x) \
      , STRINGT128(n##3,x), STRINGT16(n##38,x), STRINGT16(n##39,x) \
      , STRINGT16(n##3A,x), STRINGT16(n##3B,x), STRINGT16(n##3C,x) \
      , STRINGT16(n##3D,x), STRINGT16(n##3E,x), STRINGT16(n##3F,x)

//*~ part4 : Let's give some logic with a -DUSE_STRINGT flag!

#ifdef USE_STRINGT
#if USE_STRINGT == 0
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT1(,x)>()))
#elif USE_STRINGT == 1
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT2(,x)>()))
#elif USE_STRINGT == 2
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT4(,x)>()))
#elif USE_STRINGT == 3
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT8(,x)>()))
#elif USE_STRINGT == 4
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT16(,x)>()))
#elif USE_STRINGT == 5
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT32(,x)>()))
#elif USE_STRINGT == 6
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT64(,x)>()))
#elif USE_STRINGT == 7
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT128(,x)>()))
#elif USE_STRINGT == 8
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT256(,x)>()))
#elif USE_STRINGT == 9
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT512(,x)>()))
#elif USE_STRINGT == 10
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT1024(,x)>()))
#elif USE_STRINGT > 10

#warning !!!: custom StringT length exceeded allowed (1024)            !!!
#warning !!!: all StringTs to default maximum StringT length of 64     !!!
#warning !!!: you can use -DUSE_STRINGT=<power of two> to set length   !!!

#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT64(,x)>()))

#elif USE_STRINGT < 0

#warning !!!: You used USE_STRINGT with a negative size specified      !!!
#warning !!!: all StringTs to default maximum StringT length of 64     !!!
#warning !!!: you can use -DUSE_STRINGT=<power of two> to set length   !!!

#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT64(,x)>()))

#endif
#else
#define StrT(x) \
    decltype(Infra::typeek(Infra::StringT<STRINGT64(,x)>()))
#endif
