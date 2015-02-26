#include <bts/chain/account_index.hpp>
#include <bts/chain/account_object.hpp>
#include <functional>

namespace bts { namespace chain {

object_id_type account_index::get_next_available_id()const 
{ 
   return account_id_type(size()); 
}

/**
 * Builds a new object and assigns it the next available ID and then
 * initializes it with constructor and lastly inserts it into the index.
 */
const object*  account_index::create( const std::function<void(object*)>& constructor )
{
    unique_ptr<account_object> obj( new account_object() );
    obj->id = get_next_available_id();
    constructor( obj.get() );
    auto result = obj.get();
    add( std::move(obj) );
    return result;
}

int64_t account_index::size()const { return accounts.size(); }

void  account_index::modify( const object* obj, const std::function<void(object*)>& modify_callback )
{
   assert( obj != nullptr );
   FC_ASSERT( obj->id.instance() < accounts.size() );
   const account_object* a = dynamic_cast<const account_object*>(obj);
   assert( a != nullptr );
   const string original_name = a->name; // TODO: optimize for release
   object* objptr  = accounts[obj->id.instance()].get();
   modify_callback( objptr );
   assert( a->name == original_name );
}

void account_index::add( unique_ptr<object> o ) 
{
   const auto id = o->id;
   assert( id.space()    == account_object::space_id );
   assert( id.type()     == account_object::type_id );
   assert( id.instance() == accounts.size() );

   auto acnt = dynamic_cast<account_object*>(o.get());
   assert( acnt != nullptr );
   o.release();
   unique_ptr<account_object> new_account( acnt );

   if( !new_account->name.empty() )
   {
      auto itr = name_to_id.find(new_account->name);
      FC_ASSERT( itr == name_to_id.end(), "name: ${name} is not unique", ("name",new_account->name) );
   }

   if( id.instance() >= accounts.size() ) 
      accounts.resize( id.instance() + 1 );

   name_to_id[new_account->name] = new_account.get();
   accounts[id.instance()] = std::move(new_account);
}

void account_index::remove( object_id_type id )
{
   if( id.instance() >= accounts.size() ) 
      return;

   assert( id.space() == account_object::space_id );
   assert( id.type() == account_object::type_id );
   auto&  a = accounts[id.instance()];
   if( a ) name_to_id.erase(a->name);
   if( id.instance() == accounts.size() - 1 )
      accounts.pop_back();
   else
      a.reset();
}

const object* account_index::get( object_id_type id )const 
{
   if( id.instance() >= accounts.size()       ||
       id.type() != account_object::type_id   ||
       id.space() != account_object::space_id    )
   {
      return nullptr;
   }
   return accounts.at(id.instance()).get();
}

const account_object* account_index::get( const string& name )const
{
   auto itr = name_to_id.find(name);
   if( itr == name_to_id.end() ) return nullptr;
   return itr->second;
}

packed_object  account_index::get_meta_object()const
{
   return packed_object( index_meta_object( get_next_available_id() ) );
}
void           account_index::set_meta_object( const packed_object& obj )
{
   index_meta_object meta;
   obj.unpack(meta);
   for( uint64_t i = meta.next_object_instance; i < accounts.size(); ++i )
   {
      if( !accounts[i]->name.empty() )
         name_to_id.erase( accounts[i]->name );
   }
   accounts.resize( meta.next_object_instance );
}

} } // bts::chain
