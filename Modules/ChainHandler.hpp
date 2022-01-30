#pragma once

#include <graphene/chain/database.hpp>

#include <fc/reflect/variant.hpp>

#include <boost/signals2/signal.hpp>
#include <memory>

namespace chain = graphene::chain;
namespace protocol = graphene::protocol;
namespace db = graphene::db;

namespace sig = boost::signals2;

using ObjectSignal = sig::signal<void(uint8_t, fc::variant_object)>;

class MultiTableMonitor;

// The ChainHandler is responsible for managing the blockchain and contract databases.
class ChainHandler {
    // Meh, pick a number...
    constexpr static size_t MAX_RECURSION_DEPTH = 200;

    // Paths for the databases
    fc::path basePath = fc::home_path() / ".config/PeerplaysContractNode";
    fc::path chainPath() const { return basePath / "Chain"; }
    fc::path persistencePath() const { return basePath / "NodePersistence"; }

    // The blockchain/database
    chain::database chain;
    // The persistence database for the node's off-chain settings
    db::object_database persistence;

    // Whether the database is open or not
    bool isOpen = false;

    // Map of contract space ID to name
    std::map<uint8_t, std::string> loadedContracts;
    // Map of object space ID and type ID to an observer of that table
    std::map<std::pair<uint8_t, uint8_t>, MultiTableMonitor*> observers;

public:
    // The lowest object space in the blockchain database that we assign to contracts
    static constexpr uint8_t FIRST_AVAILABLE_SPACE_ID = 10;

    struct ContractDatabaseMonitor {
        ContractDatabaseMonitor(const std::string& contractName, const uint8_t spaceId)
            : contractName(contractName), spaceId(spaceId) {}
        virtual ~ContractDatabaseMonitor() {}

        // Name of the contract being observed
        const std::string contractName;
        // Space ID of the contract being observed
        const uint8_t spaceId = 0;

        // Notification that an object was loaded from disk; passes type ID and loaded object
        ObjectSignal object_loaded;
        // Notification that a new object was created; passes type ID and created object
        ObjectSignal object_created;
        // Notification that an object was deleted; passes type ID and object value prior to deletion
        ObjectSignal object_deleted;
        // Notification that on object was updated; passes type ID and object like {"from": <object>, "to": <object>}
        ObjectSignal object_modified;
    };

    ChainHandler();
    ~ChainHandler();

    // Manage the path for the databases
    fc::path getConfigPath() const { return basePath; }
    void setConfigPath(fc::path newPath) {
        FC_ASSERT(!isOpen, "Cannot set path after the databases are opened");
        basePath = newPath;
    }
    chain::database& getChain() { return chain; }

    // Initialize the databases
    void initialize();
    // Open the databases
    void open();

    // Get a map of database space IDs to contract name for all contracts currently loaded into the blockchain
    const std::map<uint8_t, std::string>& getLoadedContracts() const { return loadedContracts; }
    // Get the space ID of the contract with the given name
    uint8_t getSpaceId(const std::string& contractName) const {
         auto itr = std::find_if(loadedContracts.cbegin(), loadedContracts.cend(),
                                [&contractName](const auto& pair) { return pair.second == contractName; });
        FC_ASSERT(itr != loadedContracts.cend(), "[ChainHandler] Could not find contract named ${N}",
                  ("N", contractName));
        return itr->first;
    }

    // Load a contract into the blockchain, assigning it a space ID. Returns the result of the contract initializer.
    bool initializeContract(const std::string& name, std::function<bool(chain::database&, uint8_t)> initFunction);

    // Get signals notifying of a contract's database activity
    std::unique_ptr<ContractDatabaseMonitor> observeContract(uint8_t spaceId) {
        auto itr = loadedContracts.find(spaceId);
        if (itr != loadedContracts.end())
            return observeContract(spaceId, itr->second);
        return observeContract(spaceId, "Unknown Contract");
    }
    std::unique_ptr<ContractDatabaseMonitor> observeContract(const std::string& name) {
        return observeContract(getSpaceId(name), name);
    }
    std::unique_ptr<ContractDatabaseMonitor> observeContract(uint8_t spaceId, const std::string& name);

    // Inspect all objects in a contract's database. F is a functor taking a fc::variant_object.
    // Functor will be called once for every object in every table of the contract's database, in order.
    template<typename F>
    void inspectContractDatabase(uint8_t spaceId, F&& f) const {
        chain.inspect_all_indexes(spaceId, [&f](const db::index& index) {
            index.inspect_all_objects([&f](const db::object& object) {
                f(object.to_variant().get_object());
            });
        });
    }
    template<typename F>
    void inspectContractDatabase(const std::string& name, F&& f) {
        inspectContractDatabase(getSpaceId(name), std::forward<F>(f));
    }
};
