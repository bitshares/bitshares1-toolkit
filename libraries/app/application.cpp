#include <bts/app/application.hpp>

#include <bts/net/core_messages.hpp>

#include <bts/chain/time.hpp>

namespace bts { namespace app {
using net::item_hash_t;
using net::item_id;
using net::message;
using net::block_message;
using net::trx_message;

using chain::block;
using chain::block_id_type;

using std::vector;

namespace detail {

   class application_impl : public net::node_delegate
   {
      public:

      application_impl(fc::path data_dir)
         : _data_dir(data_dir),
           _chain_db(std::make_shared<chain::database>()),
           _p2p_network(std::make_shared<net::node>("Graphene Reference Implementation"))
      {
         _chain_db->open(data_dir / "blockchain");
         _p2p_network->load_configuration(data_dir / "p2p");

         _p2p_network->set_node_delegate(this);
      }

      void configure(const application::config& cfg)
      { try {
         for( const fc::ip::endpoint& node : cfg.seed_nodes )
         {
            ilog("Adding seed node ${ip}", ("ip", node));
            _p2p_network->add_node(node);
            _p2p_network->connect_to_endpoint(node);
         }

         _p2p_network->listen_on_port(cfg.p2p_endpoint.port(), true);
         _p2p_network->listen_to_p2p_network();
         ilog("Configured p2p node to listen on ${ip}", ("ip", _p2p_network->get_actual_listening_endpoint()));

         _p2p_network->connect_to_p2p_network();
         _p2p_network->sync_from(net::item_id(net::core_message_type_enum::block_message_type,
                                              _chain_db->head_block_id()),
                                 std::vector<uint32_t>());
      } FC_CAPTURE_AND_RETHROW( (cfg) ) }

      /**
       *  If delegate has the item, the network has no need to fetch it.
       */
      virtual bool has_item( const net::item_id& id ) override
      { try {
          if( id.item_type == bts::net::block_message_type )
          {
             return _chain_db->is_known_block( id.item_hash );
          }
          else
          {
             return _chain_db->is_known_transaction( id.item_hash );
          }
      } FC_CAPTURE_AND_RETHROW( (id) ) }

      /**
       *  @brief allows the application to validate an item prior to
       *         broadcasting to peers.
       *
       *  @param sync_mode true if the message was fetched through the sync process, false during normal operation
       *  @returns true if this message caused the blockchain to switch forks, false if it did not
       *
       *  @throws exception if error validating the item, otherwise the item is
       *          safe to broadcast on.
       */
      virtual bool handle_block( const bts::net::block_message& blk_msg, bool sync_mode ) override
      { try {
         ilog("Got block from network");
         return _chain_db->push_block( blk_msg.block );
      } FC_CAPTURE_AND_RETHROW( (blk_msg)(sync_mode) ) }

      virtual bool handle_transaction( const bts::net::trx_message& trx_msg, bool sync_mode ) override
      { try {
         ilog("Got transaction from network");
         _chain_db->push_transaction( trx_msg.trx );
         return false;
      } FC_CAPTURE_AND_RETHROW( (trx_msg)(sync_mode) ) }

      /**
       *  Assuming all data elements are ordered in some way, this method should
       *  return up to limit ids that occur *after* the last ID in synopsis that
       *  we recognize.
       *
       *  On return, remaining_item_count will be set to the number of items
       *  in our blockchain after the last item returned in the result,
       *  or 0 if the result contains the last item in the blockchain
       */
      virtual std::vector<item_hash_t> get_item_ids(uint32_t item_type,
                                                    const std::vector<item_hash_t>& blockchain_synopsis,
                                                    uint32_t& remaining_item_count,
                                                    uint32_t limit ) override
      { try {
         FC_ASSERT( item_type == bts::net::block_message_type );
         vector<block_id_type>  result;
         result.reserve(limit);

         auto head_block_num = _chain_db->head_block_num();
         auto itr = blockchain_synopsis.rbegin();
         while( itr != blockchain_synopsis.rend() )
         {
            if( _chain_db->is_known_block( *itr ) )
            {
               result.push_back( *itr );
               uint32_t block_num = block::num_from_id(*itr);
               while( result.size() < limit && block_num <= head_block_num )
               {
                  result.push_back( _chain_db->get_block_id_for_num( block_num ) );
                  ++block_num;
               }
               remaining_item_count = head_block_num - block_num;
               return result;
            }
            ++itr;
         }
         remaining_item_count = head_block_num;
         idump((blockchain_synopsis)(limit)(result)(remaining_item_count));
         return result;
      } FC_CAPTURE_AND_RETHROW( (blockchain_synopsis)(remaining_item_count)(limit) ) }

      /**
       *  Given the hash of the requested data, fetch the body.
       */
      virtual message get_item( const item_id& id ) override
      { try {
         if( id.item_type == bts::net::block_message_type )
         {
            auto opt_block = _chain_db->fetch_block_by_id( id.item_hash );
            FC_ASSERT( opt_block.valid() );
            return block_message( std::move(*opt_block) );
         }
         return trx_message( _chain_db->get_recent_transaction( id.item_hash ) );
      } FC_CAPTURE_AND_RETHROW( (id) ) }

      virtual fc::sha256 get_chain_id()const override
      {
         return fc::sha256();
      }

      /**
       * Returns a synopsis of the blockchain used for syncing.
       * This consists of a list of selected item hashes from our current preferred
       * blockchain, exponentially falling off into the past.  Horrible explanation.
       *
       * If the blockchain is empty, it will return the empty list.
       * If the blockchain has one block, it will return a list containing just that block.
       * If it contains more than one block:
       *   the first element in the list will be the hash of the genesis block
       *   the second element will be the hash of an item at the half way point in the blockchain
       *   the third will be ~3/4 of the way through the block chain
       *   the fourth will be at ~7/8...
       *     &c.
       *   the last item in the list will be the hash of the most recent block on our preferred chain
       */
      virtual std::vector<item_hash_t> get_blockchain_synopsis( uint32_t item_type,
                                                                const bts::net::item_hash_t& reference_point,
                                                                uint32_t number_of_blocks_after_reference_point ) override
      { try {
         std::vector<item_hash_t> result;
         result.reserve(30);
         auto head_block_num = _chain_db->head_block_num();
         result.push_back( _chain_db->head_block_id() );
         auto current = 1;
         while( current < head_block_num )
         {
            result.push_back( _chain_db->get_block_id_for_num( head_block_num - current ) );
            current = current*2;
         }
         if( result.empty() || result.back() != block_id_type() )
            result.push_back(block_id_type());
         std::reverse( result.begin(), result.end() );
         idump((reference_point)(number_of_blocks_after_reference_point)(result));
         return result;
      } FC_CAPTURE_AND_RETHROW( (reference_point)(number_of_blocks_after_reference_point) ) }

      /**
       *  Call this after the call to handle_message succeeds.
       *
       *  @param item_type the type of the item we're synchronizing, will be the same as item passed to the sync_from() call
       *  @param item_count the number of items known to the node that haven't been sent to handle_item() yet.
       *                    After `item_count` more calls to handle_item(), the node will be in sync
       */
      virtual void     sync_status( uint32_t item_type, uint32_t item_count ) override
      {
         // any status reports to GUI go here
      }

      /**
       *  Call any time the number of connected peers changes.
       */
      virtual void     connection_count_changed( uint32_t c ) override
      {
        // any status reports to GUI go here
      }

      virtual uint32_t get_block_number(const item_hash_t& block_id) override
      { try {
         return block::num_from_id(block_id);
      } FC_CAPTURE_AND_RETHROW( (block_id) ) }

      /**
       * Returns the time a block was produced (if block_id = 0, returns genesis time).
       * If we don't know about the block, returns time_point_sec::min()
       */
      virtual fc::time_point_sec get_block_time(const item_hash_t& block_id) override
      { try {
         auto opt_block = _chain_db->fetch_block_by_id( block_id );
         if( opt_block.valid() ) return opt_block->timestamp;
         return fc::time_point_sec::min();
      } FC_CAPTURE_AND_RETHROW( (block_id) ) }

      /** returns bts::blockchain::now() */
      virtual fc::time_point_sec get_blockchain_now() override
      {
         return bts::chain::now();
      }

      virtual item_hash_t get_head_block_id() const override
      {
         return _chain_db->head_block_id();
      }

      virtual uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const override
      {
         return 0; // there are no forks in graphene
      }

      virtual void error_encountered(const std::string& message, const fc::oexception& error) override
      {
         // notify GUI or something cool
      }

      fc::path _data_dir;

      std::shared_ptr<bts::chain::database> _chain_db;
      std::shared_ptr<bts::net::node>       _p2p_network;
   };

}

application::application(fc::path data_dir)
   : my(new detail::application_impl(data_dir))
{}

void application::configure(const application::config& cfg)
{
   my->configure(cfg);
}

net::node_ptr application::p2p_node()
{
   return my->_p2p_network;
}

std::shared_ptr<chain::database> application::chain_database() const
{
   return my->_chain_db;
}

// namespace detail
} }
