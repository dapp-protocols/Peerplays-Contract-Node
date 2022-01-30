#pragma once

#include <graphene/chain/database.hpp>

#include <boost/config.hpp>

// REQUIRED:
// This function is called to allow the contract to register itself with the blockchain.
extern "C" BOOST_SYMBOL_EXPORT bool registerContract(graphene::chain::database&, uint8_t spaceId);

// The rest of the interface is optional -- contracts may implement these as desired for additional features, but if
// not available, the contract will still operate.

// Contracts may supply a human-readable name string like so:
extern "C" BOOST_SYMBOL_EXPORT const char* contractName;

// This function is called to notify the contract that it will be unloaded, and allow it to deregister itself first.
// Implement this function to add support for live reloading of the contract.
extern "C" BOOST_SYMBOL_EXPORT void deregisterContract();

// A list of strings with list length. Strings are expected to be null-terminated.
struct StringList {
    const char** const values;
    const uint32_t count;
};

// A list of table names for the contract. If provided, it should contain a name for every table registered with the
// chain, in the same order as the tables' type IDs with the chain.
extern "C" BOOST_SYMBOL_EXPORT const StringList* tableNames;
