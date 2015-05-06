#include <bts/chain/types.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/database.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/block_summary_object.hpp>
#include <bts/chain/proposal_object.hpp>
#include <bts/chain/withdraw_permission_object.hpp>
#include <bts/chain/bond_object.hpp>
#include <bts/db/simple_index.hpp>
#include <bts/db/flat_index.hpp>

#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/key_evaluator.hpp>
#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/delegate_evaluator.hpp>
#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/limit_order_evaluator.hpp>
#include <bts/chain/short_order_evaluator.hpp>
#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/transaction_object.hpp>
#include <bts/chain/transfer_evaluator.hpp>
#include <bts/chain/proposal_evaluator.hpp>
#include <bts/chain/operation_history_object.hpp>
#include <bts/chain/global_parameters_evaluator.hpp>
#include <bts/chain/witness_object.hpp>
#include <bts/chain/witness_evaluator.hpp>
#include <bts/chain/bond_evaluator.hpp>

#include <fc/io/raw.hpp>
#include <fc/crypto/digest.hpp>
#include <fc/container/flat.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
   /*
template<typename T>
struct restore_on_scope_exit
{
   restore_on_scope_exit( T& v)
   :original_copy(v),value(v){}
   ~restore_on_scope_exit(){ value = original_copy; }
   T   original_copy;
   T&  value;
};

//template<typename T>
//restore_on_scope_exit<T> make_restore_on_exit( T& v ) { return restore_on_scope_exit<T>(v); }
*/

database::database()
{
   initialize_indexes();
   initialize_evaluators();
}

database::~database(){
   if( _pending_block_session )
      _pending_block_session->commit();
}

void database::close(uint32_t blocks_to_rewind)
{
   _pending_block_session.reset();

   for(int i = 0; i < blocks_to_rewind && head_block_num() > 0; ++i)
      pop_block();

   object_database::close();

   if( _block_id_to_block.is_open() )
      _block_id_to_block.close();

   _fork_db.reset();
}

const asset_object& database::get_core_asset() const
{
   return get(asset_id_type());
}

void database::wipe(const fc::path& data_dir, bool include_blocks)
{
   ilog("Wiping database", ("include_blocks", include_blocks));
   close();
   object_database::wipe(data_dir);
   if( include_blocks )
      fc::remove_all( data_dir / "database" );
}

void database::open( const fc::path& data_dir, const genesis_allocation& initial_allocation )
{ try {
   ilog("Open database in ${d}", ("d", data_dir));
   object_database::open( data_dir );

   _block_id_to_block.open( data_dir / "database" / "block_num_to_block" );

   if( !find(global_property_id_type()) )
      init_genesis(initial_allocation);

   _pending_block.previous  = head_block_id();
   _pending_block.timestamp = head_block_time();

   auto last_block_itr = _block_id_to_block.last();
   if( last_block_itr.valid() )
      _fork_db.start_block( last_block_itr.value() );

} FC_CAPTURE_AND_RETHROW( (data_dir) ) }

void database::initialize_evaluators()
{
   _operation_evaluators.resize(255);
   register_evaluator<key_create_evaluator>();
   register_evaluator<account_create_evaluator>();
   register_evaluator<account_update_evaluator>();
   register_evaluator<account_whitelist_evaluator>();
   register_evaluator<delegate_create_evaluator>();
   register_evaluator<asset_create_evaluator>();
   register_evaluator<asset_issue_evaluator>();
   register_evaluator<asset_update_evaluator>();
   register_evaluator<asset_update_bitasset_evaluator>();
   register_evaluator<asset_update_feed_producers_evaluator>();
   register_evaluator<asset_settle_evaluator>();
   register_evaluator<limit_order_create_evaluator>();
   register_evaluator<limit_order_cancel_evaluator>();
   register_evaluator<short_order_create_evaluator>();
   register_evaluator<short_order_cancel_evaluator>();
   register_evaluator<call_order_update_evaluator>();
   register_evaluator<transfer_evaluator>();
   register_evaluator<asset_fund_fee_pool_evaluator>();
   register_evaluator<asset_publish_feeds_evaluator>();
   register_evaluator<proposal_create_evaluator>();
   register_evaluator<proposal_update_evaluator>();
   register_evaluator<proposal_delete_evaluator>();
   register_evaluator<global_parameters_update_evaluator>();
   register_evaluator<witness_create_evaluator>();
   register_evaluator<witness_withdraw_pay_evaluator>();
   register_evaluator<create_bond_offer_evaluator>();
}

void database::initialize_indexes()
{
   reset_indexes();

   //Protocol object indexes
   add_index< primary_index< asset_index> >();
   add_index< primary_index< force_settlement_index> >();
   add_index< primary_index< account_index> >();
   add_index< primary_index< simple_index<key_object>> >();
   add_index< primary_index< simple_index<delegate_object>> >();
   add_index< primary_index< simple_index<witness_object>> >();
   add_index< primary_index< limit_order_index > >();
   add_index< primary_index< short_order_index > >();
   add_index< primary_index< call_order_index > >();
   add_index< primary_index< proposal_index > >();
   add_index< primary_index< withdraw_permission_index > >();
   add_index< primary_index< bond_index > >();
   add_index< primary_index< bond_offer_index > >();

   //Implementation object indexes
   add_index< primary_index< transaction_index                             > >();
   add_index< primary_index< account_balance_index                         > >();
   add_index< primary_index< asset_bitasset_data_index                     > >();
   add_index< primary_index< simple_index< global_property_object         >> >();
   add_index< primary_index< simple_index< dynamic_global_property_object >> >();
   add_index< primary_index< simple_index< account_statistics_object      >> >();
   add_index< primary_index< simple_index< asset_dynamic_data_object      >> >();
   add_index< primary_index< flat_index<   block_summary_object           >> >();
}

void database::init_genesis(const genesis_allocation& initial_allocation)
{
   _undo_db.disable();

   fc::ecc::private_key genesis_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));
   const key_object& genesis_key =
      create<key_object>( [&genesis_private_key](key_object& k) {
         k.key_data = public_key_type(genesis_private_key.get_public_key());
      });
   const account_statistics_object& genesis_statistics =
      create<account_statistics_object>( [&](account_statistics_object& b){
      });
   create<account_balance_object>( [](account_balance_object& b) {
      b.balance = BTS_INITIAL_SUPPLY;
   });
   const account_object& genesis_account =
      create<account_object>( [&](account_object& n) {
         n.owner.add_authority(genesis_key.get_id(), 1);
         n.owner.weight_threshold = 1;
         n.active = n.owner;
         n.memo_key = genesis_key.id;
         n.statistics = genesis_statistics.id;
      });

   vector<delegate_id_type> init_delegates;
   vector<witness_id_type> init_witnesses;

   auto delegates_and_witnesses = std::max(BTS_MIN_WITNESS_COUNT, BTS_MIN_DELEGATE_COUNT);
   for( int i = 0; i < delegates_and_witnesses; ++i )
   {
      const account_statistics_object& stats_obj =
         create<account_statistics_object>( [&](account_statistics_object&){
         });
      const account_object& delegate_account =
         create<account_object>( [&](account_object& a) {
            a.active = a.owner = genesis_account.owner;
            a.name = string("init") + fc::to_string(i);
            a.statistics = stats_obj.id;
         });
      const delegate_object& init_delegate = create<delegate_object>( [&](delegate_object& d) {
         d.delegate_account = delegate_account.id;
         d.vote_id = i * 2;
      });
      init_delegates.push_back(init_delegate.id);

      const witness_object& init_witness = create<witness_object>( [&](witness_object& d) {
            d.witness_account = delegate_account.id;
            d.vote_id = i * 2 + 1;
            secret_hash_type::encoder enc;
            fc::raw::pack( enc, genesis_private_key );
            fc::raw::pack( enc, d.last_secret );
            d.next_secret = secret_hash_type::hash(enc.result());
      });
      init_witnesses.push_back(init_witness.id);

   }
   create<block_summary_object>( [&](block_summary_object& p) {
   });

   const global_property_object& properties =
      create<global_property_object>( [&](global_property_object& p) {
         p.active_delegates = init_delegates;
         p.active_witnesses = init_witnesses;
         p.next_available_vote_id = delegates_and_witnesses * 2;
      });
   (void)properties;

   create<dynamic_global_property_object>( [&](dynamic_global_property_object& p) {
      });

   const asset_dynamic_data_object& dyn_asset =
      create<asset_dynamic_data_object>( [&]( asset_dynamic_data_object& a ) {
         a.current_supply = BTS_INITIAL_SUPPLY;
      });

   const asset_object& core_asset =
     create<asset_object>( [&]( asset_object& a ) {
         a.symbol = BTS_SYMBOL;
         a.options.max_supply = BTS_INITIAL_SUPPLY;
         a.options.flags = 0;
         a.options.issuer_permissions = 0;
         a.issuer = genesis_account.id;
         a.options.core_exchange_rate.base.amount = 1;
         a.options.core_exchange_rate.base.asset_id = 0;
         a.options.core_exchange_rate.quote.amount = 1;
         a.options.core_exchange_rate.quote.asset_id = 0;
         a.dynamic_asset_data_id = dyn_asset.id;
      });
   assert( asset_id_type(core_asset.id) == asset().asset_id );
   assert( get_balance(account_id_type(), asset_id_type()) == asset(dyn_asset.current_supply) );
   (void)core_asset;

   if( !initial_allocation.empty() )
   {
      share_type total_allocation = 0;
      for( const auto& handout : initial_allocation )
         total_allocation += handout.second;

      fc::time_point start_time = fc::time_point::now();

      for( const auto& handout : initial_allocation )
      {
         asset amount(handout.second);
         amount.amount = ((fc::uint128(amount.amount.value) * BTS_INITIAL_SUPPLY)/total_allocation.value).to_uint64();
         if( amount.amount == 0 )
         {
            wlog("Skipping zero allocation to ${k}", ("k", handout.first));
            continue;
         }

         signed_transaction trx;
         trx.operations.emplace_back(key_create_operation({genesis_account.id, asset(), handout.first}));
         relative_key_id_type key_id(0);
         authority account_authority(1, key_id, 1);
         account_create_operation cop;
         cop.registrar = account_id_type(1);
         cop.active = account_authority;
         cop.owner = account_authority;
         cop.memo_key = key_id;
         trx.operations.push_back(cop);
         trx.validate();
         auto ptrx = apply_transaction(trx, ~0);
         trx = signed_transaction();
         account_id_type account_id(ptrx.operation_results.back().get<object_id_type>());
         trx.operations.emplace_back(transfer_operation({
                                                           genesis_account.id,
                                                           account_id,
                                                           amount,
                                                           asset(),
                                                           vector<char>()
                                                        }));
         trx.validate();
         apply_transaction(trx, ~0);
      }

      asset leftovers = get_balance(account_id_type(), asset_id_type());
      if( leftovers.amount > 0 )
      {
         modify(*get_index_type<account_balance_index>().indices().get<by_balance>().find(boost::make_tuple(account_id_type(), asset_id_type())),
                [](account_balance_object& b) {
            b.adjust_balance(-b.get_balance());
         });
         modify(core_asset.dynamic_asset_data_id(*this), [&leftovers](asset_dynamic_data_object& d) {
            d.accumulated_fees += leftovers.amount;
         });
      }

      fc::microseconds duration = fc::time_point::now() - start_time;
      ilog("Finished allocating to ${n} accounts in ${t} milliseconds.",
           ("n", initial_allocation.size())("t", duration.count() / 1000));
   }
   _undo_db.enable();
}

asset database::get_balance(account_id_type owner, asset_id_type asset_id) const
{
   auto& index = get_index_type<account_balance_index>().indices().get<by_balance>();
   auto itr = index.find(boost::make_tuple(owner, asset_id));
   if( itr == index.end() )
      return asset(0, asset_id);
   return itr->get_balance();
}

asset database::get_balance(const account_object& owner, const asset_object& asset_obj) const
{
   return get_balance(owner.get_id(), asset_obj.get_id());
}

void database::adjust_balance(account_id_type account, asset delta)
{ try {
   if( delta.amount == 0 )
      return;

   auto& index = get_index_type<account_balance_index>().indices().get<by_balance>();
   auto itr = index.find(boost::make_tuple(account, delta.asset_id));
   if(itr == index.end())
   {
      FC_ASSERT(delta.amount > 0);
      create<account_balance_object>([account,&delta](account_balance_object& b) {
         b.owner = account;
         b.asset_type = delta.asset_id;
         b.balance = delta.amount;
      });
   } else {
      FC_ASSERT(delta.amount > 0 || itr->get_balance() >= -delta);
      modify(*itr, [delta](account_balance_object& b) {
         b.adjust_balance(delta);
      });
   }
} FC_CAPTURE_AND_RETHROW( (account)(delta) ) }

/**
 *  Matches the two orders,
 *
 *  @return a bit field indicating which orders were filled (and thus removed)
 *
 *  0 - no orders were matched
 *  1 - bid was filled
 *  2 - ask was filled
 *  3 - both were filled
 */
template<typename OrderType>
int database::match( const limit_order_object& usd, const OrderType& core, const price& match_price )
{
   assert( usd.sell_price.quote.asset_id == core.sell_price.base.asset_id );
   assert( usd.sell_price.base.asset_id  == core.sell_price.quote.asset_id );
   assert( usd.for_sale > 0 && core.for_sale > 0 );

   auto usd_for_sale = usd.amount_for_sale();
   auto core_for_sale = core.amount_for_sale();

   asset usd_pays, usd_receives, core_pays, core_receives;

   if( usd_for_sale <= core_for_sale * match_price )
   {
      core_receives = usd_for_sale;
      usd_receives  = usd_for_sale * match_price;
   }
   else
   {
      //This line once read: assert( core_for_sale < usd_for_sale * match_price );
      //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
      //Although usd_for_sale is greater than core_for_sale * match_price, core_for_sale == usd_for_sale * match_price
      //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
      usd_receives = core_for_sale;
      core_receives = core_for_sale * match_price;
   }

   core_pays = usd_receives;
   usd_pays  = core_receives;

   assert( usd_pays == usd.amount_for_sale() ||
           core_pays == core.amount_for_sale() );

   int result = 0;
   result |= fill_order( usd, usd_pays, usd_receives );
   result |= fill_order( core, core_pays, core_receives ) << 1;
   assert( result != 0 );
   return result;
}

int database::match( const call_order_object& call, const force_settlement_object& settle, const price& match_price )
{
   assert(call.get_debt().asset_id == settle.balance.asset_id );
   assert(call.debt > 0 && call.collateral > 0 && settle.balance.amount > 0);

   auto settle_for_sale = settle.balance;
   auto call_debt = call.get_debt();

   asset call_receives = std::min(settle_for_sale, call_debt),
         call_pays = call_receives * match_price,
         settle_pays = call_receives,
         settle_receives = call_pays;

   assert( settle_pays == settle.balance || call_receives == call.get_debt() );

   int result = 0;
   result |= fill_order(call, call_pays, call_receives);
   result |= fill_order(settle, settle_pays, settle_receives) << 1;

   assert(result != 0);
   return result;
}

int database::match( const limit_order_object& bid, const limit_order_object& ask, const price& match_price )
{
   return match<limit_order_object>( bid, ask, match_price );
}
int database::match( const limit_order_object& bid, const short_order_object& ask, const price& match_price )
{
   return match<short_order_object>( bid, ask, match_price );
}

/**
 *
 */
bool database::check_call_orders( const asset_object& mia )
{ try {
    if( !mia.is_market_issued() ) return false;
    const asset_bitasset_data_object& bitasset = mia.bitasset_data(*this);
    if( bitasset.current_feed.call_limit.is_null() ) return false;

    const call_order_index& call_index = get_index_type<call_order_index>();
    const auto& call_price_index = call_index.indices().get<by_price>();

    const limit_order_index& limit_index = get_index_type<limit_order_index>();
    const auto& limit_price_index = limit_index.indices().get<by_price>();

    const short_order_index& short_index = get_index_type<short_order_index>();
    const auto& short_price_index = short_index.indices().get<by_price>();

    auto short_itr = short_price_index.lower_bound( price::max( mia.id, bitasset.short_backing_asset ) );
    auto short_end = short_price_index.upper_bound( ~bitasset.current_feed.call_limit );

    auto limit_itr = limit_price_index.lower_bound( price::max( mia.id, bitasset.short_backing_asset ) );
    auto limit_end = limit_price_index.upper_bound( ~bitasset.current_feed.call_limit );

    auto call_itr = call_price_index.lower_bound( price::min( bitasset.short_backing_asset, mia.id ) );
    auto call_end = call_price_index.upper_bound( price::max( bitasset.short_backing_asset, mia.id ) );

    bool filled_short_or_limit = false;

    while( call_itr != call_end )
    {
       bool  current_is_limit = true;
       bool  filled_call      = false;
       price match_price;
       asset usd_for_sale;
       if( limit_itr != limit_end )
       {
          assert( limit_itr != limit_price_index.end() );
          if( short_itr != short_end && limit_itr->sell_price < short_itr->sell_price )
          {
             assert( short_itr != short_price_index.end() );
             current_is_limit = false;
             match_price      = short_itr->sell_price;
             usd_for_sale     = short_itr->amount_for_sale();
          }
          else
          {
             current_is_limit = true;
             match_price      = limit_itr->sell_price;
             usd_for_sale     = limit_itr->amount_for_sale();
          }
       }
       else if( short_itr != short_end )
       {
          assert( short_itr != short_price_index.end() );
          current_is_limit = false;
          match_price      = short_itr->sell_price;
          usd_for_sale     = short_itr->amount_for_sale();
       }
       else return filled_short_or_limit;

       match_price.validate();

       if( match_price > ~call_itr->call_price )
       {
          return filled_short_or_limit;
       }

       auto usd_to_buy   = call_itr->get_debt();

       if( usd_to_buy * match_price > call_itr->get_collateral() )
       {
          elog( "black swan, we do not have enough collateral to cover at this price" );
          settle_black_swan( mia, call_itr->get_debt() / call_itr->get_collateral() );
          return true;
       }

       asset call_pays, call_receives, order_pays, order_receives;
       if( usd_to_buy >= usd_for_sale )
       {  // fill order
          call_receives   = usd_for_sale;
          order_receives  = usd_for_sale * match_price;
          call_pays       = order_receives;
          order_pays      = usd_for_sale;

          filled_short_or_limit = true;
          filled_call           = (usd_to_buy == usd_for_sale);
       }
       else // fill call
       {
          call_receives  = usd_to_buy;
          order_receives = usd_to_buy * match_price;
          call_pays      = order_receives;
          order_pays     = usd_to_buy;

          filled_call    = true;
       }

       auto old_call_itr = call_itr;
       if( filled_call ) ++call_itr;
       fill_order( *old_call_itr, call_pays, call_receives );
       if( current_is_limit )
       {
          auto old_limit_itr = !filled_call ? limit_itr++ : limit_itr;
          fill_order( *old_limit_itr, order_pays, order_receives );
       }
       else
       {
          auto old_short_itr = !filled_call ? short_itr++ : short_itr;
          fill_order( *old_short_itr, order_pays, order_receives );
       }
    } // whlie call_itr != call_end

    return filled_short_or_limit;
} FC_CAPTURE_AND_RETHROW() }

void database::cancel_order( const limit_order_object& order, bool create_virtual_op  )
{
   auto refunded = order.amount_for_sale();

   modify( order.seller(*this).statistics(*this),[&]( account_statistics_object& obj ){
      if( refunded.asset_id == asset_id_type() )
         obj.total_core_in_orders -= refunded.amount;
   });
   adjust_balance(order.seller, refunded);

   if( create_virtual_op )
   {
      // TODO: create a virtual cancel operation
   }

   remove( order );
}

/**
    for each short order, fill it at settlement price and place funds received into a total
    calculate the USD->BTS price and convert all USD balances to BTS at that price and subtract BTS from total
       - any fees accumulated by the issuer in the bitasset are forfeit / not redeemed
       - cancel all open orders with bitasset in it
       - any bonds with the bitasset as collateral get converted to BTS as collateral
       - any bitassets that use this bitasset as collateral are immediately settled at their feed price
       - convert all balances in bitasset to BTS and subtract from total
       - any prediction markets with usd as the backing get converted to BTS as the backing
    any BTS left over due to rounding errors is paid to accumulated fees
*/
void database::settle_black_swan( const asset_object& mia, const price& settlement_price )
{ try {
   elog( "BLACK SWAN!" );
   debug_dump();

   edump( (mia.symbol)(settlement_price) );

   const asset_bitasset_data_object& bitasset = mia.bitasset_data(*this);
   const asset_object& backing_asset = bitasset.short_backing_asset(*this);
   asset collateral_gathered = backing_asset.amount(0);

   const asset_dynamic_data_object& mia_dyn = mia.dynamic_asset_data_id(*this);
   auto original_mia_supply = mia_dyn.current_supply;

   const call_order_index& call_index = get_index_type<call_order_index>();
   const auto& call_price_index = call_index.indices().get<by_price>();

    auto call_itr = call_price_index.lower_bound( price::min( bitasset.short_backing_asset, mia.id ) );
    auto call_end = call_price_index.upper_bound( price::max( bitasset.short_backing_asset, mia.id ) );
    while( call_itr != call_end )
    {
       auto pays = call_itr->get_debt() * settlement_price;
       wdump( (call_itr->get_debt() ) );
       collateral_gathered += pays;
       const auto&  order = *call_itr;
       ++call_itr;
       FC_ASSERT( fill_order( order, pays, order.get_debt() ) );
    }

   const limit_order_index& limit_index = get_index_type<limit_order_index>();
   const auto& limit_price_index = limit_index.indices().get<by_price>();

    // cancel all orders selling the market issued asset
    auto limit_itr = limit_price_index.lower_bound( price::max( mia.id, bitasset.short_backing_asset ) );
    auto limit_end = limit_price_index.upper_bound( ~bitasset.current_feed.call_limit );
    while( limit_itr != limit_end )
    {
       const auto& order = *limit_itr;
       ilog( "CANCEL LIMIT ORDER" );
        idump((order));
       ++limit_itr;
       cancel_order( order );
    }

    limit_itr = limit_price_index.begin();
    while( limit_itr != limit_end )
    {
       if( limit_itr->amount_for_sale().asset_id == mia.id )
       {
          const auto& order = *limit_itr;
          ilog( "CANCEL_AGAIN" );
          edump((order));
          ++limit_itr;
          cancel_order( order );
       }
    }

   limit_itr = limit_price_index.begin();
   while( limit_itr != limit_end )
   {
      if( limit_itr->amount_for_sale().asset_id == mia.id )
      {
         const auto& order = *limit_itr;
         edump((order));
         ++limit_itr;
         cancel_order( order );
      }
   }

    asset total_mia_settled = mia.amount(0);
    auto& index = get_index_type<account_balance_index>().indices().get<by_asset>();
    auto range = index.equal_range(mia.get_id());
    for( auto itr = range.first; itr != range.second; ++itr )
    {
       auto mia_balance = itr->get_balance();
       if( mia_balance.amount > 0 )
       {
          adjust_balance(itr->owner, -mia_balance);
          auto settled_amount = mia_balance * settlement_price;
          idump( (mia_balance)(settled_amount)(settlement_price) );
          adjust_balance(itr->owner, settled_amount);
          total_mia_settled += mia_balance;
          collateral_gathered -= settled_amount;
       }
    }

    // TODO: convert collateral held in bonds
    // TODO: convert payments held in escrow
    // TODO: convert usd held as prediction market collateral

    modify( mia_dyn, [&]( asset_dynamic_data_object& obj ){
       total_mia_settled.amount += obj.accumulated_fees;
       obj.accumulated_fees = 0;
    });

    wlog( "====================== AFTER SETTLE BLACK SWAN UNCLAIMED SETTLEMENT FUNDS ==============\n" );
    wdump((collateral_gathered)(total_mia_settled)(original_mia_supply)(mia_dyn.current_supply));
    modify( bitasset.short_backing_asset(*this).dynamic_asset_data_id(*this), [&]( asset_dynamic_data_object& obj ){
       obj.accumulated_fees += collateral_gathered.amount;
    });

    FC_ASSERT( total_mia_settled.amount == original_mia_supply, "", ("total_settled",total_mia_settled)("original",original_mia_supply) );
} FC_CAPTURE_AND_RETHROW( (mia)(settlement_price) ) }

asset database::calculate_market_fee( const asset_object& trade_asset, const asset& trade_amount )
{
   assert( trade_asset.id == trade_amount.asset_id );

   if( !trade_asset.charges_market_fees() )
      return trade_asset.amount(0);
   if( trade_asset.options.market_fee_percent == 0 )
      return trade_asset.amount(trade_asset.options.min_market_fee);

   fc::uint128 a(trade_amount.amount.value);
   a *= trade_asset.options.market_fee_percent;
   a /= BTS_100_PERCENT;
   asset percent_fee = trade_asset.amount(a.to_uint64());

   if( percent_fee.amount > trade_asset.options.max_market_fee )
      percent_fee.amount = trade_asset.options.max_market_fee;
   else if( percent_fee.amount < trade_asset.options.min_market_fee )
      percent_fee.amount = trade_asset.options.min_market_fee;

   return percent_fee;
}

asset database::pay_market_fees( const asset_object& recv_asset, const asset& receives )
{
   auto issuer_fees = calculate_market_fee( recv_asset, receives );
   assert(issuer_fees <= receives );

   //Don't dirty undo state if not actually collecting any fees
   if( issuer_fees.amount > 0 )
   {
      const auto& recv_dyn_data = recv_asset.dynamic_asset_data_id(*this);
      modify( recv_dyn_data, [&]( asset_dynamic_data_object& obj ){
                   idump((issuer_fees));
         obj.accumulated_fees += issuer_fees.amount;
      });
   }

   return issuer_fees;
}

void database::pay_order( const account_object& receiver, const asset& receives, const asset& pays )
{
   const auto& balances = receiver.statistics(*this);
   modify( balances, [&]( account_statistics_object& b ){
         if( pays.asset_id == asset_id_type() )
            b.total_core_in_orders -= pays.amount;
   });
   adjust_balance(receiver.get_id(), receives);
}

/**
 *  For Market Issued assets Managed by Delegates, any fees collected in the MIA need
 *  to be sold and converted into CORE by accepting the best offer on the table.
 */
bool database::convert_fees( const asset_object& mia )
{
   if( mia.issuer != account_id_type() ) return false;
   return false;
}

bool database::fill_order( const limit_order_object& order, const asset& pays, const asset& receives )
{
   assert( order.amount_for_sale().asset_id == pays.asset_id );
   assert( pays.asset_id != receives.asset_id );

   const account_object& seller = order.seller(*this);
   const asset_object& recv_asset = receives.asset_id(*this);

   auto issuer_fees = pay_market_fees( recv_asset, receives );
   pay_order( seller, receives - issuer_fees, pays );

   push_applied_operation( fill_order_operation{ order.id, order.seller, pays, receives, issuer_fees } );

   if( pays == order.amount_for_sale() )
   {
      remove( order );
      return true;
   }
   else
   {
      modify( order, [&]( limit_order_object& b ) {
                             b.for_sale -= pays.amount;
                          });
      /**
       *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
       *  have hit the limit where the seller is asking for nothing in return.  When this
       *  happens we must refund any balance back to the seller, it is too small to be
       *  sold at the sale price.
       */
      if( order.amount_to_receive().amount == 0 )
      {
         cancel_order(order);
         return true;
      }
      return false;
   }
}
bool database::fill_order( const call_order_object& order, const asset& pays, const asset& receives )
{ try {
   idump((pays)(receives)(order));
   assert( order.get_debt().asset_id == receives.asset_id );
   assert( order.get_collateral().asset_id == pays.asset_id );
   assert( order.get_collateral() >= pays );

   optional<asset> collateral_freed;
   modify( order, [&]( call_order_object& o ){
            o.debt       -= receives.amount;
            o.collateral -= pays.amount;
            if( o.debt == 0 )
            {
              collateral_freed = o.get_collateral();
              o.collateral = 0;
            }
       });
   const asset_object& mia = receives.asset_id(*this);
   assert( mia.is_market_issued() );

   const asset_dynamic_data_object& mia_ddo = mia.dynamic_asset_data_id(*this);

   modify( mia_ddo, [&]( asset_dynamic_data_object& ao ){
       idump((receives));
        ao.current_supply -= receives.amount;
      });

   const account_object& borrower = order.borrower(*this);
   if( collateral_freed || pays.asset_id == asset_id_type() )
   {
      const account_statistics_object& borrower_statistics = borrower.statistics(*this);
      if( collateral_freed )
         adjust_balance(borrower.get_id(), *collateral_freed);
      modify( borrower_statistics, [&]( account_statistics_object& b ){
              if( collateral_freed && collateral_freed->amount > 0 )
                b.total_core_in_orders -= collateral_freed->amount;
              if( pays.asset_id == asset_id_type() )
                b.total_core_in_orders -= pays.amount;
              assert( b.total_core_in_orders >= 0 );
           });
   }

   if( collateral_freed )
   {
      remove( order );
   }

   push_applied_operation( fill_order_operation{ order.id, order.borrower, pays, receives, asset(0, pays.asset_id) } );

   return collateral_freed.valid();
} FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) ) }


bool database::fill_order( const short_order_object& order, const asset& pays, const asset& receives )
{ try {
   assert( order.amount_for_sale().asset_id == pays.asset_id );
   assert( pays.asset_id != receives.asset_id );

   const call_order_index& call_index = get_index_type<call_order_index>();

   const account_object& seller = order.seller(*this);
   const asset_object& recv_asset = receives.asset_id(*this);
   const asset_object& pays_asset = pays.asset_id(*this);
   assert( pays_asset.is_market_issued() );

   auto issuer_fees = pay_market_fees( recv_asset, receives );

   bool filled               = pays == order.amount_for_sale();
   auto seller_to_collateral = filled ? order.get_collateral() : pays * order.sell_price;
   auto buyer_to_collateral  = receives - issuer_fees;

   if( receives.asset_id == asset_id_type() )
   {
      const auto& statistics = seller.statistics(*this);
      modify( statistics, [&]( account_statistics_object& b ){
             b.total_core_in_orders += buyer_to_collateral.amount;
      });
   }

   modify( pays_asset.dynamic_asset_data_id(*this), [&]( asset_dynamic_data_object& obj ){
      obj.current_supply += pays.amount;
   });

   const auto& call_account_index = call_index.indices().get<by_account>();
   auto call_itr = call_account_index.find(  boost::make_tuple(order.seller, pays.asset_id) );
   if( call_itr == call_account_index.end() )
   {
      create<call_order_object>( [&]( call_order_object& c ){
         c.borrower    = seller.id;
         c.collateral  = seller_to_collateral.amount + buyer_to_collateral.amount;
         c.debt        = pays.amount;
         c.maintenance_collateral_ratio = order.maintenance_collateral_ratio;
         c.call_price  = price::max(seller_to_collateral.asset_id, pays.asset_id);
         c.update_call_price();
      });
   }
   else
   {
      modify( *call_itr, [&]( call_order_object& c ){
         c.debt       += pays.amount;
         c.collateral += seller_to_collateral.amount + buyer_to_collateral.amount;
         c.maintenance_collateral_ratio = order.maintenance_collateral_ratio;
         c.update_call_price();
      });
   }

   if( filled )
   {
      remove( order );
   }
   else
   {
      modify( order, [&]( short_order_object& b ) {
         b.for_sale -= pays.amount;
         b.available_collateral -= seller_to_collateral.amount;
         assert( b.available_collateral > 0 );
         assert( b.for_sale > 0 );
      });

      /**
       *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
       *  have hit the limit where the seller is asking for nothing in return.  When this
       *  happens we must refund any balance back to the seller, it is too small to be
       *  sold at the sale price.
       */
      if( order.amount_to_receive().amount == 0 )
      {
         adjust_balance(seller.get_id(), order.get_collateral());
         if( order.get_collateral().asset_id == asset_id_type() )
         {
            const auto& statistics = seller.statistics(*this);
            modify( statistics, [&]( account_statistics_object& b ){
                 b.total_core_in_orders -= order.available_collateral;
            });
         }

         remove( order );
         filled = true;
      }
   }

   push_applied_operation( fill_order_operation{ order.id, order.seller, pays, receives, issuer_fees } );

   return filled;
} FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) ) }

bool database::fill_order(const force_settlement_object& settle, const asset& pays, const asset& receives)
{ try {
   bool filled = false;

   auto issuer_fees = pay_market_fees(get(receives.asset_id), receives);

   if( pays < settle.balance )
   {
      modify(settle, [&pays](force_settlement_object& s) {
         s.balance -= pays;
      });
      filled = false;
   } else {
      remove(settle);
      filled = true;
   }
   adjust_balance(settle.owner, receives - issuer_fees);

   push_applied_operation( fill_order_operation{ settle.id, settle.owner, pays, receives, issuer_fees } );

   return filled;
} FC_CAPTURE_AND_RETHROW( (settle)(pays)(receives) ) }

void database::reindex(fc::path data_dir, genesis_allocation initial_allocation)
{ try {
   wipe(data_dir, false);
   open(data_dir, initial_allocation);
   assert(head_block_num() == 0);

   auto start = fc::time_point::now();
   auto itr = _block_id_to_block.begin();
   while( itr.valid() )
   {
      apply_block( itr.value(), skip_delegate_signature |
                                skip_transaction_signatures |
                                skip_undo_block |
                                skip_undo_transaction |
                                skip_transaction_dupe_check |
                                skip_tapos_check );
      ++itr;
   }
   auto end = fc::time_point::now();
   wdump( ((end-start).count()/1000000.0) );
} FC_CAPTURE_AND_RETHROW( (data_dir)(initial_allocation) ) }

asset database::current_delegate_registration_fee()const
{
   return asset();
}

void database::apply_block( const signed_block& next_block, uint32_t skip )
{ try {
   _applied_ops.clear();

   const witness_object& signing_witness = validate_block_header(skip, next_block);
   const auto& global_props = get_global_properties();
   const auto& dynamic_global_props = get<dynamic_global_property_object>(dynamic_global_property_id_type());

   _current_block_num    = next_block.block_num();
   _current_trx_in_block = 0;

   for( const auto& trx : next_block.transactions )
   {
      /* We do not need to push the undo state for each transaction
       * because they either all apply and are valid or the
       * entire block fails to apply.  We only need an "undo" state
       * for transactions when validating broadcast transactions or
       * when building a block.
       */
      apply_transaction( trx, skip );
      ++_current_trx_in_block;
   }

   update_global_dynamic_data( next_block );
   update_signing_witness(signing_witness, next_block);

   auto current_block_interval = global_props.parameters.block_interval;

   // Are we at the maintenance interval?
   if( dynamic_global_props.next_maintenance_time <= next_block.timestamp )
      // This will update _pending_block.timestamp if the block interval has changed
      perform_chain_maintenance(next_block, global_props);
   // If we're at the end of a round, shuffle the active witnesses
   // We can skip this if they were just updated during chain maintenance
   else if( (next_block.block_num() % global_props.active_delegates.size()) == 0 )
      modify(global_props, [this](global_property_object& p) {
         shuffle_vector(p.active_witnesses);
      });

   create_block_summary(next_block);
   clear_expired_transactions();
   clear_expired_proposals();
   clear_expired_orders();

   // notify observers that the block has been applied
   applied_block( next_block ); //emit
   _applied_ops.clear();

   const auto& head_undo = _undo_db.head();
   vector<object_id_type> changed_ids;  changed_ids.reserve(head_undo.old_values.size());
   for( const auto& item : head_undo.old_values ) changed_ids.push_back(item.first);
   changed_objects(changed_ids);


   update_pending_block(next_block, current_block_interval);
} FC_CAPTURE_AND_RETHROW( (next_block.block_num())(skip) )  }

time_point database::get_next_generation_time( witness_id_type del_id )const
{
   const auto& gp = get_global_properties();
   auto now = bts::chain::now();
   const auto& active_witness = gp.active_witnesses;
   const auto& interval   = gp.parameters.block_interval;
   auto witness_slot = ((now.sec_since_epoch()+interval-1) /interval);
   for( uint32_t i = 0; i < active_witness.size(); ++i )
   {
      if( active_witness[ witness_slot % active_witness.size()] == del_id )
         return time_point_sec() + fc::seconds( witness_slot * interval );
      ++witness_slot;
   }
   FC_ASSERT( !"Not an Active Witness" );
}

std::pair<fc::time_point, witness_id_type> database::get_next_generation_time(const set<bts::chain::witness_id_type>& witnesses) const
{
   std::pair<fc::time_point, witness_id_type> result;
   result.first = fc::time_point::maximum();
   for( witness_id_type id : witnesses )
      result = std::min(result, std::make_pair(get_next_generation_time(id), id));
   return result;
}


signed_block database::generate_block( const fc::ecc::private_key& delegate_key,
                                       witness_id_type witness_id, uint32_t  skip )
{ try {
   const auto& witness_obj = witness_id(*this);

   if( !(skip & skip_delegate_signature) )
      FC_ASSERT( witness_obj.signing_key(*this).key() == delegate_key.get_public_key() );

   _pending_block.timestamp = get_next_generation_time( witness_id );

   secret_hash_type::encoder last_enc;
   fc::raw::pack( last_enc, delegate_key );
   fc::raw::pack( last_enc, witness_obj.last_secret );
   _pending_block.previous_secret = last_enc.result();

   secret_hash_type::encoder next_enc;
   fc::raw::pack( next_enc, delegate_key );
   fc::raw::pack( next_enc, _pending_block.previous_secret );
   _pending_block.next_secret_hash = secret_hash_type::hash(next_enc.result());

   _pending_block.witness = witness_id;
   if( !(skip & skip_delegate_signature) ) _pending_block.sign( delegate_key );

   FC_ASSERT( fc::raw::pack_size(_pending_block) <= get_global_properties().parameters.maximum_block_size );
   //This line used to std::move(_pending_block) but this is unsafe as _pending_block is later referenced without being
   //reinitialized. Future optimization could be to move it, then reinitialize it with the values we need to preserve.
   signed_block tmp = _pending_block;
   _pending_block.transactions.clear();
   push_block( tmp, skip );
   return tmp;
} FC_CAPTURE_AND_RETHROW( (witness_id) ) }

void database::update_active_witnesses()
{ try {
   share_type stake_target = _total_voting_stake.value / 2;
   share_type stake_tally = _witness_count_histogram_buffer[0];
   int witness_count = 0;
   while( stake_tally <= stake_target )
      stake_tally += _witness_count_histogram_buffer[++witness_count];

   auto wits = sort_votable_objects<witness_object>(std::max(witness_count, BTS_MIN_WITNESS_COUNT));
   shuffle_vector(wits);

   modify( get_global_properties(), [&]( global_property_object& gp ){
      gp.active_witnesses.clear();
      std::transform(wits.begin(), wits.end(),
                     std::inserter(gp.active_witnesses, gp.active_witnesses.end()),
                     [](const witness_object& w) {
         return w.id;
      });
      gp.witness_accounts.clear();
      std::transform(wits.begin(), wits.end(),
                     std::inserter(gp.witness_accounts, gp.witness_accounts.end()),
                     [](const witness_object& w) {
         return w.witness_account;
      });
   });
} FC_CAPTURE_AND_RETHROW() }

void database::update_active_delegates()
{ try {
   share_type stake_target = _total_voting_stake.value / 2;
   share_type stake_tally = _committee_count_histogram_buffer[0];
   int delegate_count = 0;
   while( stake_tally <= stake_target )
      stake_tally += _committee_count_histogram_buffer[++delegate_count];

   auto delegates = sort_votable_objects<delegate_object>(std::max(delegate_count, BTS_MIN_DELEGATE_COUNT));

   // Update genesis authorities
   if( !delegates.empty() )
      modify( get(account_id_type()), [&]( account_object& a ) {
         share_type total_votes = 0;
         map<account_id_type, share_type> weights;
         a.owner.weight_threshold = 0;
         a.owner.auths.clear();

         for( const delegate_object& del : delegates )
         {
            weights.emplace(del.delegate_account, _vote_tally_buffer[del.vote_id]);
            total_votes += _vote_tally_buffer[del.vote_id];
         }

         // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
         // then I want to keep the most significant 16 bits of what's left.
         int8_t bits_to_drop = std::max(int(64 - __builtin_clzll(total_votes.value)) - 16, 0);
         for( const auto& weight : weights )
         {
            // Ensure that everyone has at least one vote. Zero weights aren't allowed.
            uint16_t votes = std::max((weight.second.value >> bits_to_drop), int64_t(1) );
            a.owner.auths[weight.first] += votes;
            a.owner.weight_threshold += votes;
         }

         a.owner.weight_threshold /= 2;
         a.owner.weight_threshold += 1;
         a.active = a.owner;
      });
   modify( get_global_properties(), [&]( global_property_object& gp ) {
      gp.active_delegates.clear();
      std::transform(delegates.begin(), delegates.end(),
                     std::back_inserter(gp.active_delegates),
                     [](const delegate_object& d) { return d.id; });
   });
} FC_CAPTURE_AND_RETHROW() }

void database::update_global_dynamic_data( const signed_block& b )
{
   modify( dynamic_global_property_id_type(0)(*this), [&]( dynamic_global_property_object& dgp ){
      secret_hash_type::encoder enc;
      fc::raw::pack( enc, dgp.random );
      fc::raw::pack( enc, b.previous_secret );
      dgp.random = enc.result();
      dgp.head_block_number = b.block_num();
      dgp.head_block_id = b.id();
      dgp.time = b.timestamp;
      dgp.current_witness = b.witness;
   });
}

/**
 *  Removes the most recent block from the database and
 *  undoes any changes it made.
 */
void database::pop_block()
{ try {
   _pending_block_session.reset();
   _block_id_to_block.remove( _pending_block.previous );
   pop_undo();
   _pending_block.previous  = head_block_id();
   _pending_block.timestamp = head_block_time();
   _fork_db.pop_block();
} FC_CAPTURE_AND_RETHROW() }

void database::clear_pending()
{ try {
   _pending_block.transactions.clear();
   _pending_block_session.reset();
} FC_CAPTURE_AND_RETHROW() }

bool database::is_known_block( const block_id_type& id )const
{
   return _fork_db.is_known_block(id) || _block_id_to_block.find(id).valid();
}
/**
 *  Only return true *if* the transaction has not expired or been invalidated. If this
 *  method is called with a VERY old transaction we will return false, they should
 *  query things by blocks if they are that old.
 */
bool database::is_known_transaction( const transaction_id_type& id )const
{
  const auto& trx_idx = get_index_type<transaction_index>().indices().get<by_trx_id>();
  return trx_idx.find( id ) != trx_idx.end();
}

/**
 *  For each prime account, adjust the vote total object
 */
void database::update_vote_totals(const global_property_object& props)
{ try {
    const account_index& account_idx = get_index_type<account_index>();
    _total_voting_stake = 0;

    for( const account_object& account : account_idx.indices() )
    {
       if( true || account.is_prime() )
       {
          const auto& stats = account.statistics(*this);
          share_type voting_stake = stats.total_core_in_orders + stats.cashback_rewards
                   + get_balance(account.get_id(), asset_id_type()).amount;
          for( vote_id_type id : account.votes )
             _vote_tally_buffer[id] += voting_stake;

          if( account.num_witness <= props.parameters.maximum_witness_count )
             _witness_count_histogram_buffer[account.num_witness] += voting_stake;
          if( account.num_committee <= props.parameters.maximum_committee_count )
             _committee_count_histogram_buffer[account.num_committee] += voting_stake;

          _total_voting_stake += voting_stake;
       }
    }
} FC_CAPTURE_AND_RETHROW() }

/**
 *  Push block "may fail" in which case every partial change is unwound.  After
 *  push block is successful the block is appended to the chain database on disk.
 *
 *  @return true if we switched forks as a result of this push.
 */
bool database::push_block( const signed_block& new_block, uint32_t skip )
{ try {
   if( !(skip&skip_fork_db) )
   {
      auto new_head = _fork_db.push_block( new_block );
      //If the head block from the longest chain does not build off of the current head, we need to switch forks.
      if( new_head->data.previous != head_block_id() )
      {
         edump((new_head->data.previous));
         //If the newly pushed block is the same height as head, we get head back in new_head
         //Only switch forks if new_head is actually higher than head
         if( new_head->data.block_num() > head_block_num() )
         {
            auto branches = _fork_db.fetch_branch_from( new_head->data.id(), _pending_block.previous );
            for( auto item : branches.first )
               wdump( ("new")(item->id)(item->data.previous) );
            for( auto item : branches.second )
               wdump( ("old")(item->id)(item->data.previous) );

            // pop blocks until we hit the forked block
            while( head_block_id() != branches.second.back()->data.previous )
               pop_block();

            // push all blocks on the new fork
            for( auto ritr = branches.first.rbegin(); ritr != branches.first.rend(); ++ritr )
            {
                optional<fc::exception> except;
                try {
                   auto session = _undo_db.start_undo_session();
                   apply_block( (*ritr)->data, skip );
                   _block_id_to_block.store( new_block.id(), (*ritr)->data );
                   session.commit();
                }
                catch ( const fc::exception& e ) { except = e; }
                if( except )
                {
                   elog( "Encountered error when switching to a longer fork at id ${id}. Going back.",
                          ("id", (*ritr)->id) );
                   // remove the rest of branches.first from the fork_db, those blocks are invalid
                   while( ritr != branches.first.rend() )
                   {
                      _fork_db.remove( (*ritr)->data.id() );
                      ++ritr;
                   }
                   _fork_db.set_head( branches.second.front() );

                   // pop all blocks from the bad fork
                   while( head_block_id() != branches.second.back()->data.previous )
                      pop_block();

                   // restore all blocks from the good fork
                   for( auto ritr = branches.second.rbegin(); ritr != branches.second.rend(); ++ritr )
                   {
                      auto session = _undo_db.start_undo_session();
                      apply_block( (*ritr)->data, skip );
                      _block_id_to_block.store( new_block.id(), (*ritr)->data );
                      session.commit();
                   }
                   throw *except;
                }
            }
            return true;
         }
         else return false;
      }
   }

   // If there is a pending block session, then the database state is dirty with pending transactions.
   // Drop the pending session to reset the database to a clean head block state.
   // TODO: Preserve pending transactions, and re-apply any which weren't included in the new block.
   clear_pending();

   try {
      auto session = _undo_db.start_undo_session();
      apply_block( new_block, skip );
      _block_id_to_block.store( new_block.id(), new_block );
      session.commit();
   } catch ( const fc::exception& e ) {
      elog("Failed to push new block:\n${e}", ("e", e.to_detail_string()));
      _fork_db.remove(new_block.id());
      throw;
   }

   return false;
} FC_CAPTURE_AND_RETHROW( (new_block) ) }

/**
 *  Attempts to push the transaction into the pending queue
 *
 * When called to push a locally generated transaction, set the skip_block_size_check bit on the skip argument. This
 * will allow the transaction to be pushed even if it causes the pending block size to exceed the maximum block size.
 * Although the transaction will probably not propagate further now, as the peers are likely to have their pending
 * queues full as well, it will be kept in the queue to be propagated later when a new block flushes out the pending
 * queues.
 */
processed_transaction database::push_transaction( const signed_transaction& trx, uint32_t skip )
{
   //wdump((trx.digest())(trx.id()));
   // If this is the first transaction pushed after applying a block, start a new undo session.
   // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
   if( !_pending_block_session ) _pending_block_session = _undo_db.start_undo_session();
   auto session = _undo_db.start_undo_session();
   auto processed_trx = apply_transaction( trx, skip );
   _pending_block.transactions.push_back(processed_trx);

   FC_ASSERT( (skip & skip_block_size_check) ||
              fc::raw::pack_size(_pending_block) <= get_global_properties().parameters.maximum_block_size );

   // The transaction applied successfully. Merge its changes into the pending block session.
   session.merge();
   return processed_trx;
}

processed_transaction database::push_proposal(const proposal_object& proposal)
{
   transaction_evaluation_state eval_state(this);
   eval_state._is_proposed_trx = true;

   //Inject the approving authorities into the transaction eval state
   std::transform(proposal.required_active_approvals.begin(),
                  proposal.required_active_approvals.end(),
                  std::inserter(eval_state.approved_by, eval_state.approved_by.begin()),
                  []( account_id_type id ) {
                     return std::make_pair(id, authority::active);
                  });
   std::transform(proposal.required_owner_approvals.begin(),
                  proposal.required_owner_approvals.end(),
                  std::inserter(eval_state.approved_by, eval_state.approved_by.begin()),
                  []( account_id_type id ) {
                     return std::make_pair(id, authority::owner);
                  });

   ilog("Attempting to push proposal ${prop}", ("prop", proposal));
   idump((eval_state.approved_by));

   eval_state.operation_results.reserve(proposal.proposed_transaction.operations.size());
   processed_transaction ptrx(proposal.proposed_transaction);
   eval_state._trx = &ptrx;

   auto session = _undo_db.start_undo_session();
   for( auto& op : proposal.proposed_transaction.operations )
      eval_state.operation_results.emplace_back(apply_operation(eval_state, op));
   remove(proposal);
   session.merge();

   ptrx.operation_results = std::move(eval_state.operation_results);
   return ptrx;
}

processed_transaction database::apply_transaction( const signed_transaction& trx, uint32_t skip )
{ try {
   trx.validate();
   auto& trx_idx = get_mutable_index_type<transaction_index>();
   auto trx_id = trx.id();
   FC_ASSERT( (skip & skip_transaction_dupe_check) ||
              trx_idx.indices().get<by_trx_id>().find(trx_id) == trx_idx.indices().get<by_trx_id>().end() );
   transaction_evaluation_state eval_state(this, skip&skip_transaction_signatures );
   eval_state._trx = &trx;

   if( !(skip & skip_transaction_signatures) )
   {
      for( auto sig : trx.signatures )
      {
         //wdump((sig.first));
         //wdump((sig.first(*this)));
         FC_ASSERT( sig.first(*this).key_address() == fc::ecc::public_key( sig.second, trx.digest() ), "", ("sig.first",sig.first)("key_address",sig.first(*this).key_address())("addr", address(fc::ecc::public_key( sig.second, trx.digest() ))) );
      }
   }
   eval_state.operation_results.reserve( trx.operations.size() );

   const chain_parameters& chain_parameters = get_global_properties().parameters;
   processed_transaction ptrx(trx);
   _current_op_in_trx = 0;
   for( auto op : ptrx.operations )
   {
      eval_state.operation_results.emplace_back(apply_operation(eval_state, op));
      ++_current_op_in_trx;
   }
   ptrx.operation_results = std::move( eval_state.operation_results );

   //If we're skipping tapos check, but not expiration check, assume all transactions have maximum expiration time.
   fc::time_point_sec trx_expiration = _pending_block.timestamp + chain_parameters.maximum_time_until_expiration;
   if( !(skip & skip_tapos_check) )
   {
      //Check the TaPoS reference and expiration time
      //Remember that the TaPoS block number is abbreviated; it contains only the lower 16 bits.
      //Lookup TaPoS block summary by block number (remember block summary instances are the block numbers)
      const block_summary_object& tapos_block_summary
            = static_cast<const block_summary_object&>(get_index<block_summary_object>()
                                                       .get(block_summary_id_type((head_block_num() & ~0xffff)
                                                                                  + trx.ref_block_num)));
      //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
      FC_ASSERT( trx.ref_block_prefix == tapos_block_summary.block_id._hash[1] );
      trx_expiration = tapos_block_summary.timestamp + chain_parameters.block_interval*trx.relative_expiration.value;
      if( tapos_block_summary.timestamp == time_point_sec() )
         trx_expiration = now() + chain_parameters.block_interval*trx.relative_expiration.value;
      FC_ASSERT( _pending_block.timestamp <= trx_expiration ||
                 (trx.ref_block_prefix == 0 && trx.ref_block_num == 0 && head_block_num() < trx.relative_expiration)
                 , "", ("exp", trx_expiration) );
      FC_ASSERT( trx_expiration <= head_block_time() + chain_parameters.maximum_time_until_expiration
                 //Allow transactions through on block 1
                 || head_block_num() == 0 );
   }

   //Insert transaction into unique transactions database.
   if( !(skip & skip_transaction_dupe_check) )
   {
      create<transaction_object>([&](transaction_object& transaction) {
         transaction.expiration = trx_expiration;
         transaction.trx_id = trx.id();
         transaction.trx = trx;
      });
   }
   return ptrx;
} FC_CAPTURE_AND_RETHROW( (trx) ) }

operation_result database::apply_operation(transaction_evaluation_state& eval_state, const operation& op)
{
   assert("No registered evaluator for this operation." &&
          _operation_evaluators.size() > op.which() && _operation_evaluators[op.which()]);
   auto op_id = push_applied_operation( op );
   auto result =  _operation_evaluators[op.which()]->evaluate( eval_state, op, true );
   set_applied_operation_result( op_id, result );
   return result;
}
uint32_t database::push_applied_operation( const operation& op )
{
   _applied_ops.emplace_back(op);
   auto& oh = _applied_ops.back();
   oh.block_num    = _current_block_num;
   oh.trx_in_block = _current_trx_in_block;
   oh.op_in_trx    = _current_op_in_trx;
   oh.virtual_op   = _current_virtual_op++;
   return _applied_ops.size() - 1;
}
void database::set_applied_operation_result( uint32_t op_id, const operation_result& result )
{
   assert( op_id < _applied_ops.size() );
   _applied_ops[op_id].result = result;
}

const vector<operation_history_object>& database::get_applied_operations() const
{
   return _applied_ops;
}

const global_property_object& database::get_global_properties()const
{
   return get( global_property_id_type() );
}

const dynamic_global_property_object&database::get_dynamic_global_properties() const
{
   return get( dynamic_global_property_id_type() );
}
const fee_schedule_type&  database::current_fee_schedule()const
{
   return get_global_properties().parameters.current_fees;
}
time_point_sec database::head_block_time()const
{
   return get( dynamic_global_property_id_type() ).time;
}
uint32_t       database::head_block_num()const
{
   return get( dynamic_global_property_id_type() ).head_block_number;
}
block_id_type       database::head_block_id()const
{
   return get( dynamic_global_property_id_type() ).head_block_id;
}

block_id_type  database::get_block_id_for_num( uint32_t block_num )const
{ try {
   block_id_type lb; lb._hash[0] = htonl(block_num);
   auto itr = _block_id_to_block.lower_bound( lb );
   FC_ASSERT( itr.valid() && itr.key()._hash[0] == lb._hash[0] );
   return itr.key();
} FC_CAPTURE_AND_RETHROW( (block_num) ) }

optional<signed_block> database::fetch_block_by_id( const block_id_type& id )const
{
   auto b = _fork_db.fetch_block( id );
   if( !b )
      return _block_id_to_block.fetch_optional(id);
   return b->data;
}

optional<signed_block> database::fetch_block_by_number( uint32_t num )const
{
   auto results = _fork_db.fetch_block_by_number(num);
   if( results.size() == 1 )
      return results[0]->data;
   else
   {
      block_id_type lb; lb._hash[0] = htonl(num);
      auto itr = _block_id_to_block.lower_bound( lb );
      if( itr.valid() && itr.key()._hash[0] == lb._hash[0] )
         return itr.value();
   }
   return optional<signed_block>();
}

const signed_transaction& database::get_recent_transaction(const transaction_id_type& trx_id) const
{
   auto& index = get_index_type<transaction_index>().indices().get<by_trx_id>();
   auto itr = index.find(trx_id);
   FC_ASSERT(itr != index.end());
   return itr->trx;
}

const witness_object& database::validate_block_header(uint32_t skip, const signed_block& next_block)
{
   auto now = bts::chain::now();
   const auto& global_props = get_global_properties();
   FC_ASSERT( _pending_block.timestamp <= (now  + fc::seconds(1)), "", ("now",now)("pending",_pending_block.timestamp) );
   FC_ASSERT( _pending_block.previous == next_block.previous, "", ("pending.prev",_pending_block.previous)("next.prev",next_block.previous) );
   FC_ASSERT( _pending_block.timestamp <= next_block.timestamp, "", ("_pending_block.timestamp",_pending_block.timestamp)("next",next_block.timestamp)("blocknum",next_block.block_num()) );
   FC_ASSERT( _pending_block.timestamp.sec_since_epoch() % global_props.parameters.block_interval == 0 );
   const witness_object& witness = next_block.witness(*this);
   FC_ASSERT( secret_hash_type::hash(next_block.previous_secret) == witness.next_secret, "",
              ("previous_secret", next_block.previous_secret)("next_secret", witness.next_secret));
   if( !(skip&skip_delegate_signature) ) FC_ASSERT( next_block.validate_signee( witness.signing_key(*this).key() ) );
   auto expected_witness_num = (next_block.timestamp.sec_since_epoch() /
                                global_props.parameters.block_interval) % global_props.active_witnesses.size();
   FC_ASSERT( next_block.witness == global_props.active_witnesses[expected_witness_num] );

   return witness;
}

void database::update_signing_witness(const witness_object& signing_witness, const signed_block& new_block)
{
   const auto& core_asset = get( asset_id_type() );
   const auto& asset_data = core_asset.dynamic_asset_data_id(*this);
   const auto& gparams = get_global_properties().parameters;

   // Slowly pay out income based on configured witness pay rate
   fc::uint128 witness_pay( asset_data.accumulated_fees.value );
   witness_pay *= gparams.witness_pay_percent_of_accumulated;
   witness_pay /= BTS_WITNESS_PAY_PERCENT_PRECISION;

   auto burn = witness_pay;
   burn *= gparams.burn_percent_of_fee;
   burn /= gparams.witness_percent_of_fee;

   modify( asset_data, [&]( asset_dynamic_data_object& o ){
              o.accumulated_fees -= witness_pay.to_uint64();
              o.accumulated_fees -= burn.to_uint64();
              o.burned         += burn.to_uint64();
              o.current_supply -= burn.to_uint64();
           } );

   modify( signing_witness, [&]( witness_object& obj ){
           obj.last_secret = new_block.previous_secret;
           obj.next_secret = new_block.next_secret_hash;
           obj.accumulated_income += witness_pay.to_uint64();
           });
}

void database::update_pending_block(const signed_block& next_block, uint8_t current_block_interval)
{
   _pending_block.timestamp = next_block.timestamp + current_block_interval;
   _pending_block.previous = next_block.id();
   auto old_pending_trx = std::move(_pending_block.transactions);
   _pending_block.transactions.clear();
   for( auto old_trx : old_pending_trx )
      push_transaction( old_trx );
}

void database::perform_chain_maintenance(const signed_block& next_block, const global_property_object& global_props)
{
   _vote_tally_buffer.resize(global_props.next_available_vote_id);
   _witness_count_histogram_buffer.resize(global_props.parameters.maximum_witness_count);
   _committee_count_histogram_buffer.resize(global_props.parameters.maximum_committee_count);
   update_vote_totals(global_props);
   update_active_witnesses();
   update_active_delegates();
   _vote_tally_buffer.clear();
   _witness_count_histogram_buffer.clear();
   _committee_count_histogram_buffer.clear();

   const global_property_object& global_properties = get_global_properties();
   if( global_properties.pending_parameters )
      modify(get_global_properties(), [](global_property_object& p) {
         p.parameters = std::move(*p.pending_parameters);
         p.pending_parameters.reset();
      });

   auto new_block_interval = global_props.parameters.block_interval;

   // if block interval CHANGED during this block *THEN* we cannot simply
   // add the interval if we want to maintain the invariant that all timestamps are a multiple
   // of the interval.
   _pending_block.timestamp = next_block.timestamp + fc::seconds(new_block_interval);
   uint32_t r = _pending_block.timestamp.sec_since_epoch()%new_block_interval;
   if( !r )
   {
      _pending_block.timestamp -=  r;
      assert( (_pending_block.timestamp.sec_since_epoch() % new_block_interval)  == 0 );
   }

   auto next_maintenance_time = get<dynamic_global_property_object>(dynamic_global_property_id_type()).next_maintenance_time;
   auto maintenance_interval = get_global_properties().parameters.maintenance_interval;

   if( next_maintenance_time <= next_block.timestamp )
   {
      if( next_block.block_num() == 1 )
         next_maintenance_time = time_point_sec() +
               (((next_block.timestamp.sec_since_epoch() / maintenance_interval) + 1) * maintenance_interval);
      else
         next_maintenance_time += maintenance_interval;
      assert( next_maintenance_time > next_block.timestamp );
   }

   modify(get_dynamic_global_properties(), [next_maintenance_time](dynamic_global_property_object& d) {
      d.next_maintenance_time = next_maintenance_time;
   });
}

void database::create_block_summary(const signed_block& next_block)
{
   const auto& sum = create<block_summary_object>( [&](block_summary_object& p) {
         p.block_id = next_block.id();
         p.timestamp = next_block.timestamp;
   });
   FC_ASSERT( sum.id.instance() == next_block.block_num(), "", ("summary.id",sum.id)("next.block_num",next_block.block_num()) );
}

void database::clear_expired_transactions()
{
   //Look for expired transactions in the deduplication list, and remove them.
   //Transactions must have expired by at least two forking windows in order to be removed.
   auto& transaction_idx = static_cast<transaction_index&>(get_mutable_index(implementation_ids, impl_transaction_object_type));
   const auto& dedupe_index = transaction_idx.indices().get<by_expiration>();
   const auto& global_parameters = get_global_properties().parameters;
   auto forking_window_time = global_parameters.maximum_undo_history * global_parameters.block_interval;
   while( !dedupe_index.empty()
          && head_block_time() - dedupe_index.rbegin()->expiration >= fc::seconds(forking_window_time) )
      transaction_idx.remove(*dedupe_index.rbegin());
}

void database::clear_expired_proposals()
{
   const auto& proposal_expiration_index = get_index_type<proposal_index>().indices().get<by_expiration>();
   while( !proposal_expiration_index.empty() && proposal_expiration_index.begin()->expiration_time <= head_block_time() )
   {
      const proposal_object& proposal = *proposal_expiration_index.begin();
      processed_transaction result;
      try {
         if( proposal.is_authorized_to_execute(this) )
         {
            result = push_proposal(proposal);
            //TODO: Do something with result so plugins can process it.
            continue;
         }
      } catch( const fc::exception& e ) {
         elog("Failed to apply proposed transaction on its expiration. Deleting it.\n${proposal}\n${error}",
              ("proposal", proposal)("error", e.to_detail_string()));
      }
      remove(proposal);
   }
}

void database::clear_expired_orders()
{
   transaction_evaluation_state cancel_context(this, true);

   //Cancel expired limit orders
   auto& limit_index = get_index_type<limit_order_index>().indices().get<by_expiration>();
   while( !limit_index.empty() && limit_index.begin()->expiration <= head_block_time() )
   {
      const limit_order_object& order = *limit_index.begin();
      limit_order_cancel_operation canceler;
      canceler.fee_paying_account = order.seller;
      canceler.order = order.id;
      apply_operation(cancel_context, canceler);
   }

   //Cancel expired short orders
   auto& short_index = get_index_type<short_order_index>().indices().get<by_expiration>();
   while( !short_index.empty() && short_index.begin()->expiration <= head_block_time() )
   {
      const short_order_object& order = *short_index.begin();
      short_order_cancel_operation canceler;
      canceler.fee_paying_account = order.seller;
      canceler.order = order.id;
      apply_operation(cancel_context, canceler);
   }

   //Process expired force settlement orders
   //TODO: Do this on an asset-by-asset basis, and skip the current asset if it's maximally settled or has settlements disabled
   auto& settlement_index = get_index_type<force_settlement_index>().indices().get<by_expiration>();
   while( !settlement_index.empty() && settlement_index.begin()->settlement_date <= head_block_time() )
   {
      const force_settlement_object& order = *settlement_index.begin();
      auto order_id = order.id;
      const asset_bitasset_data_object mia = get(order.balance.asset_id).bitasset_data(*this);
      auto& pays = order.balance;
      auto receives = (order.balance * mia.current_feed.settlement_price);
      receives.amount = (fc::uint128_t(receives.amount.value) *
                         (BTS_100_PERCENT - mia.options.force_settlement_offset_percent) / BTS_100_PERCENT).to_uint64();
      assert(receives <= order.balance * mia.current_feed.settlement_price);

      price settlement_price = pays / receives;

      auto& call_index = get_index_type<call_order_index>().indices().get<by_collateral>();
      // Match against the least collateralized short until the settlement is finished
      while( !call_index.empty() && !(match(*call_index.begin(), order, settlement_price) & 2) );
      // Under no circumstances should the settlement not be finished now
      assert(find_object(order_id) == nullptr);
   }
}

void database::update_expired_feeds()
{
   auto& asset_idx = get_index_type<asset_bitasset_data_index>().indices().get<by_feed_expiration>();
   if( asset_idx.empty() ) return;
   while( asset_idx.begin()->feed_is_expired(head_block_time()) )
      modify(*asset_idx.begin(), [this](asset_bitasset_data_object& a) {
         a.update_median_feeds(head_block_time());
      });
}

/**
 *  This method dumps the state of the blockchain in a semi-human readable form for the
 *  purpose of tracking down funds and mismatches in currency allocation
 */
void database::debug_dump()
{
   const auto& db = *this;
   const asset_dynamic_data_object& core_asset_data = db.get_core_asset().dynamic_asset_data_id(db);

   const auto& balance_index = db.get_index_type<account_balance_index>().indices();
   const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
   map<asset_id_type,share_type> total_balances;
   map<asset_id_type,share_type> total_debts;
   share_type core_in_orders;
   share_type reported_core_in_orders;

   for( const account_balance_object& a : balance_index )
   {
      idump(("balance")(a));
      total_balances[a.asset_type] += a.balance;
   }
   for( const account_statistics_object& s : statistics_index )
   {
      idump(("statistics")(s));
      total_balances[asset_id_type()] += s.cashback_rewards;
      reported_core_in_orders += s.total_core_in_orders;
   }
   for( const limit_order_object& o : db.get_index_type<limit_order_index>().indices() )
   {
      idump(("limit_order")(o));
      auto for_sale = o.amount_for_sale();
      if( for_sale.asset_id == asset_id_type() ) core_in_orders += for_sale.amount;
      total_balances[for_sale.asset_id] += for_sale.amount;
   }
   for( const short_order_object& o : db.get_index_type<short_order_index>().indices() )
   {
      idump(("short_order")(o));
      auto col = o.get_collateral();
      if( col.asset_id == asset_id_type() ) core_in_orders += col.amount;
      total_balances[col.asset_id] += col.amount;
   }
   for( const call_order_object& o : db.get_index_type<call_order_index>().indices() )
   {
      idump(("call_order")(o));
      auto col = o.get_collateral();
      if( col.asset_id == asset_id_type() ) core_in_orders += col.amount;
      total_balances[col.asset_id] += col.amount;
      total_debts[o.get_debt().asset_id] += o.get_debt().amount;
   }
   for( const asset_object& asset_obj : db.get_index_type<asset_index>().indices() )
   {
      total_balances[asset_obj.id] += asset_obj.dynamic_asset_data_id(db).accumulated_fees;
      total_balances[asset_id_type()] += asset_obj.dynamic_asset_data_id(db).fee_pool;
   }
   for( const witness_object& witness_obj : db.get_index_type<simple_index<witness_object>>() )
   {
      //idump((witness_obj));
      total_balances[asset_id_type()] += witness_obj.accumulated_income;
   }
   if( total_balances[asset_id_type()].value != core_asset_data.current_supply.value )
   {
      edump( (total_balances[asset_id_type()].value)(core_asset_data.current_supply.value ));
   }
}

} } // namespace bts::chain
