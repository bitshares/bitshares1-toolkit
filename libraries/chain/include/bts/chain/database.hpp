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
         virtual uint16_t           object_id_space()const { return ObjectType::id_space; }
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
       vector<object_id_type>                 old_next_object_ids;
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
            auto id_space = uint16_t(T::type)>>8;
            auto type_in_space = uint16_t(T::type)&0x00ff;
            if( _object_factory.size() <= id_space ) 
               _object_factory.resize( id_space + 1 );
            if( _object_factory[id_space].size() <= type_in_space )
               _object_factory[id_space].resize( type_in_space+1 );
            _object_factory[id_space][type_in_space] = unique_ptr< object_builder_impl<T> >( new object_builder_impl<T>() );
         }

         template<typename T>
         T* create()
         {
            if( _next_object_ids.size() <= T::id_space )
               _next_object_ids.resize( T::id_space + 1 );
            auto local_obj_id = ++_next_object_ids[T::id_space];

            unique_ptr<T> obj( new T() );
            obj->id  = _next_object_ids[T::id_space] | (uint64_t(T::id_space) << 48);

            _undo_state.back().old_values[obj->object_id()] = packed_object();
            auto r = obj.get();
            obj->mark_dirty();

            if( _loaded_objects.size() <= T::id_space )
               _loaded_objects.resize( T::id_space + 1 );
            if( _loaded_objects[T::id_space].size() <= local_obj_id )
               _loaded_objects[T::id_space].resize( local_obj_id + 1 );
               
            _loaded_objects[T::id_space][local_obj_id] = std::move(obj);
            return r;
         }

         const object* get_object( object_id_type id )const
         {
            if( id == 0 ) return nullptr;
            return _loaded_objects[id>>48][id&0x0000ffffffffffff].get();
         }

         object* get_mutable_object( object_id_type id )
         {
            if( id == 0 ) return nullptr;
            auto obj = _loaded_objects[id>>48][id&0x0000ffffffffffff].get();
            save_undo(obj);
            return obj;
         }
         
         template<typename T>
         const T* get( object_id_type id )const
         {
            const object* obj = get_object( id );
            auto result      = dynamic_cast<const T*>(obj);
            return result;
         }

         template<typename T>
         T* get_mutable( object_id_type id )
         { 
            return  dynamic_cast<T*>(get_mutable_object(id));
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

         unique_ptr<object>& get_object_ptr( object_id_type id )
         {
            return _loaded_objects[id>>48][id&0x0000ffffffffffff];
         }

         /** index in vector indicates which id space */
         vector<object_id_type>                           _next_object_ids; 
         deque<uint32_t>                                  _recent_block_prefixes;
         deque<undo_state>                                _undo_state;
                                                          
         shared_ptr<fork_block>                           _oldest_fork_point;
         vector< shared_ptr<fork_block> >                 _head_blocks;
         /**
          *  Object types are first filted by the upper 8 bits of the type
          *  which identifies which "id_space" the object belongs to and
          *  then indexed by the lower 8 bits of the type which identifies
          *  the type within the space.   This allows for up to 255 objects
          *  types per "id_space" and 255 "id_spaces" which should be
          *  sufficient for most applications, but could easily be
          *  expanded upon for special cases.
          */
         vector< vector< unique_ptr<object_builder> >  >  _object_factory;
         object_builder* get_object_builder( uint16_t type )const;

         vector< vector< unique_ptr<object> > >   _loaded_objects;
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
            (old_next_object_ids)
            (old_values)
            (old_account_index) 
            (old_symbol_index)
          )




