#pragma once
#include <bts/chain/types.hpp>

namespace bts { namespace chain {

   struct asset 
   {
      share_type    amount;
      asset_id_type asset_id;
   };

   struct price 
   {
      asset base;
      asset quote;
   };

} }

FC_REFLECT( bts::chain::asset, (amount)(asset_id) )
FC_REFLECT( bts::chain::price, (base)(quote) )
