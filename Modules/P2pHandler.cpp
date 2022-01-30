#include "P2pHandler.hpp"

#include <graphene/net/exceptions.hpp>

#include <fc/log/logger.hpp>
#include <fc/network/resolve.hpp>

#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/algorithm/reverse.hpp>
#include <boost/range/algorithm/transform.hpp>

bool P2pHandler::NodeInterface::blockIsInOurChain(graphene::protocol::block_id_type id) {
    uint32_t block_num = Chain::block_header::num_from_id(id);
    Chain::block_id_type block_id_in_preferred_chain = handler.db.get_block_id_for_num(block_num);
    return id == block_id_in_preferred_chain;
}

P2pHandler::NodeInterface::NodeInterface(P2pHandler& handler) : handler(handler) {}

bool P2pHandler::NodeInterface::has_item(const Net::item_id& id) {
    if (id.item_type == Net::block_message_type)
        return handler.db.is_known_block(id.item_hash);
    else if (id.item_type == Net::trx_message_type)
        return handler.db.is_known_transaction(id.item_hash);
    else {
        elog("net::node asked if we recognize ID of unkown type: ${id}", ("id", id));
        return false;
    }
}

bool P2pHandler::NodeInterface::handle_block(const Net::block_message& blk_msg, bool sync_mode, std::vector<fc::uint160_t>& contained_transaction_message_ids) {
    handler.blockReceived(blk_msg.block);

    contained_transaction_message_ids.clear();
    boost::transform(blk_msg.block.transactions, std::back_inserter(contained_transaction_message_ids),
                     [](const Chain::signed_transaction& trx) { return Net::message(Net::trx_message(trx)).id(); });

    if (!sync_mode && handler.syncing) {
        handler.syncing = false;
        handler.syncFinished();
    }

    // Whether we switched forks for this block. We always return false here now, but might be more careful later.
    return false;
}

void P2pHandler::NodeInterface::handle_transaction(const Net::trx_message& trx_msg) {
    handler.transactionReceived(trx_msg.trx);
}

void P2pHandler::NodeInterface::handle_message(const Net::message& message_to_process) {
    elog("net::node asked us to handle a message that even it doesn't know what it is: ${msg}",
         ("msg", message_to_process));
    return;
}

std::vector<Net::item_hash_t> P2pHandler::NodeInterface::get_block_ids(const std::vector<Net::item_hash_t>& blockchain_synopsis, uint32_t& remaining_item_count, uint32_t limit) {
    std::vector<Chain::block_id_type> result;
    remaining_item_count = 0;
    if (handler.db.head_block_num() == 0)
       return result;

    result.reserve(limit);
    Chain::block_id_type last_known_block_id;

    if (blockchain_synopsis.empty() ||
        (blockchain_synopsis.size() == 1 && blockchain_synopsis[0] == Chain::block_id_type()))
    {
      // peer has sent us an empty synopsis meaning they have no blocks.
      // A bug in old versions would cause them to send a synopsis containing block 000000000
      // when they had an empty blockchain, so pretend they sent the right thing here.

      // do nothing, leave last_known_block_id set to zero
    } else {
      bool found_a_block_in_synopsis = false;
      for (const Net::item_hash_t& block_id_in_synopsis : boost::adaptors::reverse(blockchain_synopsis))
        if (block_id_in_synopsis == Chain::block_id_type() ||
            (handler.db.is_known_block(block_id_in_synopsis) && blockIsInOurChain(block_id_in_synopsis)))
        {
          last_known_block_id = block_id_in_synopsis;
          found_a_block_in_synopsis = true;
          break;
        }
      if (!found_a_block_in_synopsis)
        FC_THROW_EXCEPTION(graphene::net::peer_is_on_an_unreachable_fork, "Unable to provide a list of blocks starting at any of the blocks in peer's synopsis");
    }
    for( uint32_t num = Chain::block_header::num_from_id(last_known_block_id);
         num <= handler.db.head_block_num() && result.size() < limit;
         ++num )
       if( num > 0 )
          result.push_back(handler.db.get_block_id_for_num(num));

    if( !result.empty() && Chain::block_header::num_from_id(result.back()) < handler.db.head_block_num() )
       remaining_item_count = handler.db.head_block_num() - Chain::block_header::num_from_id(result.back());

    return result;
}

Net::message P2pHandler::NodeInterface::get_item(const Net::item_id& id) {
    if (id.item_type == Net::block_message_type) {
        auto found = handler.db.fetch_block_by_id(id.item_hash);
        FC_ASSERT(found, "Could not find requested block ${id}", ("id", id.item_hash));
        return Net::block_message(*found);
    } else if (id.item_type == Net::trx_message_type)
        return Net::trx_message(handler.db.get_recent_transaction(id.item_hash));

    elog("net::node asked for item with ID of unkown type: ${id}", ("id", id));
    FC_THROW_EXCEPTION(fc::assert_exception, "Unknown message type ${type}", ("type", id.item_type));
}

graphene::protocol::chain_id_type P2pHandler::NodeInterface::get_chain_id() const {
    return handler.db.get_chain_id();
}

std::vector<Net::item_hash_t>
P2pHandler::NodeInterface::get_blockchain_synopsis(const Net::item_hash_t& reference_point,
                                                    uint32_t number_of_blocks_after_reference_point) {
    std::vector<Net::item_hash_t> synopsis;
    synopsis.reserve(30);
    uint32_t high_block_num;
    uint32_t non_fork_high_block_num;
    uint32_t low_block_num = handler.db.last_non_undoable_block_num();
    std::vector<Chain::block_id_type> fork_history;

    if (reference_point != Net::item_hash_t())
    {
        // the node is asking for a summary of the block chain up to a specified
        // block, which may or may not be on a fork
        // for now, assume it's not on a fork
        if (blockIsInOurChain(reference_point))
        {
            // reference_point is a block we know about and is on the main chain
            uint32_t reference_point_block_num = Chain::block_header::num_from_id(reference_point);
            assert(reference_point_block_num > 0);
            high_block_num = reference_point_block_num;
            non_fork_high_block_num = high_block_num;

            if (reference_point_block_num < low_block_num)
            {
                // we're on the same fork (at least as far as reference_point) but we've passed
                // reference point and could no longer undo that far if we diverged after that
                // block.  This should probably only happen due to a race condition where
                // the network thread calls this function, and then immediately pushes a bunch of blocks,
                // then the main thread finally processes this function.
                // with the current framework, there's not much we can do to tell the network
                // thread what our current head block is, so we'll just pretend that
                // our head is actually the reference point.
                // this *may* enable us to fetch blocks that we're unable to push, but that should
                // be a rare case (and correctly handled)
                low_block_num = reference_point_block_num;
            }
        }
        else
        {
            // block is a block we know about, but it is on a fork
            try
            {
                fork_history = handler.db.get_block_ids_on_fork(reference_point);
                // returns a vector where the last element is the common ancestor with the preferred chain,
                // and the first element is the reference point you passed in
                assert(fork_history.size() >= 2);

                if( fork_history.front() != reference_point )
                {
                    edump( (fork_history)(reference_point) );
                    assert(fork_history.front() == reference_point);
                }
                Chain::block_id_type last_non_fork_block = fork_history.back();
                fork_history.pop_back();  // remove the common ancestor
                boost::reverse(fork_history);

                if (last_non_fork_block == Chain::block_id_type()) // if the fork goes all the way back to genesis (does graphene's fork db allow this?)
                    non_fork_high_block_num = 0;
                else
                    non_fork_high_block_num = Chain::block_header::num_from_id(last_non_fork_block);

                high_block_num = non_fork_high_block_num + fork_history.size();
                assert(high_block_num == Chain::block_header::num_from_id(fork_history.back()));
            }
            catch (const fc::exception& e)
            {
                // unable to get fork history for some reason.  maybe not linked?
                // we can't return a synopsis of its chain
                elog("Unable to construct a blockchain synopsis for reference hash ${hash}: ${exception}", ("hash", reference_point)("exception", e));
                throw;
            }
            if (non_fork_high_block_num < low_block_num)
            {
                wlog("Unable to generate a usable synopsis because the peer we're generating it for forked too long ago "
                     "(our chains diverge after block #${non_fork_high_block_num} but only undoable to block #${low_block_num})",
                     ("low_block_num", low_block_num)
                     ("non_fork_high_block_num", non_fork_high_block_num));
                FC_THROW_EXCEPTION(Net::block_older_than_undo_history, "Peer is are on a fork I'm unable to switch to");
            }
        }
    }
    else
    {
        // no reference point specified, summarize the whole block chain
        high_block_num = handler.db.head_block_num();
        non_fork_high_block_num = high_block_num;
        if (high_block_num == 0)
            return synopsis; // we have no blocks
    }

    if( low_block_num == 0)
        low_block_num = 1;

    // at this point:
    // low_block_num is the block before the first block we can undo,
    // non_fork_high_block_num is the block before the fork (if the peer is on a fork, or otherwise it is the same as high_block_num)
    // high_block_num is the block number of the reference block, or the end of the chain if no reference provided

    // true_high_block_num is the ending block number after the network code appends any item ids it
    // knows about that we don't
    uint32_t true_high_block_num = high_block_num + number_of_blocks_after_reference_point;
    do
    {
        // for each block in the synopsis, figure out where to pull the block id from.
        // if it's <= non_fork_high_block_num, we grab it from the main blockchain;
        // if it's not, we pull it from the fork history
        if (low_block_num <= non_fork_high_block_num)
            synopsis.push_back(handler.db.get_block_id_for_num(low_block_num));
        else
            synopsis.push_back(fork_history[low_block_num - non_fork_high_block_num - 1]);
        low_block_num += (true_high_block_num - low_block_num + 2) / 2;
    }
    while (low_block_num <= high_block_num);

    return synopsis;
}

void P2pHandler::NodeInterface::sync_status(uint32_t, uint32_t) {
    // *shrug* Maybe use this eventually
}

void P2pHandler::NodeInterface::connection_count_changed(uint32_t) {
    // *shrug* Maybe use this eventually
}

uint32_t P2pHandler::NodeInterface::get_block_number(const Net::item_hash_t& block_id) {
    return Chain::block_header::num_from_id(block_id);
}

fc::time_point_sec P2pHandler::NodeInterface::get_block_time(const Net::item_hash_t& block_id) {
    auto found = handler.db.fetch_block_by_id(block_id);
    if (!found) return fc::time_point::min();
    return found->timestamp;
}

Net::item_hash_t P2pHandler::NodeInterface::get_head_block_id() const {
    return handler.db.head_block_id();
}

uint32_t P2pHandler::NodeInterface::estimate_last_known_fork_from_git_revision_timestamp(uint32_t) const {
    // Not sure what this is, but the standard node just returns zero
    return 0;
}

void P2pHandler::NodeInterface::error_encountered(const std::string& message, const fc::oexception& error) {
    edump((message)(error));
}

uint8_t P2pHandler::NodeInterface::get_current_block_interval_in_seconds() const {
    return handler.db.get_global_properties().parameters.block_interval;
}

void P2pHandler::syncFrom(Chain::block_id_type blockId) {
    node.sync_from(Net::item_id(Net::block_message_type, blockId), {});
    syncing = true;
}

void P2pHandler::connectToSeeds() {
    node.listen_to_p2p_network();
    node.connect_to_p2p_network();
    ilog("Node set up and listening on ${ep}", ("ep", node.get_actual_listening_endpoint()));
}
