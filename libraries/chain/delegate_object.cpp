#include <bts/chain/delegate_object.hpp>

using namespace bts::chain;

fc::optional<price_feed> delegate_feeds_object::get_feed(asset_id_type quote, asset_id_type base) const
{
   fc::optional<price_feed> result;
   price_feed dummy;
   dummy.call_limit = price({asset(1, base), asset(1, quote)});
   auto itr = feeds.find(dummy);
   if( itr != feeds.end() )
      result = *itr;
   return result;
}

price_feed& delegate_feeds_object::set_feed(const price_feed& p)
{
   auto itr = feeds.find(p);
   if( itr != feeds.end() )
      *itr = p;
   else
      itr = feeds.insert(p).first;
   return *itr;
}
