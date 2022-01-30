#include "ChainHandler.hpp"

#include <graphene/chain/genesis_state.hpp>
#include <graphene/utilities/key_conversion.hpp>

#include <fc/io/fstream.hpp>

#include <boost/multi_index/indexed_by.hpp>
#include <limits>

constexpr static uint8_t CONTRACT_RECORD_TYPE_ID = 0;
using ContractRecordId = protocol::object_id<ChainHandler::FIRST_AVAILABLE_SPACE_ID, CONTRACT_RECORD_TYPE_ID>;

// A record of a contract that has been loaded into this node before.
// Primarily used to consistently assign object space IDs.
struct ContractRecord : public db::abstract_object<ContractRecord> {
    // This object goes in the persistence DB, so it can be space/type zero.
    const static uint8_t space_id = 0;
    const static uint8_t type_id = 0;

    std::string name;

    // The space ID assigned to this contract in the chain database (not the persistence database)
    uint8_t contractObjectSpaceId() const { return ChainHandler::FIRST_AVAILABLE_SPACE_ID + id.number; }
};

FC_REFLECT_DERIVED(ContractRecord, (db::object), (name));

namespace mic = boost::multi_index;
using db::by_id;
using chain::by_name;

using ContractRecordIndex = db::primary_index<
    db::generic_index<ContractRecord,
        boost::multi_index_container<ContractRecord,
        mic::indexed_by<
            mic::ordered_unique<mic::tag<by_id>,
                                mic::member<chain::object, chain::object_id_type, &ContractRecord::id>>,
            mic::ordered_unique<mic::tag<by_name>,
                                mic::member<ContractRecord, std::string, &ContractRecord::name>>>>>>;

ChainHandler::ChainHandler() {}
ChainHandler::~ChainHandler() {
    chain.close();
    persistence.flush();
    persistence.close();
}

void ChainHandler::initialize() {
    persistence.add_index<ContractRecordIndex>();
    persistence.open(persistencePath());
}

void ChainHandler::open() {
    auto computeGenesis = [this] {
        using namespace graphene::chain;
        using graphene::utilities::key_to_wif;

        auto genesisPath = chainPath().parent_path() / "genesis.json";
        if (fc::is_regular_file(genesisPath)) {
            ilog("Using genesis at ${G}", ("G", genesisPath));

            auto genesis = fc::json::from_file(genesisPath).as<genesis_state_type>(MAX_RECURSION_DEPTH);
            genesis.initial_chain_id = fc::sha256::hash(fc::json::to_string(genesis) + "\n");
            return genesis;
        }

        auto genesisKey = fc::ecc::private_key::regenerate(fc::sha256::hash("Pollaris Development Key"));
        auto genesisKeyPair = std::make_pair(public_key_type(genesisKey.get_public_key()), key_to_wif((genesisKey)));
        ilog("Configuring genesis with key ${key}", ("key", genesisKeyPair));

        genesis_state_type genesis;
        genesis.initial_timestamp = fc::time_point::now();
        genesis.initial_timestamp -= genesis.initial_timestamp.sec_since_epoch() % GRAPHENE_DEFAULT_BLOCK_INTERVAL;
        genesis.initial_parameters.current_fees = std::make_shared<fee_schedule>(fee_schedule::get_default());
        genesis.initial_accounts = {{"init", genesisKey.get_public_key(), genesisKey.get_public_key(), true}};
        genesis.initial_balances = {{genesisKey.get_public_key(), GRAPHENE_SYMBOL, GRAPHENE_MAX_SHARE_SUPPLY}};
        genesis.initial_active_witnesses = 1;
        genesis.initial_witness_candidates = {{"init", genesisKey.get_public_key()}};

        ilog("Saving genesis to ${G}", ("G", genesisPath));
        auto genesisString = fc::json::to_string(genesis) + "\n";
        fc::ofstream genesisFile(genesisPath);
        genesisFile.write(genesisString.data(), genesisString.size());
        genesisFile.close();

        genesis.initial_chain_id = fc::sha256::hash(genesisString);
        return genesis;
    };

    ilog("[ChainHandler] Opening chain with data directory ${D}", ("D", chainPath()));
    chain.open(chainPath(), computeGenesis, GRAPHENE_CURRENT_DB_VERSION);
    isOpen = true;
}

bool ChainHandler::initializeContract(const std::string& name,
                                      std::function<bool (chain::database&, uint8_t)> initFunction) {
    auto& primaryIndex = persistence.get_index_type<ContractRecordIndex>();

    // If this contract is one we've seen before, use the original space ID instead of a new one
    auto& indexByName = primaryIndex.indices().get<by_name>();
    auto itr = indexByName.lower_bound(name);
    if (itr != indexByName.end() && itr->name == name) {
        ilog("[ChainHandler] Recognized contract ${N} with space ID ${S}",
             ("N", name)("S", itr->contractObjectSpaceId()));
    } else {
        // Create a record of our new object
        auto& record = persistence.create<ContractRecord>([&name](ContractRecord& record) { record.name = name; });
        itr = indexByName.iterator_to(record);
        ilog("Assigning contract ${N} a new object space: ${S}.", ("N", name)("S", itr->contractObjectSpaceId()));
    }

    if (initFunction(chain, itr->contractObjectSpaceId())) {
        loadedContracts.emplace(itr->contractObjectSpaceId(), name);
        return true;
    }
    return false;
}

struct TableMonitor : public db::secondary_index {
    uint8_t typeId = 0;
    ObjectSignal* object_loaded_signal = nullptr;
    ObjectSignal* object_created_signal = nullptr;
    ObjectSignal* object_deleted_signal = nullptr;
    ObjectSignal* object_modified_signal = nullptr;
    std::optional<fc::variant_object> preModifiedObject;

    // secondary_index interface
    void object_loaded(const db::object& obj) override {
        FC_ASSERT(object_loaded_signal != nullptr, "[ChainHandler] Table Monitor used before being initialized!");
        (*object_loaded_signal)(typeId, obj.to_variant().get_object());
    }
    void object_created(const db::object& obj) override {
        FC_ASSERT(object_created_signal != nullptr, "[ChainHandler] Table Monitor used before being initialized!");
        (*object_created_signal)(typeId, obj.to_variant().get_object());
    }
    void object_removed(const db::object& obj) override {
        FC_ASSERT(object_deleted_signal != nullptr, "[ChainHandler] Table Monitor used before being initialized!");
        (*object_deleted_signal)(typeId, obj.to_variant().get_object());
    }
    void about_to_modify(const db::object& before) override {
        preModifiedObject = before.to_variant().get_object();
    }
    void object_modified(const db::object& after) override {
        FC_ASSERT(object_modified_signal != nullptr, "[ChainHandler] Table Monitor used before being initialized!");
        auto object = after.to_variant().get_object();
        if (preModifiedObject.has_value()) {
            auto change = fc::mutable_variant_object("from", std::move(*preModifiedObject))("to", std::move(object));
            (*object_modified_signal)(typeId, std::move(change));
            preModifiedObject.reset();
        } else {
            elog("[ChainHandler] Object notified of post-modified object without having been notified of pre-modified"
                 " object! Post-modified object: ${O}",
                 ("O", object));
        }
    }
};

class MultiTableMonitor : public ChainHandler::ContractDatabaseMonitor {
    db::object_database& db;
    std::vector<TableMonitor*> monitors;

public:
    MultiTableMonitor(const std::string& contractName, const uint8_t spaceId, db::object_database& db)
        : ContractDatabaseMonitor(contractName, spaceId), db(db) {

        // Find each table in the object space and attach a monitor to it
        db.inspect_all_indexes(spaceId, [&db, this](const db::index& index) {
            try {
                auto* monitor = db.add_secondary_index<TableMonitor>(index.object_space_id(), index.object_type_id());
                monitor->typeId = index.object_type_id();
                monitor->object_loaded_signal = &object_loaded;
                monitor->object_created_signal = &object_created;
                monitor->object_deleted_signal = &object_deleted;
                monitor->object_modified_signal = &object_modified;
                monitors.emplace_back(monitor);
            } catch (fc::exception_ptr e) {
                elog("[ChainHandler] Failed to monitor table ${S}.${T} due to error. Proceeding with other tables."
                     " Error: ${E}", ("S", index.object_space_id())("T", index.object_type_id())("E", *e));
            }
        });
    }
    virtual ~MultiTableMonitor() {
        // Destroy the table monitors
        for (const TableMonitor* monitor : monitors)
            db.delete_secondary_index(spaceId, monitor->typeId, *monitor);
        monitors.clear();
    }
};

std::unique_ptr<ChainHandler::ContractDatabaseMonitor> ChainHandler::observeContract(uint8_t spaceId,
                                                                                     const std::string& name) {
    return std::make_unique<MultiTableMonitor>(name, spaceId, chain);
}
