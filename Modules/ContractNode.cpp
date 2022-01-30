#include "ContractNode.hpp"

#include <ContractApi.hpp>

#include <fc/interprocess/signals.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/asio.hpp>

#include <boost/dll/runtime_symbol_info.hpp>

namespace Node {

bool ContractNode::waitForExit() {
    exitPromise = fc::promise<bool>::create("Exit promise");

    // Set signal handler
    if (signalSet == nullptr) {
        signalSet.reset(new boost::asio::signal_set(fc::asio::default_io_service(), SIGINT, SIGUSR1, SIGUSR2));
        signalSet->async_wait([this](auto a, auto b) { signalHandler(a, b); });
    }

    return exitPromise->wait();
}

void ContractNode::signalHandler(boost::system::error_code error, int signal) {
    if (error == boost::asio::error::operation_aborted)
        return;

    if (signal == SIGINT) {
        // Set exit promise. Resolving a future is super fast, so no need to async it
        exitPromise->set_value(false);
    } else if (signal == SIGUSR1) {
        ilog("Received SIGUSR1 -- searching for new plugins");
        // Async this to keep signal handler snappy
        mainThread.async([this] { initializePlugins(searchForPlugins(DLL::program_location().parent_path())); },
                  "SIGUSR1 Handler");
    } else if (signal == SIGUSR2) {
        ilog("Received SIGUSR2 -- dumping all contract databases");
        // Async this to keep signal handler snappy
        mainThread.async([this] { this->dumpContractDatabases(); },
                  "SIGUSR2 Handler");
    }

    // Re-set the signal handler
    if (signalSet != nullptr)
        signalSet->async_wait([this](auto a, auto b) { signalHandler(a, b); });
}

void ContractNode::initializeBlockchain() {
    // Sanity check
    if (!chainHandler) {
        elog("Cannot initialize blockchain: ChainHandler not yet created!");
        return;
    }
    chainHandler->initialize();

    auto programPath = DLL::program_location().parent_path();
    ilog("Node path: ${P}", ("P", programPath.string()));
    initializePlugins(searchForPlugins(programPath));
}

bool ContractNode::loadPlugin(BFS::path file) {
    try {
        ilog("Checking plugin ${P}", ("P", file.string()));
        auto plugin = std::make_unique<DLL::shared_library>(file);
        if (plugin->is_loaded() && plugin->has("registerContract")) {
            ilog("Loaded plugin: ${P}", ("P", file.string()));
            loadedLibraries.emplace(file, std::move(plugin));
            return true;
        } else {
            ilog("Plugin failed to load: ${P}", ("P", file.string()));
            plugin.reset();
            return false;
        }
    } catch (const boost::system::system_error& e) {
        elog("Failed to load plugin: ${P}\nError: ${E}", ("P", file.string())("E", e.what()));
        return false;
    }
}

std::vector<BFS::path> ContractNode::loadPlugins(BFS::path directory) {
    std::vector<BFS::path> loadedPlugins;
    directory = BFS::weakly_canonical(directory);
    ilog("Searching for plugins in ${D}", ("D", directory.string()));

    if (!BFS::is_directory(directory))
        return loadedPlugins;

    // Search directory for files with the right extension that we haven't already loaded
    for(BFS::directory_iterator file(directory); file != BFS::directory_iterator(); ++file) {
        auto path = file->path();
        std::set<BFS::path> checkedExtensions = {".so", ".dylib", ".dll"};
        if (checkedExtensions.count(path.extension()) &&
                loadedLibraries.count(path) == 0) {
            if (loadPlugin(path))
                loadedPlugins.emplace_back(path);
        }
    }

    return loadedPlugins;
}

std::vector<BFS::path> ContractNode::searchForPlugins(BFS::path programPath) {
    auto loadedPlugins = loadPlugins(programPath / "plugins");
    auto binaryName = DLL::program_location().stem();
    auto more = loadPlugins(programPath / "../lib" / binaryName / "plugins");
    loadedPlugins.insert(loadedPlugins.end(),
                         std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
    return loadedPlugins;
}

bool ContractNode::initializePlugin(LibraryPointer& library) {
    std::string contractName = library->location().filename().string();

    // Try to initialize the contract
    try {
        if (library->has("contractName"))
            contractName = library->template get<const char*>("contractName");

        const StringList* tables = nullptr;
        if (library->has("tableNames"))
            tables = library->template get<const StringList*>("tableNames");

        auto initialize = library->template get<bool(graphene::chain::database&, uint8_t)>("registerContract");
        if (chainHandler->initializeContract(contractName, initialize)) {
            auto monitor = chainHandler->observeContract(contractName);
            monitor->object_created.connect([tables, contractName](uint8_t type, const fc::variant_object& o) {
                std::string tableName;
                if (tables && tables->count > type)
                    tableName = tables->values[type];
                else
                    tableName = std::to_string(type);

                dlog("Contract ${C} has created a new object in its ${T} table:\n${O}",
                     ("C", contractName)("T", tableName)("O", o));
            });
            monitor->object_deleted.connect([tables, contractName](uint8_t type, const fc::variant_object& o) {
                std::string tableName;
                if (tables && tables->count > type)
                    tableName = tables->values[type];
                else
                    tableName = std::to_string(type);

                dlog("Contract ${C} has deleted an object in its ${T} table:\n${O}",
                     ("C", contractName)("T", tableName)("O", o));
            });
            monitor->object_modified.connect([tables, contractName](uint8_t type, const fc::variant_object& o) {
                std::string tableName;
                if (tables && tables->count > type)
                    tableName = tables->values[type];
                else
                    tableName = std::to_string(type);

                dlog("Contract ${C} has modified an object in its ${T} table:\n${O}",
                     ("C", contractName)("T", tableName)("O", o));
            });
            contractMonitors.emplace_back(std::move(monitor));

            ilog("Contract ${N} initialized successfully.", ("N", contractName));

            return true;
        } else {
            elog("Contract ${N} failed to initialize.", ("N", contractName));
            return false;
        }
    } catch (const boost::exception& e) {
        ilog("Failed to initialize plugin ${P}", ("P", contractName));
        return false;
    }
}

void ContractNode::initializePlugins(std::vector<BFS::path> pluginPaths) {
    // Fetch the register function and call it on each plugin to initialize
    for (const auto& path : pluginPaths) {
        auto itr = loadedLibraries.find(path);
        if (initializePlugin(itr->second))
            ++itr;
        else
            itr = loadedLibraries.erase(itr);
    }
}

void ContractNode::dumpContractDatabases() const {
    for (const auto& [id, name] : chainHandler->getLoadedContracts()) {
        dlog("Dumping database for contract: ${N}", ("N", name));
        auto dumper = [typeId=-1](const fc::variant_object& object) mutable {
            if (object.contains("id")) {
                auto newTypeId = object.find("id")->value().as<db::object_id_type>(1).type();
                if (newTypeId != typeId) {
                    typeId = newTypeId;
                    // TODO: Convert table type ID to table name
                    // This will require some refactoring to make contracts indexable by space ID
                    dlog("");
                    dlog("Table ${T}:", ("T", typeId));
                }
            }

            ddump((object));
        };
        chainHandler->inspectContractDatabase(id, dumper);
    }
}

ContractNode::ContractNode(char* argv, char** argc) : argv(argv), argc(argc), mainThread(fc::thread::current()) {}
ContractNode::~ContractNode() {}

int ContractNode::run() {
    // OK, time to start the program. First, create the chain, initialize the blockchain, and open the database.
    try {
        ilog("Creating blockchain");
        chainHandler = std::make_unique<ChainHandler>();
        ilog("Contract node configuration directory: ${D}", ("D", chainHandler->getConfigPath()));

        ilog("Initializing blockchain");
        initializeBlockchain();

        ilog("Loading database");
        chainHandler->open();
        ilog("Blockchain opened successfully at block #${num}", ("num", chainHandler->getChain().head_block_num()));
        ilog("Chain ID is ${cid}", ("cid", chainHandler->getChain().get_chain_id()));
    } catch (const fc::exception& e) {
        elog("Failed to initialize chain: ${e}", ("e", e.to_detail_string()));
        return 1;
    }

    try {
        // Now create the P2P node, giving it read access to the chain database
        ilog("Creating P2P Node");
        p2pHandler = std::make_unique<P2pHandler>(chainHandler->getChain());
        blockConnection = p2pHandler->blockReceived.connect([this](const chain::signed_block& block) {
            if (block.previous == chainHandler->getChain().head_block_id()) {
                ilog("Received next block in chain: #${num}, block time ${time}",
                     ("num", block.block_num())("time", block.timestamp));
                try {
                    chainHandler->getChain().push_block(block);
                    FC_ASSERT(chainHandler->getChain().head_block_id() == block.id(),
                              "Block pushed OK, but did not update chain");
                } catch (const fc::exception& e) {
                    elog("Failed to push block to chain: ${e}", ("e", e.to_detail_string()));
                }
            } else {
                wlog("Got a block, but it's not the next one in the chain. Ignoring it.");
            }
        });
        transactionConnection = p2pHandler->transactionReceived.connect([](const chain::signed_transaction& trx) {
            ilog("Got TRX ID ${id}, but I don't care about transactions, so I'm ignoring it.", ("id", trx.id()));
        });

        // Connect P2P node to seed nodes
        ilog("Connecting to seed nodes");
        p2pHandler->connectToSeeds();

        // Begin sync
        ilog("Beginning sync");
        p2pHandler->syncFrom(chainHandler->getChain().head_block_id());
    } catch (const fc::exception& e) {
        elog("Failed to initialize P2P Node: ${e}", ("e", e.to_detail_string()));
        return 1;
    }

    ilog("Node stable");
    return waitForExit();
}

} // namespace Node
