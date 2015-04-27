#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/block.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/global_property_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/fork_database.hpp>

#include <bts/db/object_database.hpp>
#include <bts/db/object.hpp>
#include <bts/db/level_map.hpp>
#include <bts/db/level_pod_map.hpp>
#include <bts/db/simple_index.hpp>
#include <fc/signals.hpp>

#include <fc/log/logger.hpp>

#include <map>

namespace bts { namespace chain {
   using bts::db::abstract_object;
   using bts::db::object;

   typedef vector<std::pair<fc::static_variant<address, public_key_type>, share_type >> genesis_allocation;

   /**
    *   @class database
    *   @brief tracks the blockchain state in an extensible manner
    */
   class database : public object_database
   {
      public:
         database();
         ~database();

         enum validation_steps
         {
            skip_nothing                = 0x00,
            skip_delegate_signature     = 0x01, ///< used while reindexing
            skip_transaction_signatures = 0x02, ///< used by non delegate nodes
            skip_undo_block             = 0x04, ///< used while reindexing
            skip_undo_transaction       = 0x08, ///< used while applying block
            skip_transaction_dupe_check = 0x10, ///< used while reindexing
            skip_fork_db                = 0x20, ///< used while reindexing
            skip_block_size_check       = 0x40, ///< used when applying locally generated transactions
            skip_tapos_check            = 0x80, ///< used while reindexing -- note this skips expiration check as well
         };

         void open(const fc::path& data_dir, const genesis_allocation& initial_allocation = genesis_allocation());
         void reindex(fc::path data_dir, genesis_allocation initial_allocation = genesis_allocation());

         /**
          * @brief wipe Delete database from disk, and potentially the raw chain as well.
          * @param include_blocks If true, delete the raw chain as well as the database.
          *
          * Will close the database before wiping. Database will be closed when this function returns.
          */
         void wipe(bool include_blocks);
         void close(uint32_t blocks_to_rewind = 0);

         /**
          *  @return true if the block is in our fork DB or saved to disk as
          *  part of the official chain, otherwise return false
          */
         bool                       is_known_block( const block_id_type& id )const;
         bool                       is_known_transaction( const transaction_id_type& id )const;
         block_id_type              get_block_id_for_num( uint32_t block_num )const;
         optional<signed_block>     fetch_block_by_id( const block_id_type& id )const;
         optional<signed_block>     fetch_block_by_number( uint32_t num )const;
         const signed_transaction&  get_recent_transaction( const transaction_id_type& trx_id )const;

         bool push_block( const signed_block& b, uint32_t skip = skip_nothing );
         processed_transaction push_transaction( const signed_transaction& trx, uint32_t skip = skip_nothing );
         ///@throws fc::exception if the proposed transaction fails to apply.
         processed_transaction push_proposal( const proposal_object& proposal );

         time_point   get_next_generation_time(witness_id_type del_id )const;
         std::pair<fc::time_point, witness_id_type> get_next_generation_time(const set<witness_id_type>& witnesses )const;
         signed_block generate_block(const fc::ecc::private_key& delegate_key,
                                      witness_id_type del_id, uint32_t skip = 0 );

         asset current_delegate_registration_fee()const;

         const asset_object&                    get_core_asset()const;
         const global_property_object&          get_global_properties()const;
         const dynamic_global_property_object&  get_dynamic_global_properties()const;
         const fee_schedule_type&               current_fee_schedule()const;

         time_point_sec head_block_time()const;
         uint32_t       head_block_num()const;
         block_id_type  head_block_id()const;

         void initialize_evaluators();
         /// Reset the object graph in-memory
         void initialize_indexes();
         void init_genesis(const genesis_allocation& initial_allocation = genesis_allocation());

         template<typename EvaluatorType>
         void register_evaluator()
         {
            _operation_evaluators[
               operation::tag<typename EvaluatorType::operation_type>::value].reset( new op_evaluator_impl<EvaluatorType>() );
         }

         template<typename EvaluatorType>
         void register_evaluation_observer( evaluation_observer& observer )
         {
            unique_ptr<op_evaluator>& op_eval = _operation_evaluators[operation::tag<typename EvaluatorType::operation_type>::value];
            op_eval->eval_observers.push_back( &observer );
            return;
         }

         void pop_block();
         void clear_pending();

         /**
          *  This method is used to track appied operations during the evaluation of a block, these
          *  operations should include any operation actually included in a transaction as well
          *  as any implied/virtual operations that resulted, such as filling an order.  The
          *  applied operations is cleared after applying each block and calling the block
          *  observers which may want to index these operations.
          *
          *  @return the op_id which can be used to set the result after it has finished being applied.
          */
         uint32_t  push_applied_operation( const operation& op );
         void      set_applied_operation_result( uint32_t op_id, const operation_result& r );
         const vector<operation_history_object>& get_applied_operations()const;

         /**
          *  This signal is emitted after all operations and virtual operation for a
          *  block have been applied but before the get_applied_operations() are cleared.
          *
          *  You may not yield from this callback because the blockchain is holding
          *  the write lock and may be in an "inconstant state" until after it is
          *  released.
          */
         fc::signal<void(const signed_block&)> applied_block;

         void debug_dump();

         /**
          * @{
          * @group High Level Database Queries
          *
          * These methods implement higher-level operations on the database, which involve constructs such as accounts,
          * balances, and other semantic interpretations of the data in the database.
          */

         /**
          * @brief Retrieve a particular account's balance in a given asset
          * @param owner Account whose balance should be retrieved
          * @param asset_id ID of the asset to get balance in
          * @return owner's balance in asset
          */
         asset get_balance(account_id_type owner, asset_id_type asset_id)const;
         /// This is an overloaded method.
         asset get_balance(const account_object& owner, const asset_object& asset_obj)const;
         /// This is an overloaded method.
         asset get_balance(const account_object* owner, const asset_object* asset_obj)const
         { return get_balance(*owner, *asset_obj); }
         /**
          * @brief Adjust a particular account's balance in a given asset by a delta
          * @param account ID of account whose balance should be adjusted
          * @param delta Asset ID and amount to adjust balance by
          */
         void adjust_balance(account_id_type account, asset delta);
         /// This is an overloaded method.
         void adjust_balance(const account_object& account, asset delta);
         /// This is an overloaded method.
         void adjust_balance(const account_object* account, asset delta) { adjust_balance(*account, delta); }

         /// @{ @group Market Helpers
         void settle_black_swan( const asset_object& bitasset, const price& settle_price );
         void cancel_order( const limit_order_object& order, bool create_virtual_op = true );

         /**
          *  Matches the two orders,
          *
          *  @return a bit field indicating which orders were filled (and thus removed)
          *
          *  0 - no orders were matched
          *  1 - bid was filled
          *  2 - ask was filled
          *  3 - both were filled
          */
         ///@{
         template<typename OrderType>
         int match( const limit_order_object& bid, const OrderType& ask, const price& match_price );
         int match( const limit_order_object& bid, const limit_order_object& ask, const price& trade_price );
         int match( const limit_order_object& bid, const short_order_object& ask, const price& trade_price );
         int match( const call_order_object& ask, const limit_order_object& );
         int match( const call_order_object& call, const force_settlement_object& settle , const price& match_price );
         int match( const call_order_object& ask, const short_order_object& );
         ///@}

         /**
          * @return true if the order was completely filled and thus freed.
          */
         bool fill_order( const limit_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const short_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const call_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const force_settlement_object& settle, const asset& pays, const asset& receives );

         bool convert_fees( const asset_object& mia );
         bool check_call_orders( const asset_object& mia );

         // helpers to fill_order
         void pay_order( const account_object& receiver, const asset& receives, const asset& pays );
         asset pay_market_fees( const asset_object& recv_asset, const asset& receives );

         asset calculate_market_fee( const asset_object& aobj, const asset& trade_amount );

         ///@}

         /**
          * @}
          */
   protected:
         //Mark pop_undo() as protected -- we do not want outside calling pop_undo(); it should call pop_block() instead
         void pop_undo() { object_database::pop_undo(); }

      private:
         optional<undo_database::session>       _pending_block_session;
         vector< unique_ptr<op_evaluator> >     _operation_evaluators;

         template<class Content>
         void shuffle_vector(vector<Content>& ids);
         template<class ObjectType>
         vector<object_id<ObjectType::space_id, ObjectType::type_id, ObjectType>> sort_votable_objects()const;

         void                  apply_block( const signed_block& next_block, uint32_t skip = skip_nothing );
         processed_transaction apply_transaction( const signed_transaction& trx, uint32_t skip = skip_nothing );
         operation_result      apply_operation( transaction_evaluation_state& eval_state, const operation& op );

         ///Steps involved in applying a new block
         ///@{
         const witness_object& validate_block_header(uint32_t skip, const signed_block& next_block);
         void update_global_dynamic_data( const signed_block& b );
         void update_signing_witness(const witness_object& signing_witness, const signed_block& new_block);
         void update_pending_block(const signed_block& next_block, uint8_t current_block_interval);
         ///Steps performed only at maintenance intervals
         ///@{
         void update_active_witnesses();
         void update_active_delegates();
         void update_vote_totals();
         void perform_chain_maintenance(const signed_block& next_block, const global_property_object& global_props);
         ///@}
         void create_block_summary(const signed_block& next_block);
         void clear_expired_transactions();
         void clear_expired_proposals();
         void clear_expired_orders();
         ///@}

         signed_block                           _pending_block;
         fork_database                          _fork_db;

         /**
          *  Note: we can probably store blocks by block num rather than
          *  block id because after the undo window is past the block ID
          *  is no longer relevant and its number is irreversible.
          *
          *  During the "fork window" we can cache blocks in memory
          *  until the fork is resolved.  This should make maintaining
          *  the fork tree relatively simple.
          */
         bts::db::level_map<block_id_type, signed_block>   _block_id_to_block;

         /**
          * Contains the set of ops that are in the process of being applied from
          * the current block.  It contains real and virtual operations in the
          * order they occur and is cleared after the applied_block signal is
          * emited.
          */
         vector<operation_history_object>  _applied_ops;
         uint32_t                          _current_block_num    = 0;
         uint16_t                          _current_trx_in_block = 0;
         uint16_t                          _current_op_in_trx    = 0;
         uint16_t                          _current_virtual_op   = 0;
   };

   template<class Content>
   void database::shuffle_vector(vector<Content>& ids)
   {
      auto randvalue = dynamic_global_property_id_type()(*this).random;
      for( uint32_t i = 0; i < ids.size(); ++i )
      {
         const auto rands_per_hash = sizeof(secret_hash_type) / sizeof(randvalue._hash[0]);
         std::swap( ids[i], ids[ i + (randvalue._hash[i%rands_per_hash] % (ids.size()-i))] );
         if( i % rands_per_hash == (rands_per_hash-1) )
            randvalue = secret_hash_type::hash( randvalue );
      }
   }

   template<class ObjectType>
   vector<object_id<ObjectType::space_id, ObjectType::type_id, ObjectType>> database::sort_votable_objects() const
   {
      using ObjectIdType = object_id<ObjectType::space_id, ObjectType::type_id, ObjectType>;
      const auto& all_objects = dynamic_cast<const simple_index<ObjectType>&>(get_index<ObjectType>());
      vector<ObjectIdType> ids;
      ids.reserve(all_objects.size());
      std::transform(all_objects.begin(), all_objects.end(), std::back_inserter(ids), [](const ObjectType& w) {
         return w.id;
      });
      std::sort( ids.begin(), ids.end(), [&]( ObjectIdType a, ObjectIdType b )->bool {
         return a(*this).vote(*this).total_votes >
               b(*this).vote(*this).total_votes;
      });

      uint64_t base_threshold = ids[9](*this).vote(*this).total_votes.value;
      uint64_t threshold =  base_threshold * 75 / 100;
      uint32_t i = 10;

      if( threshold > 0 )
         for( ; i < ids.size(); ++i )
         {
            if( ids[i](*this).vote(*this).total_votes < threshold ) break;
            threshold = (base_threshold / (100) ) * (75 + i/(ids.size()/4));
         }
      ids.resize( i );

      return ids;
   }

} }
