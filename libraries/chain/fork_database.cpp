#include <bts/chain/fork_database.hpp>


namespace bts { namespace chain {
fork_database::fork_database()
{
}
void fork_database::reset()
{
   _head.reset();
   _index.clear();
}

void fork_database::pop_block()
{
   if( _head ) _head = _head->prev.lock();
}

void     fork_database::start_block( signed_block b )
{
   auto item = std::make_shared<fork_item>( std::move(b) );
   _index.insert( item );
   _head = item;
}

shared_ptr<fork_item>  fork_database::push_block( signed_block b )
{
   auto item = std::make_shared<fork_item>( std::move(b) );
   //wdump((item->num)(_head?_head->num:0));

   if( _head && b.previous != block_id_type() )
   {
      auto itr = _index.get<block_id>().find( b.previous );
      FC_ASSERT( itr != _index.get<block_id>().end() );
      FC_ASSERT( !(*itr)->invalid );
      item->prev = *itr;
   }

   _index.insert( item );
   if( !_head ) _head = item;
   else if( item->num > _head->num )
   {
      _head = item;
      _index.get<block_num>().erase( _head->num - 1024 );
   }
   return _head;
}
bool fork_database::is_known_block( const block_id_type& id )const
{
   auto& index = _index.get<block_id>();
   auto itr = index.find(id);
   return itr != index.end();
}
item_ptr fork_database::fetch_block( const block_id_type& id )const
{
   auto itr = _index.get<block_id>().find(id);
   if( itr != _index.get<block_id>().end() )
      return *itr;
   return item_ptr();
}
vector<item_ptr> fork_database::fetch_block_by_number( uint32_t num )const
{
   vector<item_ptr> result;
   auto itr = _index.get<block_num>().find(num);
   while( itr != _index.get<block_num>().end() )
   {
      if( (*itr)->num == num )
         result.push_back( *itr );
      else
         break;
      ++itr;
   }
   return result;
}

pair<fork_database::branch_type,fork_database::branch_type>
  fork_database::fetch_branch_from( block_id_type first, block_id_type second )const
{ try {
   pair<branch_type,branch_type> result;
   auto first_branch_itr = _index.get<block_id>().find(first);
   FC_ASSERT( first_branch_itr != _index.get<block_id>().end() );
   auto first_branch = *first_branch_itr;

   auto second_branch_itr = _index.get<block_id>().find(second);
   FC_ASSERT( second_branch_itr != _index.get<block_id>().end() );
   auto second_branch = *second_branch_itr;


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
   if( first_branch && second_branch )
   {
      result.first.push_back( first_branch );
      result.second.push_back( second_branch );
   }
   return result;
} FC_CAPTURE_AND_RETHROW( (first)(second) ) }
void fork_database::set_head( shared_ptr<fork_item> h )
{
   _head = h;
}

void fork_database::remove( block_id_type id )
{
   _index.get<block_id>().erase(id);
}

} } // bts::chain
