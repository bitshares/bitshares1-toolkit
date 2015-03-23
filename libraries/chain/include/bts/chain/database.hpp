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
          * Saves the complete state of the database to disk, this could take a while
          */
         void flush();

         /**
          * @brief wipe Delete database from disk, and potentially the raw chain as well.
          * @param include_blocks If true, delete the raw chain as well as the database.
          *
          * Will close the database before wiping. Database will be closed when this function returns.
          */
         void wipe(bool include_blocks);
         void close();

         optional<signed_block> fetch_block_by_id( const block_id_type& id )const;
         optional<signed_block> fetch_block_by_number( uint32_t num )const;

         void push_block( const signed_block& b, uint32_t skip = skip_nothing );
         processed_transaction push_transaction( const signed_transaction& trx, uint32_t skip = skip_nothing );

         time_point   get_next_generation_time( delegate_id_type del_id )const;
         signed_block generate_block( const fc::ecc::private_key& delegate_key,
                                      delegate_id_type del_id, uint32_t skip = 0 );

         asset current_delegate_registration_fee()const;

         const asset_object&           get_core_asset()const;
         const global_property_object& get_global_properties()const;
         const fee_schedule_type&      current_fee_schedule()const;

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

         void pop_block();
         void clear_pending();

      protected:
         //Mark pop_undo() as protected -- we do not want outside calling pop_undo(); it should call pop_block() instead
         void pop_undo();

      private:
         optional<undo_database::session>       _pending_block_session;
         vector< unique_ptr<op_evaluator> >     _operation_evaluators;

         void update_global_dynamic_data( const signed_block& b );
         void update_active_delegates();
         void update_global_properties();

         void                  apply_block( const signed_block& next_block, uint32_t skip = skip_nothing );
         processed_transaction apply_transaction( const signed_transaction& trx, uint32_t skip = skip_nothing );

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
         bts::db::level_map<block_id_type, signed_block>           _block_id_to_block;
   };

} }

