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
         virtual uint8_t   object_space_id()const = 0;
         virtual uint8_t   object_type_id()const  = 0;

         virtual unique_ptr<object> create()const = 0;
         virtual packed_object pack( const object* p )const = 0;
         virtual void unpack( object* p, const packed_object& obj )const = 0;
         virtual variant to_variant( const object* p )const = 0;
   };

   template<typename ObjectType>
   class object_builder_impl : public object_builder
   {
      public:
         virtual uint8_t   object_space_id()const override { return ObjectType::space_id; }
         virtual uint8_t   object_type_id()const override  { return ObjectType::type_id; }

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
            auto& cast = *((ObjectType*)p);
            obj.unpack(cast); 
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
         void index_account( account_object* account_obj );
         void index_symbol( asset_object* asset_obj );

         asset current_delegate_registration_fee()const;

         template<typename T>
         void register_object()
         {
            auto id_space      = T::space_id;
            auto type_in_space = T::type_id;
            if( _object_factory.size() <= id_space ) 
               _object_factory.resize( id_space + 1 );
            if( _object_factory[id_space].size() <= type_in_space )
               _object_factory[id_space].resize( type_in_space+1 );
            _object_factory[id_space][type_in_space] = unique_ptr< object_builder_impl<T> >( new object_builder_impl<T>() );
         }

         template<typename T>
         T* create()
         {
            undo_state& undo = _undo_state.back();
            auto old_next_ids_itr = undo.old_next_object_ids.find( make_pair<int,int>( T::space_id, T::type_id ) );
            if( old_next_ids_itr == undo.old_next_object_ids.end() )
               undo.old_next_object_ids[ make_pair<int,int>(T::space_id,T::type_id) ] = _next_object_ids[T::space_id][T::type_id];

            unique_ptr<T> obj( new T() );
            obj->id  = object_id_type( T::space_id, T::type_id, ++_next_object_ids[T::space_id][T::type_id] );
            obj->mark_dirty();

            _undo_state.back().old_values[obj->id] = packed_object();

            if( _loaded_objects.size() <= T::space_id )
               _loaded_objects.resize( T::space_id + 1 );
            if( _loaded_objects[T::space_id].size() <= T::type_id )
               _loaded_objects[T::space_id].resize( T::type_id + 1 );
            if( _loaded_objects[T::space_id][T::type_id].size() <= obj->id.instance() )
               _loaded_objects[T::space_id][T::type_id].resize( obj->id.instance() + 1 );
               
            auto r = obj.get();
            _loaded_objects[T::space_id][T::type_id][obj->id.instance()] = std::move(obj);
            return r;
         }

         const object* get_object( object_id_type id )const
         {
            if( id.is_null() ) return nullptr;
            return get_object_ptr( id ).get();
         }

         object* get_mutable_object( object_id_type id )
         {
            if( id.is_null()) return nullptr;
            const auto& obj = get_object_ptr( id );
            save_undo(obj.get());
            return obj.get();
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

         const unique_ptr<object>& get_object_ptr( object_id_type id )const
         {
            FC_ASSERT( id.space()    < _loaded_objects.size() );
            FC_ASSERT( id.type()     < _loaded_objects[id.space()].size() );
            FC_ASSERT( id.instance() < _loaded_objects[id.space()][id.type()].size() );
            return _loaded_objects[id.space()][id.type()][id.instance()];
         }

         unique_ptr<object>& get_object_ptr( object_id_type id )
         {
            FC_ASSERT( id.space()    < _loaded_objects.size() );
            FC_ASSERT( id.type()     < _loaded_objects[id.space()].size() );
            FC_ASSERT( id.instance() < _loaded_objects[id.space()][id.type()].size() );
            return _loaded_objects[id.space()][id.type()][id.instance()];
         }

         /** 
          *  Stores object IDs as _next_object_ids[SPACE_ID][TYPE_ID] = NEXT_INSTANCE_ID 
          **/
         vector< vector< object_id_type > >               _next_object_ids; 

         /** store the last 32 bits of block_hash[0]%1024 */
         vector<uint32_t>                                 _recent_block_prefixes;

         deque<undo_state>                                _undo_state;
                                                          
         // track recent forks...
         shared_ptr<fork_block>                           _oldest_fork_point;
         vector< shared_ptr<fork_block> >                 _head_blocks;

         /**
          *  Object types are first filted by the upper 8 bits of the type
          *  which identifies which "space_id" the object belongs to and
          *  then indexed by the lower 8 bits of the type which identifies
          *  the type within the space.   This allows for up to 255 objects
          *  types per "space_id" and 255 "space_ids" which should be
          *  sufficient for most applications, but could easily be
          *  expanded upon for special cases.
          */
         vector< vector< unique_ptr<object_builder> >  >    _object_factory;
         object_builder* get_object_builder( uint8_t space, uint8_t type )const;

         vector< vector< vector< unique_ptr<object> > >  >   _loaded_objects;
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



FC_REFLECT( bts::chain::undo_state, 
            (old_next_object_ids)
            (old_values)
          )




