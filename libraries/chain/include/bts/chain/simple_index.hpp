#pragma once
#include <bts/chain/index.hpp>

namespace bts { namespace chain {

   template<typename T>
   class simple_index : public index
   {
      public:
         typedef T object_type;

         virtual object_id_type get_next_available_id()const override
         {
            return object_id_type( T::space_id, T::type_id, size());
         }

         virtual packed_object  get_meta_object()const override
         {
            return packed_object( index_meta_object( get_next_available_id().instance() ) );
         }
         virtual void           set_meta_object( const packed_object& obj ) override
         {
            index_meta_object meta;
            obj.unpack(meta);
            _objects.resize( meta.next_object_instance );
         }

         virtual const object*  create( const std::function<void(object*)>& constructor,
                                        object_id_type /*requested_id*/ ) override
         {
             auto next_id = get_next_available_id();
             auto instance = next_id.instance();
             if( instance >= _objects.size() ) _objects.resize( instance + 1 );
             _objects[instance].reset(new T);
             auto result = _objects[instance].get();
             result->id = next_id;
             constructor( result );
             return result;
         }

         virtual int64_t size()const { return _objects.size(); }

         virtual void modify( const object* obj, const std::function<void(object*)>& modify_callback ) override
         {
            assert( obj != nullptr );
            assert( obj->id.instance() < _objects.size() );
            modify_callback( _objects[obj->id.instance()].get());
         }

         virtual void add( unique_ptr<object> o )override
         {
             assert( o );
             assert( o->id.space() == T::space_id );
             assert( o->id.type() == T::type_id );
             _objects.push_back( std::move(o) );
         }

         virtual void remove( object_id_type id ) override
         {
            assert( id.space() == T::space_id );
            assert( id.type() == T::type_id );
            const auto instance = id.instance();

            if( instance == _objects.size() - 1 )
               _objects.pop_back();
            else if( instance < _objects.size() )
               _objects[instance].reset();
         }

         virtual const object* get( object_id_type id )const override
         {
            assert( id.space() == T::space_id );
            assert( id.type() == T::type_id );

            const auto instance = id.instance();
            if( instance >= _objects.size() ) return nullptr;
            return _objects[instance].get();
         }

         void inspect_all_objects(std::function<void (const object*)> inspector)
         {
            try {
               for( const auto& ptr : _objects )
                  inspector(ptr.get());
            } FC_CAPTURE_AND_RETHROW()
         }

         class const_iterator
         {
            public:
               const_iterator(){}
               const_iterator( const vector<unique_ptr<object>>::const_iterator& a ):_itr(a){}
               friend bool operator==( const const_iterator& a, const const_iterator& b ) { return a._itr == b._itr; }
               friend bool operator!=( const const_iterator& a, const const_iterator& b ) { return a._itr != b._itr; }
               const T* operator*()const { return static_cast<const T*>(_itr->get()); }
               const_iterator& operator++(int){ ++_itr; return *this; }
               const_iterator& operator++()   { ++_itr; return *this; }
            private:
               vector<unique_ptr<object>>::const_iterator _itr;
         };
         const_iterator begin()const { return const_iterator(_objects.begin()); }
         const_iterator end()const   { return const_iterator(_objects.end());   }

         virtual void               replace( unique_ptr<object> o ) 
         {
            assert( dynamic_cast<T*>(o.get()) != nullptr );
            assert( _objects.size() > o->id.instance() );
            _objects[o->id.instance()] = std::move(o);
         }
      private:
         vector< unique_ptr<object> > _objects;
   };

} } // bts::chain
