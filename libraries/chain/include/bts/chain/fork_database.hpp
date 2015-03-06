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
         fork_database();
         void reset();

         void                   start_block( signed_block b );
         shared_ptr<fork_item>  push_block( signed_block b );
         void                   pop_block();
      private:
         unordered_map< block_id_type, shared_ptr<fork_item> > _by_id;
         vector< vector<shared_ptr<fork_item>> >               _by_num;
         shared_ptr<fork_item>                                 _head;
   };
} } // bts::chain 
