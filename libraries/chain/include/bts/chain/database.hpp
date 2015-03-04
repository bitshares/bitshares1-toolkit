#pragma once
#include <bts/db/level_map.hpp>
#include <bts/db/level_pod_map.hpp>
#include <bts/chain/object.hpp>
#include <bts/chain/block.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/index.hpp>
#include <bts/chain/global_property_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/evaluator.hpp>
#include <map>
#include <boost/rational.hpp>

#include <fc/log/logger.hpp>

namespace bts { namespace chain {
   class account_index;
   class asset_index;

   typedef vector<std::pair<fc::static_variant<address, public_key_type>, share_type >> genesis_allocation;

   /**
    *  Undo State saves off the initial values associated
    *  with each object prior to making changes. When
    *  applying the undo_state the old_values should be
    *  restored in REVERSE ORDER to make sure that
    *  indexes can effeciently resize when the last
    *  element is removed.
    */
   struct undo_state
   {
       /** note: we could use clones of objects rather than packed objects as
        * a potentail performance optimization.  We should only have to serialize
        * when we go to disk.  This would make applying an "undo" operation much
        * faster because we can simply swap pointers.
        */
       map<object_id_type, packed_object>      old_values;
       vector<object_id_type>                  new_ids;
       vector<object_id_type>                  removed_ids;
       map<object_id_type, packed_object>      old_index_meta_objects;

       template<typename T>
       bool has_old_index_meta_object()const
       {
          return old_index_meta_objects.find( object_id_type(T::space_id,T::type_id,BTS_MAX_INSTANCE_ID) ) != old_index_meta_objects.end();
       }

       template<typename T>
       void set_old_index_meta_object(packed_object o)
       {
          old_index_meta_objects[ object_id_type(T::space_id,T::type_id,BTS_MAX_INSTANCE_ID) ] = std::move(o);
       }

   };

   struct fork_block : public enable_shared_from_this<fork_block>
   {
      shared_ptr<fork_block>           prev_block;
      vector< shared_ptr<fork_block> > next_blocks;
      signed_block                     block_data;
      optional<undo_state>             changes;
   };

   class database
   {
      public:
         database(bool validate_signatures = true);
         ~database();

         void open(const fc::path& data_dir, const genesis_allocation& initial_allocation = genesis_allocation());
         void reindex();
         void flush();
         void close();

         void push_undo_state();
         void pop_undo_state();
         void undo();

         void push_block( const signed_block& b );
         bool push_transaction( const signed_transaction& trx );

         asset current_delegate_registration_fee()const;

         template<typename T, typename F>
         const T* create( F&& constructor, object_id_type requested_id = object_id_type() )
         {
            undo_state& undo = _undo_state.back();
            auto& idx = get_index<T>();
            if( !undo.has_old_index_meta_object<T>() )
               undo.set_old_index_meta_object<T>( idx.get_meta_object() );

            const object* result = idx.create( [&](object* o)
            {
               assert( dynamic_cast<T*>(o) );
               constructor( static_cast<T*>(o) );
            }, requested_id );

            undo.new_ids.push_back(result->id);
            return static_cast<const T*>(result);
         }
         template<typename T, typename Lambda>
         void modify( const T* obj, const Lambda& m )
         {
            get_index<T>().modify( obj, m );
         }

         const object* get_object( object_id_type id )const;

         template<typename T>
         const T* get( object_id_type id )const
         {
            const object* obj = get_object( id );
            auto result       = dynamic_cast<const T*>(obj);
            return result;
         }

         template<uint8_t SpaceID, uint8_t TypeID, typename T>
         const T* get( object_id<SpaceID,TypeID,T> id )const { return get<T>(id); }

         template<typename IndexType>
         const IndexType* add_index()
         {
            typedef typename IndexType::object_type ObjectType;
            unique_ptr<index> indexptr( new IndexType(this) );
            assert(!_index[ObjectType::space_id][ObjectType::type_id]);
            _index[ObjectType::space_id][ObjectType::type_id] = std::move(indexptr);
            return static_cast<const IndexType*>(_index[ObjectType::space_id][ObjectType::type_id].get());
         }


         template<typename T>
         index& get_index() { return get_index(T::space_id,T::type_id); }

         template<typename T>
         const index& get_index()const { return get_index(T::space_id,T::type_id); }

         index& get_index(uint8_t space_id, uint8_t type_id);
         const index& get_index(uint8_t space_id, uint8_t type_id)const;

         const account_index&          get_account_index()const;
         const asset_index&            get_asset_index()const;

         account_index&                get_account_index();
         asset_index&                  get_asset_index();

         const asset_object*           get_base_asset()const;
         const global_property_object* get_global_properties()const;
         const fee_schedule_type&      current_fee_schedule()const;

         void init_genesis(const genesis_allocation& initial_allocation = genesis_allocation());

         template<typename EvaluatorType>
         void register_evaluator()
         {
            _operation_evaluators[
               operation::tag<typename EvaluatorType::operation_class_type>::value].reset( new op_evaluator_impl<EvaluatorType>() );
         }

      private:
         friend class base_primary_index;

         vector< unique_ptr<op_evaluator> >     _operation_evaluators;

         void save_undo( const object* obj );

         void                  apply_block(const signed_block& next_block, bool save_undo );
         processed_transaction apply_transaction(const signed_transaction& trx);

         void pop_pending_block();
         void push_pending_block();

         /** store the last 32 bits of block_hash[0]%1024 */
         vector<uint32_t>                                 _recent_block_prefixes;
         deque<undo_state>                                _undo_state;
         bool                                             _save_undo = true;
         bool                                             _validate_signatures;

         /**
          * @brief The signature_inhibitor struct disables signature validation on a database within a scope.
          *
          * Tasks which need to disable signature verification for a period of time may create a signature_inhibitor.
          * The inhibitor will back up the state of signature validation at its construction, and disable signature
          * validation while it is in scope. When it goes out of scope, it will restore the previous state of signature
          * validation.
          */
         struct signature_inhibitor {
            bool backup = false;
            database* db;

            signature_inhibitor(database* db)
               :db(db)
            {
               std::swap(backup, db->_validate_signatures);
            }
            ~signature_inhibitor()
            {
               std::swap(backup, db->_validate_signatures);
            }
         };

         // track recent forks...
         shared_ptr<fork_block>                 _oldest_fork_point;
         vector< shared_ptr<fork_block> >       _head_blocks;

         vector< vector< unique_ptr<index> > >  _index;
         block                                  _pending_block;

         /**
          *  Note: we can probably store blocks by block num rather than
          *  block id because after the undo window is past the block ID
          *  is no longer relevant and its number is irreversible.
          *
          *  During the "fork window" we can cache blocks in memory
          *  until the fork is resolved.  This should make maintaining
          *  the fork tree relatively simple.
          */
         bts::db::level_map<uint32_t, signed_block>                _block_num_to_block;
         bts::db::level_pod_map<block_id_type,uint32_t>            _block_id_to_num;
         shared_ptr<db::level_map<object_id_type, packed_object>>  _object_id_to_object;
   };

} }



FC_REFLECT( bts::chain::undo_state, (old_values)(new_ids)(removed_ids)(old_index_meta_objects) )




