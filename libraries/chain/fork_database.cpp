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
      _by_num[n].resize(0);

   _by_num[n].push_back(item);

   if( !_head || item->data.block_num() > _head->data.block_num() )
      _head = item;
   return _head;
}

} } // bts::chain
