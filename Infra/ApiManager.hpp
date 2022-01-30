#pragma once

#include <Infra/Infra.hpp>
#include <Infra/Modular.hpp>
#include <Infra/TypeList.hpp>
#include <Infra/StringT.hpp>
#include <Infra/StaticVariant.hpp>

#include <boost/preprocessor/seq/for_each.hpp>

#include <memory>

/* The API Architecture */
// Classes can expose APIs that are then published for access over the network, IPC, etc.
//
// Classes expose APIs by having a method to fetch an instance of an API (called "allocating" it) for invocation. The
// class can advertise these APIs via Infra allowing them to be discovered and allocated through Infra. An advertised
// API may have zero or more categorizations, which are category/subcategory paths used to organize APIs. An API also
// has a name. API categories and names are compile-time strings.
//
// The API manager uses Infra to discover the APIs and API categories advertised by modules in the program and host
// them for access via some RPC channel.

namespace Infra {
namespace Api {

namespace TL = Infra::TypeList;
namespace Mdlr = Infra::Modular;

/* DECLARATIONS */
/* Documentation for each declaration appears below the declaration */

struct ApiTag;
// A type used to tag API demarcations within the DMarc.

template<typename ApiCategorization, typename ApiName, typename MethodDemarcation>
struct ApiDemarcation;
// A demarcation of an API, consisting of the method demarcation and the API categorization (Categorization member
// type) and name (Name member type).

template<typename C>
constexpr static bool IsApiCategorization = false;
// Metafunction determining whether a type is an API categorization or not.
// An API categorization is a TypeList of StringTs.

template<typename C>
constexpr static bool IsApiDemarcation = false;
// Metafunction determining whether a type is an API demarcation or not.

template<typename Module, typename Path, typename ApiDemarcations>
struct ModuleAdvertisedApis;
// Record of the APIs advertised by a module. When a module advertises APIs, its DMarc contains a list of
// ApiDemarcations. This type records a module, its path, and the ApiDemarcations advertised by that module.

template<typename Module, typename Path, typename State>
struct ApiAdvertisementAccumulator;
// Accumulator for Mdlr::WalkModuleTree which builds a TypeList of ModuleAdvertisedApis records for all modules which
// advertised any APIs.

struct ApiAdvertisement {
// A runtime data record of an API advertisement
    std::vector<std::string> categorization;
    // Categorization of the API, formatted as a series of strings containing the category, subcategory, etc.
    std::string name;
    // Name of the API
};

template<typename ApiDemarcations, typename Query>
constexpr auto FindMatchingAdvertisements();
// Given a TypeList of ApiDemarcation types and a StringT API query, find the API demarcations that match the query.
// Query is a StringT with an API categorization and/or name, delimited by forward slashes.
// Returns a struct with member types ExactMatches and InexactMatches which are TypeLists of the matching demarcations

template<typename... Modules>
class ApiManager {
// A switchboard of APIs for the provided list of modules. The ApiManager is templated on a list of modules, and it
// scans them and their submodule trees for all advertised APIs and API categories. From there, it provides two main
// services: API advertisement listing, and API allocation. Advertisement listing answers the question "What APIs are
// available?" with a list of all advertised APIs and their categories. Allocation is used to get a particular API by
// name, and optionally, category, in order to access and invoke the API.

    struct Storage;
    // Data storage for the ApiManager
    std::unique_ptr<Storage> storage = std::make_unique<Storage>();

public:
    struct Info {
    // A namespace struct containing static information about the modules the APIManager manages
        using ModuleList = TL::List<Modules...>;
        // List of module types the ApiManager manages
        using Advertisements = TL::concat<typename Mdlr::WalkModuleTree<Modules, ApiAdvertisementAccumulator>...>;
        // List of all static API advertisement records from all modules and submodules
        using ApiTypes = TL::List<>;
        // TODO: A list of all known API types.
    };

    ApiManager(Modules& ...modules);
    // Constructor of ApiManager, which takes references to the managed modules.

    std::vector<ApiAdvertisement> GetAdvertisedApis();
    // Get a listing of all advertised APIs.

    struct UnknownApi {};
    // Empty type which indicates that the requested API did not match any known API from any module
    struct AmbiguousRequest {
    // A type indicating that the API manager could not unambiguously match the requested API to an API to allocate
    // without a more specific request.
        std::vector<ApiAdvertisement> exactMatches;
        // List of API advertisements which matched the query exactly
        std::vector<ApiAdvertisement> looseMatches;
        // List of API advertisements which matched the query loosely
    };
    using AllocatedApi = TL::apply<typename Info::ApiTypes, StaticVariant>;
    // An allocated API, which is one of the various API types managed by the ApiManager
    using ApiAllocationResult = StaticVariant<AllocatedApi, UnknownApi, AmbiguousRequest>;

    ApiAllocationResult AllocateApi(std::string request);
    // Allocate an API based on a request string which contains an optional API categorization and an API name,
    // delimited by forward slashes.
    ApiAllocationResult AllocateApi(std::string name, std::vector<std::string> categorization = {});
    // Overload of AllocateApi that explcitly delineates the API name and categories
    template<typename Api>
    StaticVariant<Api, UnknownApi, AmbiguousRequest> AllocateApi(std::string name,
                                                                 std::vector<std::string> categorization = {});
    // Overload of AllocateApi where the returned API type is explicitly provided. If an exact match is found, but it
    // cannot be converted to the provided type, an AmbiguousRequest with one exact match is returned.
};

// Macro: ADD_STATIC_API_ALLOCATOR(Advertisements)
// Defines a template method, allocateApi<Query>(), to this class which allocates an API based on a query


/* DEFINITIONS AND SPECIALIZATIONS */

template<typename ApiCategorization, typename ApiName, typename MethodDemarcation>
struct ApiDemarcation : public MethodDemarcation {
    static_assert(IsApiCategorization<ApiCategorization>, "Invalid API categorization");
    static_assert(IsStringT<ApiName>, "Invalid API name");

    using Name = ApiName;
    using Categorization = ApiCategorization;
};
template<typename Name, typename Method, char... category>
struct ApiDemarcation<StringT<category...>, Name, Method>
       : public ApiDemarcation<TypeList::List<StringT<category...>>, Name, Method> {};

template<typename... Categories>
constexpr static bool IsApiCategorization<TL::List<Categories...>> =
    std::conjunction_v<std::integral_constant<bool, IsStringT<Categories>>...>;

template<typename Cat, typename Name, typename Dmarc>
constexpr static bool IsApiDemarcation<ApiDemarcation<Cat, Name, Dmarc>> = true;

template<typename M, typename P, typename Ads>
struct ModuleAdvertisedApis {
    using Module = M;
    using Path = P;
    using ApiAdvertisements = Ads;
};

template<typename Module, typename Path, typename State>
struct ApiAdvertisementAccumulator {
    static_assert(TL::IsTypeList<State>, "State must be a TypeList");
    using Advertisements = DMarcTag<Module, ApiTag>;
    using Type = std::conditional_t<TL::length<Advertisements>() != 0,
                                    TL::append<State, ModuleAdvertisedApis<Module, Path, Advertisements>>,
                                    State>;
};

template<typename Query>
struct MatchQuery {
    template<typename... C1, typename... C2>
    constexpr static bool CategorizationsMatch(TL::List<C1...>, TL::List<C2...>) {
        // If categorizations are the same length, they match if identical
        if constexpr (sizeof...(C1) == sizeof...(C2))
            return std::conjunction_v<std::bool_constant<C1() == C2()>...>;
        // If categorizations are not same length, match if first is prefix of second, but not vice versa.
        else if constexpr (sizeof...(C1) < sizeof...(C2)) {
            return CategorizationsMatch(TL::List<C1...>(), TL::slice<TL::List<C2...>, 0, sizeof...(C1)>());
        } else
            return false;
    }

    template<typename Demarcation>
    struct filterExact {
        using Categorization = typename Demarcation::Categorization;
        using Name = typename Demarcation::Name;
        constexpr static bool value =
                (CategorizationsMatch(TL::slice<Query, 0, TL::length<Query>()-1>(), Categorization()) &&
                 TL::last<Query>() == Name());
    };
    template<typename Demarcation>
    struct filterInexact {
        using Categorization = typename Demarcation::Categorization;
        using Name = typename Demarcation::Name;
        constexpr static bool value = CategorizationsMatch(Query(), Categorization());
    };

    template<typename Demarcations>
    static auto FilterDemarcations() {
        struct {
            using ExactMatches = TL::filter<Demarcations, filterExact>;
            using InexactMatches = TL::filter<Demarcations, filterInexact>;
        } result;
        return result;
    }
};

template<typename ApiDemarcations, typename Query>
constexpr auto FindMatchingAdvertisements() {
    static_assert(IsStringT<Query>, "Query must be a StringT compile time string");
    static_assert(TL::IsTypeList<ApiDemarcations>, "ApiDemarcations must be a TypeList of ApiDemarcation types");
    TL::runtime::ForEach(ApiDemarcations(), [](auto d) {
        // Not actually runtime work; just compiles out
        using Demarcation = typename decltype(d)::type;
        static_assert(IsApiCategorization<typename Demarcation::Categorization>,
                      "Invalid type in ApiDemarcations list");
        static_assert(IsStringT<typename Demarcation::Name>, "Invalid type in ApiDemarcations list");
    });

    using SplitQuery = SplitStringT<Query, '/'>;

    if constexpr (TL::length<SplitQuery>() == 0) {
        struct { using ExactMatches = TL::List<>; using InexactMatches = ApiDemarcations; } r;
        return r;
    } else {
        return MatchQuery<SplitQuery>::template FilterDemarcations<ApiDemarcations>();
    }
}

} // namespace Infra::Api

namespace Modular {
template<>
struct AccumulatorEmptyState_S<Api::ApiAdvertisementAccumulator> {
    using Type = TL::List<>;
};
} } // namespace Infra::Modular

#define ADD_STATIC_API_ALLOCATOR(Advertisements) \
    template<typename ApiQuery> \
    auto allocateApi() { \
        using Results = decltype(Api::FindMatchingAdvertisements<Advertisements, ApiQuery>()); \
        static_assert(TL::length<typename Results::ExactMatches>() == 1, "API Query must exactly match one API"); \
        static_assert(TL::length<typename Results::InexactMatches>() == 0, "API Query must exactly match one API"); \
 \
        using ApiDMarc = TL::first<typename Results::ExactMatches>; \
        static_assert(TL::length<typename ApiDMarc::ArgumentTypes>() == 0, \
                      "API allocators with arguments are not yet supported."); \
 \
        return ApiDMarc::Invoke(*this); \
    }

