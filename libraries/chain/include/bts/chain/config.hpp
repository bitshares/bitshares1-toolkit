#pragma once

#define BTS_SYMBOL "BTS"
#define BTS_ADDRESS_PREFIX "BTS"
#define BTS_MAX_SYMBOL_NAME_LENGTH 16
#define BTS_MAX_ASSET_NAME_LENGTH 127
#define BTS_MAX_SHARE_SUPPLY int64_t(1000000000000ll)
#define BTS_MAX_PAY_RATE 10000 /* 100% */
#define BTS_MAX_SIG_CHECK_DEPTH 2
#define BTS_MIN_DELEGATE_COUNT 10
/**
 * Don't allow the delegates to publish a limit that would
 * make the network unable to operate.
 */
#define BTS_MIN_TRANSACTION_SIZE_LIMIT 1024
#define BTS_MAX_BLOCK_INTERVAL  30 /* seconds */

#define BTS_DEFAULT_BLOCK_INTERVAL  5 /* seconds */
#define BTS_DEFAULT_MAX_TRANSACTION_SIZE 2048
#define BTS_DEFAULT_MAX_BLOCK_SIZE  (BTS_DEFAULT_MAX_TRANSACTION_SIZE*BTS_DEFAULT_BLOCK_INTERVAL*10)
#define BTS_DEFAULT_MAX_TIME_UNTIL_EXPIRATION (60*60*24) // seconds,  aka: 1 day
#define BTS_DEFAULT_MAINTENANCE_INTERVAL  (60*60*24) // seconds, aka: 1 day
#define BTS_DEFAULT_MAX_UNDO_HISTORY 1024

#define BTS_MIN_BLOCK_SIZE_LIMIT (BTS_MIN_TRANSACTION_SIZE_LIMIT*5) // 5 transactions per block
#define BTS_MIN_TRANSACTION_EXPIRATION_LIMIT (BTS_MAX_BLOCK_INTERVAL * 5) // 5 transactions per block
#define BTS_BLOCKCHAIN_MAX_SHARES                          (1000*1000*int64_t(1000)*1000*int64_t(1000))
#define BTS_BLOCKCHAIN_PRECISION                           100000
#define BTS_BLOCKCHAIN_PRECISION_DIGITS                    5
#define BTS_INITIAL_SUPPLY                                 BTS_BLOCKCHAIN_MAX_SHARES
#define BTS_DEFAULT_TRANSFER_FEE                           (1*BTS_BLOCKCHAIN_PRECISION)
#define BTS_MAX_INSTANCE_ID                                (uint64_t(-1)>>16)
/** NOTE: making this a power of 2 (say 2^15) would greatly accelerate fee calcs */
#define BTS_MAX_MARKET_FEE_PERCENT                         10000
#define BTS_MAX_FEED_PRODUCERS                             200
#define BTS_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES        10

#define BTS_MIN_COLLATERAL_RATIO                   1001  // lower than this could result in divide by 0
#define BTS_MAX_COLLATERAL_RATIO                   32000 // higher than this is unnecessary and may exceed int16 storage
#define BTS_DEFAULT_INITIAL_COLLATERAL_RATIO       2000
#define BTS_DEFAULT_MAINTENANCE_COLLATERAL_RATIO   1750
#define BTS_DEFAULT_MARGIN_PERIOD_SEC              (30*60*60*24)

#define BTS_DEFAULT_WITNESS_PAY (50*BTS_BLOCKCHAIN_PRECISION)

#define BTS_DEFAULT_MAX_PROPOSAL_LIFETIME_SEC            (60*60*24*7*4) // Four weeks
#define BTS_DEFAULT_GENESIS_PROPOSAL_REVIEW_PERIOD_SEC   (60*60*24*7*2) // Two weeks
#define BTS_DEFAULT_WITNESS_PERCENT                      (10000/100) // 1% 
#define BTS_DEFAULT_MAX_BULK_DISCOUNT_PERCENT            (10000/2) // 50% 
#define BTS_DEFAULT_BULK_DISCOUNT_THRESHOLD_MIN          ( BTS_BLOCKCHAIN_PRECISION*int64_t(1000) )
#define BTS_DEFAULT_BULK_DISCOUNT_THRESHOLD_MAX          ( BTS_DEFAULT_BULK_DISCOUNT_THRESHOLD_MIN*int64_t(100) )


