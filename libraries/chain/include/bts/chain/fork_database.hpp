#pragma once
#include <bts/chain/block.hpp>
#include <bts/chain/types.hpp>

namespace bts { namespace chain {

   struct fork_item 
   {
      weak_ptr< fork_item > prev;
      signed_block          data;
   };

   /**
    *  As long as blocks are pushed in order the fork
    *  database will maintain a linked tree of all blocks
    *  that branch from the start_block.  The tree will
    *  have a maximum depth of 1024 blocks after which
    *  the database will start lopping off forks.
    *
    *  Every time a block is pushed into the fork DB the
    *  block with the highest block_num will be returned.
    */
   class fork_database
   {
      public:
         void                    start_block( signed_block b )
         {
            auto item = std::make_shared<fork_item>();
            item->data = std::move(b);
            auto id = item->data.id();
            _by_num.resize(1024);
            _by_num[item->data.block_num()%1024].push_back(item);
            _by_id[id] = item;
            _head = item;
         }

         shared_ptr<fork_item>  push_block( signed_block b )
         {
            auto item = std::make_shared<fork_item>();
            item->data = std::move(b);
            auto id = item->data.id();
            auto itr = _by_id.find( id );
            FC_ASSERT( itr == _by_id.end() );
            auto prev_itr = _by_id.find( item->data.previous );
            FC_ASSERT( prev_itr != _by_id.end() );
            item->prev = prev_itr->second;
            FC_ASSERT( prev_itr->second->data.block_num() == item->data.block_num()-1 );
            _by_id[id] = item;

            auto n = item->data.block_num() % 1024;
            if( _by_num[n].size() && _by_num[n][0]->data.block_num() != item->data.block_num() )
               _by_num[n].resize(0);

            _by_num[n].push_back(item);

            if( item->data.block_num() > _head->data.block_num() )
               _head = item;
            return _head;
         }
      private:
         unordered_map< block_id_type, shared_ptr<fork_item> > _by_id;
         vector< vector<shared_ptr<fork_item>> >               _by_num;
         shared_ptr<fork_item>                                 _head;
   };
} } // bts::chain 
