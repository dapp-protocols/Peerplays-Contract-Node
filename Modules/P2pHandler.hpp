#pragma once

#include <graphene/net/node.hpp>
#include <graphene/chain/database.hpp>

#include <boost/signals2/signal.hpp>

namespace Net = graphene::net;
namespace Chain = graphene::chain;

namespace Sgnl = boost::signals2;

class P2pHandler {
    /// Implementation of net::node's node_delegate interface, so net::node can get info about our blockchain
    class NodeInterface : public graphene::net::node_delegate {
        P2pHandler& handler;

        bool blockIsInOurChain(Chain::block_id_type id);

    public:
        NodeInterface(P2pHandler& handler);

        // node_delegate interface implementation
        bool has_item(const Net::item_id& id) override;
        bool handle_block(const Net::block_message& blk_msg, bool sync_mode, std::vector<fc::uint160_t>& contained_transaction_message_ids) override;
        void handle_transaction(const Net::trx_message& trx_msg) override;
        void handle_message(const Net::message& message_to_process) override;
        std::vector<Net::item_hash_t> get_block_ids(const std::vector<Net::item_hash_t>& blockchain_synopsis, uint32_t& remaining_item_count, uint32_t limit) override;
        Net::message get_item(const Net::item_id& id) override;
        graphene::protocol::chain_id_type get_chain_id() const override;
        std::vector<Net::item_hash_t> get_blockchain_synopsis(const Net::item_hash_t& reference_point, uint32_t number_of_blocks_after_reference_point) override;
        void sync_status(uint32_t item_type, uint32_t item_count) override;
        void connection_count_changed(uint32_t c) override;
        uint32_t get_block_number(const Net::item_hash_t& block_id) override;
        fc::time_point_sec get_block_time(const Net::item_hash_t& block_id) override;
        Net::item_hash_t get_head_block_id() const override;
        uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const override;
        void error_encountered(const std::string& message, const fc::oexception& error) override;
        uint8_t get_current_block_interval_in_seconds() const override;
    };

    Net::node node;
    const Chain::database& db;
    bool syncing = false;
    std::unique_ptr<NodeInterface> nodeInterface = std::make_unique<NodeInterface>(*this);

public:
    P2pHandler(const Chain::database& db) : node("Pollaris Backend Node"), db(db) {
        node.load_configuration(fc::home_path() / ".config/Follow My Vote/PollarisBackend/p2p");
        node.set_node_delegate(nodeInterface.get());
    }
    ~P2pHandler() {
        node.close();
    }

    bool isSyncing() const { return syncing; }
    void syncFrom(Chain::block_id_type blockId);

    void connectToSeeds();

    Sgnl::signal<void(Chain::signed_block)> blockReceived;
    Sgnl::signal<void(Chain::signed_transaction)> transactionReceived;
    Sgnl::signal<void()> syncFinished;
};
