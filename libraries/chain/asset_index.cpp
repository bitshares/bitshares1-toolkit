#include <bts/chain/asset_index.hpp>
#include <bts/chain/asset_object.hpp>
#include <functional>

namespace  bts { namespace  chain {

object_id_type asset_index::get_next_available_id()const
{
   return asset_id_type(size());
}

/**
 * Builds a new object and assigns it the next available ID and then
 * initializes it with constructor and lastly inserts it into the index.
 */
const object*  asset_index::create( const std::function<void(object*)>& constructor,
                                    object_id_type /*requested_id*/ )
{
    unique_ptr<asset_object> obj( new asset_object() );
    obj->id = get_next_available_id();
    constructor( obj.get() );
    auto result = obj.get();
    add( std::move(obj) );
    return result;
}

int64_t asset_index::size()const { return assets.size(); }

void  asset_index::modify( const object* obj, const std::function<void(object*)>& modify_callback )
{
   assert( obj != nullptr );
   FC_ASSERT( obj->id.instance() < assets.size() );
   const asset_object* a = dynamic_cast<const asset_object*>(obj);
   assert( a != nullptr );
   const string original_symbol = a->symbol; // TODO: optimize for release
   object* objptr  = assets[obj->id.instance()].get();
   modify_callback( objptr );
   assert( a->symbol == original_symbol );
}
void asset_index::replace( unique_ptr<object> o )
{
   assert( dynamic_cast<asset_object*>(o.get()) != nullptr );
   assets[o->id.instance()].reset( static_cast<asset_object*>(o.release()) );
}

void asset_index::add( unique_ptr<object> o )
{
   const auto id = o->id;
   assert( id.space()    == asset_object::space_id );
   assert( id.type()     == asset_object::type_id );
   assert( id.instance() == assets.size() );

   auto acnt = dynamic_cast<asset_object*>(o.get());
   assert( acnt != nullptr );
   o.release();
   unique_ptr<asset_object> new_asset( acnt );

   if( !new_asset->symbol.empty() )
   {
      auto itr = symbol_to_instance.find(new_asset->symbol);
      FC_ASSERT( itr == symbol_to_instance.end(), "symbol: ${symbol} is not unique", ("symbol",new_asset->symbol) );
   }

   if( id.instance() >= assets.size() )
      assets.resize( id.instance() + 1 );

   symbol_to_instance[new_asset->symbol] = new_asset->id.instance();
   assets[id.instance()] = std::move(new_asset);
}

void asset_index::remove( object_id_type id )
{
   if( id.instance() >= assets.size() )
      return;

   assert( id.space() == asset_object::space_id );
   assert( id.type() == asset_object::type_id );
   auto&  a = assets[id.instance()];
   if( a ) symbol_to_instance.erase(a->symbol);
   if( id.instance() == assets.size() - 1 )
      assets.pop_back();
   else
      a.reset();
}

const object* asset_index::get( object_id_type id )const
{
   if( id.instance() >= assets.size()       ||
       id.type() != asset_object::type_id   ||
       id.space() != asset_object::space_id    )
   {
      return nullptr;
   }
   return assets.at(id.instance()).get();
}

const asset_object* asset_index::get( const string& symbol )const
{
   auto itr = symbol_to_instance.find(symbol);
   if( itr == symbol_to_instance.end() ) return nullptr;
   return assets[itr->second].get();
}

packed_object  asset_index::get_meta_object()const
{
   return packed_object( index_meta_object( get_next_available_id() ) );
}
void           asset_index::set_meta_object( const packed_object& obj )
{
   index_meta_object meta;
   obj.unpack(meta);
   for( uint64_t i = meta.next_object_instance; i < assets.size(); ++i )
   {
      if( !assets[i]->symbol.empty() )
         symbol_to_instance.erase( assets[i]->symbol );
   }
   assets.resize( meta.next_object_instance );
}

void bts::chain::asset_index::inspect_all_objects(std::function<void (const object*)> inspector)
{
   try {
      for( const auto& ptr : assets )
         inspector(ptr.get());
   } FC_CAPTURE_AND_RETHROW()
}

} } // bts::chain
