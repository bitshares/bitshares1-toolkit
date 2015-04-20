
BitAssets 3.0 characteristics
-----------------------------

Following is copy-pasted from `bytemaster` post at https://bitsharestalk.org/index.php/topic,15775.0.html

- #1 No explicit short sell price limit
- #2 No pre-set expiration on short positions.
- #3 Any time someone with USD is unhappy with the current internal market price, they can request settlement at the 99% of feed (a 1% fee) in X days, where X is more than 24 hours.
- #4 On settlement day the least collateralized short position is forced to settle at 99% of feed (a 1% profit)
- #5 At any time entire market can be settled at the feed price given 30 day notice to be executed only in the event that all USD holders are unwilling to sell anywhere near a fair price. (black swan protection), this settlement can be canceled if the market returns to normal voluntarily.
- #6 200% collateral

In effect a short position is a "loan" that is callable based upon price or X day notice.

Expected Outcome:
1) The price feed should be irrelevant unless the current market price is below 99% the expected price feed in X days
2) No shorts would dare sell down the price much below the expected feed for long because longs can force settlement to call their bluff.
3) The market has a graceful escape valve where all parties have ample time to voluntarily settle to avoid being forced settled. 
4) Well collateralized shorts never have to cover
5) USD holders are guaranteed liquidity at 99% of the feed within X days (potentially as little as 24 hours).

All that is required is the threat of forced settlement to keep the market fair, by charging a fee for forced settlement longs that demand liquidity compensate the shorts who were forced out.  Over all the market rules are simpler, liquidity is much greater, and all parties are far more protected than they are today.

BitAssets 3.0 partial cover vs fractional cover
-----------------------------------------------

The main purpose of this article is to address whether BitAssets 3.0 should *cover* positions or *release* positions.  In particular,
suppose we have the following book when settlement of 50 USD is performed at a settlement price of 0.02 USD / BTS:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash         |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ------------ |
| Alice         | 100 USD       | 15000 BTS      | 10000 BTS           | 300%             | 0 BTS        |
| Bob           |  60 USD       |  6750 BTS      |  3750 BTS           | 225%             | 0 BTS        |
| Charlie       |  40 USD       |  4200 BTS      |  2200 BTS           | 210%             | 0 BTS        |

Then *fractional cover assignment algorithm* settles by selecting Charlie as the least collateralized short and cashing him out; then selecting
Bob as the next most collateralized short and partially cashing him out, resulting in the following book:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash         | Long redeemed  | Paid to long   |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ------------ | -------------- | -------------- |
| Alice         | 100 USD       | 15000 BTS      | 10000 BTS           | 300%             | 0 BTS        |  0 USD         |    0 BTS       |
| Bob           |  50 USD       |  5625 BTS      |  3125 BTS           | 225%             | 625 BTS      | 10 USD         |  500 BTS       |
| Charlie       |   0 USD       |     0 BTS      |  (n/a)              | (n/a)            | 2200 BTS     | 40 USD         | 2000 BTS       |

Partial cover assignment
------------------------

We begin with same starting book as before:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash         |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ------------ |
| Alice         | 100 USD       | 15000 BTS      | 10000 BTS           | 300%             | 0 BTS        |
| Bob           |  60 USD       |  6750 BTS      |  3750 BTS           | 225%             | 0 BTS        |
| Charlie       |  40 USD       |  4200 BTS      |  2200 BTS           | 210%             | 0 BTS        |

The *partial cover assignment algorithm* settles by selecting Charlie and partially covering him until 225%, then partially covering both Bob and Charlie
equally.

The first question we must answer, then, is how many BTS worth of USD
must Charlie redeem in order to have a backing of 225%?

This is a simple algebra problem.
Let `C` be the collateral ratio of the worst collateralized short, `x`
the number of BTS from the short collateralized short that are being
assigned, `d` be the debt of the worst-collateralized short in BTS,
we can compute the new backing after `x` shares are assigned as follows:

    r = (C - x) / (d - x)
    r * (d - x) = C - x
    r * d = C - x + r * x
    r * d - C = x * (r - 1)
    x = (r * d - C) / (r - 1)

With `r = 2.25`, `d = 2000`, `C = 4200`, we obtain `x = 240`.  Buying
out 240 BTS gives us this book:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash         | Long redeemed  | Paid to long   |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ------------ | -------------- | -------------- |
| Alice         |   100 USD     | 15000 BTS      | 10000 BTS           | 300%             | 0 BTS        |  0 USD         |    0 BTS       |
| Bob           |    60 USD     |  6750 BTS      |  3750 BTS           | 225%             | 0 BTS        |  0 USD         |    0 BTS       |
| Charlie       | 35.20 USD     |  3960 BTS      |  2200 BTS           | 225%             | 0 BTS        |  4.80 USD      |  240 BTS       |

We still $45.20 BitUSD needing to be redeemed.  Since Bob and Charlie's
positions now have equal debt-to-collateral ratio, they are fungible
against each other.  Thus, temporarily, for accounting purposes, let's
pretend Bob and Charlie are pooling their assets and liabilities into
a joint venture.  For temporary bookkeeping, we issue them 1 BC_SHARE
for each BTS worth of equity they bring to the joint venture:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash             |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ---------------- |
| Alice         |   100 USD     | 15000 BTS      | 10000 BTS           | 300%             | 0 BTS            |
| BC            | 95.20 USD     | 10710 BTS      |  5950 BTS           | 225%             | 0 BTS            |
| Bob           | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 3750 BC_SHARES   |
| Charlie       | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 2200 BC_SHARES   |

Until the USD being redeemed is fully assigned, Bob and Charlie no
longer act independently, rather they only cash out through their
joint venture.

We can proceed using the same algorithm.  With `r = 3.0`, `d = 4760`
and `C = 10710`, we find that after redeeming 1785 BTS for 35.70 BitUSD,
BC reaches Alice level backing:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash             | Long redeemed | Paid to long |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ---------------- | ------------- | ------------ |
| Alice         |   100 USD     | 15000 BTS      | 10000 BTS           | 300%             | 0 BTS            | 0 USD         |       0 BTS  |
| BC            | 59.50 USD     |  8925 BTS      |  5950 BTS           | 300%             | 0 BTS            | 35.70 USD     |    1785 BTS  |
| Bob           | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 3750 BC_SHARES   | (n/a)         | (n/a)        |
| Charlie       | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 2200 BC_SHARES   | (n/a)         | (n/a)        |

So we combine Alice with BC into ABC joint venture, printing 10,000 new
ABC_SHARES for Alice and converting Bob and Charlie's BC_SHARES into
ABC_SHARES at a 1:1 ratio:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash             |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ---------------- |
| ABC           | 159.50 USD    | 23925 BTS      | 15950 BTS           | 300%             | 0 BTS            |
| Alice         | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 10000 ABC_SHARES |
| Bob           | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 3750 ABC_SHARES  |
| Charlie       | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 2200 ABC_SHARES  |

The remaining $9.50 being redeemed is assigned to ABC:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash             | Long redeemed | Paid to long |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ---------------- | ------------- | ------------ |
| ABC           | 150.00 USD    | 23450 BTS      | 15950 BTS           | 312.7%           | 0 BTS            | 9.50 USD      | 475 BTS      |
| Alice         | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 10000 ABC_SHARES | (n/a)         | (n/a)        |
| Bob           | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 3125 ABC_SHARES  | (n/a)         | (n/a)        |
| Charlie       | (n/a)         |  (n/a)         |  (n/a)              | (n/a)            | 2200 ABC_SHARES  | (n/a)         | (n/a)        |

Finally we must re-assign the debt and collateral according to number of shares:

| Owner         | Debt          | Collateral     | Short-side equity   | Backing          | Cash             | Long redeemed   | Paid to long |
| ------------- | ------------- | -------------- | ------------------- | ---------------- | ---------------- | --------------- | ------------ |
| Alice         | 94.0439 USD   | 14702.19 BTS   | 10000 BTS           | 312.7%           | 0 BTS            |    5.9561 USD   |   297.81 BTS |
| Bob           | 35.2665 USD   | 5513.32 BTS    |  3750 BTS           | 312.7%           | 0 BTS            |   24.7335 USD   |  1236.68 BTS |
| Charlie       | 20.6897 USD   | 3234.48 BTS    |  2200 BTS           | 312.7%           | 0 BTS            |   19.3103 USD   |   965.52 BTS |

Partial cover assignment is better
----------------------------------

Partial cover assignment is superior to fractional cover assignment.

- It doesn't let under-collateralized shorts "jump the queue" and
be prioritized for redemption.

- All shorts who want to stay in can do so; the system automatically
boosts collateral levels of highest leverage shorts to exactly the
least collateral required to stay short (instead of requiring users
to run bots that try to manually approximate this approach).

- No short ever needs to post additional collateral to achieve this
collateral level boosting, it takes place entirely through paying down
debt.

- Shorts have no expiration just like longs have no expiration.

- Shorts can *only* exit the system by obtaining long shares on the
open market and covering.  When there's a shortage of long shares,
redemption doesn't pick and choose winners.

- Partial cover assignment only exits the minimum amount of collateral
necessary to honor the redemption request; the rest continues to
secure the system.  After fractional cover, we had 150 USD backed by
20,625 BTS for a backing of 275%.  After partial cover, we had 150 USD
backed by 23,450 BTS for a backing of 312.7%.  The latter is clearly
superior.
