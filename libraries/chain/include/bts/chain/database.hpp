#pragma once
#include <bts/db/level_map.hpp>
#include <bts/db/level_pod_map.hpp>
#include <bts/chain/object.hpp>
#include <bts/chain/block.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/index.hpp>
#include <map>

namespace bts { namespace chain {
   class account_index;
   class asset_index;

   /**
    *  Undo State saves off the initial values associated 
    *  with each object prior to making changes.
    */
   struct undo_state
   {
       // [space][type] = old_instance
       flat_map<pair<int,int>, object_id_type> old_next_object_ids;
       map<object_id_type, packed_object>      old_values;
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
         database();
         ~database();

         void open( const fc::path& data_dir );
         void flush();
         void close();

         void push_undo_state();
         void pop_undo_state();
         void undo();

         void push_block( const block& b );
         bool push_transaction( const signed_transaction& trx );

         asset current_delegate_registration_fee()const;

         template<typename T, typename F>
         T* create( F&& constructor )
         {
            undo_state& undo = _undo_state.back();
            /*
            auto old_next_ids_itr = undo.old_next_object_ids.find( make_pair<int,int>( T::space_id, T::type_id ) );
            if( old_next_ids_itr == undo.old_next_object_ids.end() )
               undo.old_next_object_ids[ make_pair<int,int>(T::space_id,T::type_id) ] = _next_object_ids[T::space_id][T::type_id];
            */
            auto& idx = get_index<T>();

            unique_ptr<T> obj( new T() );
            obj->id  = object_id_type( T::space_id, T::type_id, idx.size() );
            _undo_state.back().old_values[obj->id] = packed_object();
               
            auto r = obj.get();
            constructor(r);
            get_index<T>().add(std::move(obj));
            return r;
         }

         const object* get_object( object_id_type id )const;

         template<typename T>
         const T* get( object_id_type id )const
         {
            const object* obj = get_object( id );
            auto result       = dynamic_cast<const T*>(obj);
            return result;
         }

         template<typename IndexType>
         const IndexType* add_index()
         {
            typedef typename IndexType::object_type ObjectType;
            unique_ptr<index> indexptr( new IndexType(this) );
            _index[ObjectType::space_id][ObjectType::type_id] = std::move(indexptr);
            return static_cast<const IndexType*>(_index[ObjectType::space_id][ObjectType::type_id].get());
         }


         template<typename T>
         index& get_index() { return get_index(T::space_id,T::type_id); }

         template<typename T>
         const index& get_index()const { return get_index(T::space_id,T::type_id); }

         index& get_index(uint8_t space_id, uint8_t type_id);
         const index& get_index(uint8_t space_id, uint8_t type_id)const;

         const account_index& get_account_index()const;
         const asset_index&   get_asset_index()const;

         account_index&       get_account_index();
         asset_index&         get_asset_index();

      private:
         friend class base_primary_index;

         void init_genesis();
         void save_undo( const object* obj );
         processed_transaction apply_transaction( const signed_transaction& trx );
         void pop_pending_block();
         void push_pending_block();

         /** store the last 32 bits of block_hash[0]%1024 */
         vector<uint32_t>                                 _recent_block_prefixes;
         deque<undo_state>                                _undo_state;
                                                          
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
          *  Durring the "fork window" we can cache blocks in memory
          *  until the fork is resolved.  This should make maintaining
          *  the fork tree relatively simple.
          */
         bts::db::level_map<uint32_t, signed_block>         _block_num_to_block;
         bts::db::level_pod_map<block_id_type,uint32_t>     _block_id_to_num;
         bts::db::level_map<object_id_type, packed_object>  _object_id_to_object;
   };

} }



FC_REFLECT( bts::chain::undo_state, 
            (old_next_object_ids)
            (old_values)
          )




