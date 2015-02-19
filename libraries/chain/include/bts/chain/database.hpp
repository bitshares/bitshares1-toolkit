#pragma once
#include <bts/db/level_map.hpp>
#include <bts/db/level_pod_map.hpp>
#include <bts/chain/object.hpp>
#include <bts/chain/block.hpp>
#include <bts/chain/asset.hpp>
#include <map>

namespace bts { namespace chain {

   class object_builder
   {
      public:
         virtual ~object_builder(){}
         virtual unique_ptr<object> create()const = 0;
         virtual packed_object pack( const object* p )const = 0;
         virtual void unpack( object* p, const packed_object& obj )const = 0;
         virtual variant to_variant( const object* p )const = 0;
   };

   template<typename ObjectType>
   class object_builder_impl : public object_builder
   {
      public:
         virtual unique_ptr<object> create()const override
         {
            return unique_ptr<ObjectType>( new ObjectType() );
         }
         virtual packed_object pack( const object* p )const override
         {
            const auto& cast = *((const ObjectType*)p);
            return packed_object(cast);
         }
         virtual void unpack( object* p, const packed_object& obj )const override
         {
            FC_ASSERT( obj.type == ObjectType::type );
            auto& cast = *((ObjectType*)p);
            obj.unpack(cast); 
            FC_ASSERT( cast.type == ObjectType::type );
         }
         virtual variant to_variant( const object* p )const override
         {
            const auto& cast = *((const ObjectType*)p);
            return fc::variant(cast);
         }
   };

   /**
    *  Undo State saves off the initial values associated 
    *  with each object prior to making changes.
    */
   struct undo_state
   {
       object_id_type                         old_next_object_id;
       map<object_id_type, packed_object>     old_values;
       map<string,object_id_type >            old_account_index;
       map<string,object_id_type >            old_symbol_index;
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
         void index_account( account_object* account_obj );
         void index_symbol( asset_object* asset_obj );

         asset current_delegate_registration_fee()const;

         template<typename T>
         void register_object()
         {
            _object_factory[T::type] = unique_ptr< object_builder_impl<T> >( new object_builder_impl<T>() );
         }

         template<typename T>
         T* create()
         {
            ++_next_object_id;

            unique_ptr<T> obj( new T() );
            obj->id  = _next_object_id;

            _undo_state.back().old_values[obj->object_id()] = packed_object();
            auto r = obj.get();
            obj->mark_dirty();
            _loaded_objects[obj->object_id()] = std::move(obj);
            return r;
         }
         
         template<typename T>
         const T* get( object_id_type id )const
         {
            FC_ASSERT( id <= _loaded_objects.size() );
            const auto&  obj = _loaded_objects[id];
            auto result      = dynamic_cast<const T*>(obj.get());
            return result;
         }

         template<typename T>
         T* get_mutable( object_id_type id )
         { 
            FC_ASSERT( id <= _loaded_objects.size() );
            object* obj = _loaded_objects[id].get();
            auto result   = dynamic_cast<T*>(obj);
            if( result )
            {
               result->mark_dirty();
               save_undo( obj );
            }
            return result;
         } 

         const account_object* lookup_account( const string& name )const;
         const asset_object*   lookup_symbol( const string& symbol )const;

         unique_ptr<object>  load_object( const packed_object& obj );
      private:
         void init_genesis();
         void save_undo( object* obj );
         processed_transaction apply_transaction( const signed_transaction& trx );
         void pop_pending_block();
         void push_pending_block();

         object_id_type                           _next_object_id;
         deque<uint32_t>                          _recent_block_prefixes;
         deque<undo_state>                        _undo_state;

         shared_ptr<fork_block>                   _oldest_fork_point;
         vector< shared_ptr<fork_block> >         _head_blocks;
         vector< unique_ptr<object_builder> >     _object_factory;

         vector< unique_ptr<object> >             _loaded_objects;
         map<string, account_id_type >            _account_index;
         map<string, asset_id_type >              _symbol_index;
         vector< delegate_id_type >               _delegates;

         block                                    _pending_block;

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


FC_REFLECT( bts::chain::packed_object, (type)(data) )

FC_REFLECT( bts::chain::undo_state, 
            (old_next_object_id)
            (old_values)
            (old_account_index) 
            (old_symbol_index)
          )




