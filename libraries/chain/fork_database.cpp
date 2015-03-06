#include <bts/chain/fork_database.hpp>

namespace bts { namespace chain {

fork_database::fork_database()
:_by_num(1024)
{
}
void fork_database::reset()
{
   _head.reset();
   _by_num.resize(0);
   _by_num.resize(1024);
   _by_id.clear();
}

void fork_database::pop_block()
{
   if( _head ) _head = _head->prev.lock();
}

void     fork_database::start_block( signed_block b )
{
   auto item = std::make_shared<fork_item>();
   item->data = std::move(b);
   auto id = item->data.id();
   _by_num.resize(1024);
   _by_num[item->data.block_num()%1024].push_back(item);
   _by_id[id] = item;
   _head = item;
}

shared_ptr<fork_item>  fork_database::push_block( signed_block b )
{
   auto item = std::make_shared<fork_item>();
   item->data = std::move(b);
   auto id = item->data.id();
   auto itr = _by_id.find( id );
   FC_ASSERT( itr == _by_id.end() );
   auto prev_itr = _by_id.find( item->data.previous );
   if( item->data.block_num() > 1 )
   {
      FC_ASSERT( prev_itr != _by_id.end(), "unable to find prior", ("prior",item->data.previous)("num",item->data.block_num()) );
      item->prev = prev_itr->second;
      FC_ASSERT( prev_itr->second->data.block_num() == item->data.block_num()-1 );
   }
   _by_id[id] = item;

   auto n = item->data.block_num() % 1024;
   if( _by_num[n].size() && _by_num[n][0]->data.block_num() != item->data.block_num() )
   {
      for( auto i : _by_num[n] )
         _by_id.erase( i->data.id() );
      _by_num[n].resize(0);
   }

   _by_num[n].push_back(item);

   if( !_head || item->data.block_num() > _head->data.block_num() )
      _head = item;
   return _head;
}
pair<fork_database::branch_type,fork_database::branch_type>  
  fork_database::fetch_branch_from( block_id_type first, block_id_type second )const
{
   auto first_branch_itr = _by_id.find(first);
   FC_ASSERT( first_branch_itr != _by_id.end() );
   auto first_branch = first_branch_itr->second;

   auto second_branch_itr = _by_id.find(second);
   FC_ASSERT( second_branch_itr != _by_id.end() );
   auto second_branch = second_branch_itr->second;

   pair<branch_type,branch_type> result;

   while( first_branch->data.block_num() > second_branch->data.block_num() )
   {
      result.first.push_back( first_branch );
      first_branch = first_branch->prev.lock(); FC_ASSERT( first_branch );
   }
   while( second_branch->data.block_num() > first_branch->data.block_num() )
   {
      result.second.push_back( second_branch );
      second_branch = second_branch->prev.lock(); FC_ASSERT( second_branch );
   }
   while( first_branch->data.previous != second_branch->data.previous )
   {
      result.first.push_back( first_branch );
      result.second.push_back( second_branch );
      first_branch = first_branch->prev.lock(); FC_ASSERT( first_branch );
      second_branch = second_branch->prev.lock(); FC_ASSERT( second_branch );
   }
   return result;
}
void fork_database::set_head( shared_ptr<fork_item> h )
{
   _head = h;
}

void fork_database::remove( block_id_type id )
{
   auto itr = _by_id.find(id);
   if( itr == _by_id.end() ) return;
   if( itr->second == _head ) _head.reset();
   _by_id.erase(id);
}

} } // bts::chain
