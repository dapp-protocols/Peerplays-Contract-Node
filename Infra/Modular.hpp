#pragma once

#include <Infra/Infra.hpp>
#include <Infra/Utilities.hpp>
#include <Infra/TypeList.hpp>

#include <type_traits>

namespace Infra {
namespace Modular {
namespace TL = TypeList;

/* Tools for describing modular program architecture. */
/* Each tool is declared below with documentation following each declaration. */

struct SubmoduleTag;
// A type used to tag Submodule demarcations within the DMarc.

template<typename T>
using ModuleType = std::remove_pointer_t<std::decay_t<T>>;
// Given a type returned by a demarcation, clean it into a proper module type (i.e., remove ref, cv, or pointer)

template<typename RootModule, template<typename Submodule, typename Path, typename State> class Accumulator>
struct WalkModuleTree;
// Perform a pre-order traversal of the module tree rooted at RootModule, passing submodules to Accumulator. Think of
// it as a Reduce algorithm on a tree, where the Accumulator is the reducer method.
//
// Accumulator is a template parameterized on the Submodule type, the submodule Path which is a typelist of
/// demarcations beginning with a demarcation on RootModule and ending with a demarcation on Submodule, and the State
/// which stores the result of accumulation so far. At the first instantiation of Accumulator, State will be void.
/// When instantiated, Accumulator should have a member type named Type which holds the State after the last
/// accumulation.
//
// When WalkModuleTree is instantiated, it has a member type Type which holds the final accumulator state. The state
// is initialized with AccumulatorEmptyState<Accumulator>

template<template<typename, typename, typename> class Accumulator>
struct AccumulatorEmptyState_S { using Type = TL::List<>; };
// Metafunction for an empty accumulator state for a given accumulator. Accumulators whose empty state is not an empty
// list should specialize this template.
template<template<typename, typename, typename> class Accumulator>
using AccumulatorEmptyState = typename AccumulatorEmptyState_S<Accumulator>::Type;
// Convenience alias that eliminates the need to prefix with `typename`

template<typename Submodule, typename Path, typename State = TL::List<>>
struct ModuleListAccumulator;
// An accumulator for WalkModuleTree which accumulates in its state a TypeList of modules in the tree, where each
// module is itself a two-item typelist containing the Submodule and the Path.

template<typename RootModule, typename... Getters>
auto FetchSubmodule(RootModule&& root, TL::List<Getters...> submodulePath);
// Given a root module and a path to a submodule, fetch the submodule


// IMPLEMENTATIONS FOLLOW
template<typename RootModule, template<typename Submodule, typename Path, typename State> class Accumulator>
struct WalkModuleTree {
    template<typename Module, typename Path = TL::List<>, typename State = AccumulatorEmptyState<Accumulator>>
    struct WalkImpl;
    // Implementation of the walk submodule extraction and recursion

    using Type = typename WalkImpl<RootModule>::Type;

    template<typename Module, typename Path, typename OldState>
    struct WalkImpl {
        using StateAfterModule = typename Accumulator<Module, Path, OldState>::Type;

        template<typename SubmoduleDemarcation, typename Reduction>
        struct Reducer {
            using Submodule = ModuleType<typename SubmoduleDemarcation::ReturnType>;
            using NewPath = TL::append<Path, SubmoduleDemarcation>;
            using type = typename WalkImpl<Submodule, NewPath, Reduction>::Type;
        };

        using Type = TL::reduce<TL::Map::Lookup<typename DMarcFor<Module>::Type, SubmoduleTag, TL::List<>>,
                                Reducer, StateAfterModule>;
    };
};

template<typename Submodule, typename Path, typename State>
struct ModuleListAccumulator {
    static_assert(TL::IsTypeList<State>, "State type must be a TypeList!");
    using Type = TL::append<State, TL::List<Submodule, Path>>;
};

template<typename RootModule, typename... Getters>
auto FetchSubmodule(RootModule&& root, TL::List<Getters...> submodulePath) {
    using Path = decltype(submodulePath);
    static_assert(TL::length<Path>() != 0, "Cannot get submodule: path is empty");

    auto&& trunk = TL::first<Path>::Invoke(root);

    if constexpr (TL::length<Path>() == 1)
        return trunk;
    else
        return FetchSubmodule(trunk, TL::slice<Path, 1>());
}

} } // namespace Infra::Modular
