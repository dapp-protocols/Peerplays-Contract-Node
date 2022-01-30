#pragma once

#include "P2pHandler.hpp"
#include "ChainHandler.hpp"

#include <Infra/Infra.hpp>
#include <Infra/ApiManager.hpp>

#include <boost/asio/signal_set.hpp>
#include <boost/dll/import.hpp>

#include <memory>
#include <optional>

namespace Node {
namespace Api = Infra::Api;
namespace TL = Infra::TypeList;
namespace Mdlr = Infra::Modular;

namespace DLL = boost::dll;
namespace BFS = DLL::fs;

class ContractNode {
    char* argv = nullptr;
    char** argc = nullptr;
    std::unique_ptr<ChainHandler> chainHandler;
    std::unique_ptr<P2pHandler> p2pHandler;
    std::unique_ptr<boost::asio::signal_set> signalSet;
    std::vector<std::unique_ptr<ChainHandler::ContractDatabaseMonitor>> contractMonitors;

    fc::thread& mainThread;

    using LibraryPointer = std::unique_ptr<boost::dll::shared_library>;
    std::map<BFS::path, LibraryPointer> loadedLibraries;
    fc::promise<bool>::ptr exitPromise;

    void initializeBlockchain();

    bool loadPlugin(BFS::path file);
    std::vector<BFS::path> loadPlugins(BFS::path directory);
    std::vector<BFS::path> searchForPlugins(BFS::path programPath);
    bool initializePlugin(LibraryPointer& library);
    void initializePlugins(std::vector<BFS::path> pluginPaths);

    void dumpContractDatabases() const;

    bool waitForExit();
    void signalHandler(boost::system::error_code error, int signal);

    std::optional<Sgnl::connection> blockConnection;
    std::optional<Sgnl::connection> transactionConnection;

public:
    ContractNode(char* argv = nullptr, char** argc = nullptr);
    ~ContractNode();

    ChainHandler* getChainHandler() { return chainHandler.get(); }
    P2pHandler* getP2pHandler() { return p2pHandler.get(); }

    int run();
    void exit(bool withError) { exitPromise->set_value(withError); }

    using Submodules = TL::List<DEMARCATE(ContractNode::getChainHandler),
                                DEMARCATE(ContractNode::getP2pHandler)>;
    using ApiAdvertisements = TL::List<>;

    using DMarc = TL::List<TL::List<Api::ApiTag, ApiAdvertisements>, TL::List<Mdlr::SubmoduleTag, Submodules>>;
};

} // namespace Node
