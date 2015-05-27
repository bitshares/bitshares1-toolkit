
Witness scheduler RNG
---------------------

The witness scheduler RNG is a random number generator which uses the
blockchain random number generator state as its seed.

The witness scheduler RNG creates an infinite stream of random bytes
by computing `sha256( sha256( seed ) + little_endian_64bit(i) )`, increasing
`i` from 0 to 1 to 2, etc.  The RNG only runs during a scheduling block,
and `i` starts from `0` in each scheduling block (relying on different
seeds to produce different results).

This infinite stream of random bytes is equivalent to an infinite
stream of random bits in little bit-endian order.  Given a bound `B`,
the bitstream can be used to produce a random number uniformly
distributed in the range `[0, B)` using a sample-and-reject algorithm:

- Let `n` be the smallest integer such that `2^n >= B`.
- Let `x` be the next `n` bits from the bitstream, interpreted as an integer in little bit-endian order.
- If `x <= B`, return `x`.  Otherwise, throw `x` away and repeat.

The worst-case running time is unbounded, but each iteration has a
termination probability greater than one half.  Thus the average-case
running time is `2` iterations, and and a running time of more than `N`
iterations will occur (on average) at most once every `2^N`
RNG queries (assuming a worst-case choice of e.g. `B = 2^63+1` for all
queries).  Since each RNG query schedules a witness, the query rate
is (over the long term) equal to the block production rate (although
in practice many queries are all performed at once in scheduling
blocks).  So while it is, in theory, possible for the algorithm to
require more than 1000 iterations, in practice this will occur on average
only once every `2^1000` blocks (again assuming all queries have
worst-case `B`).

Witness scheduler RNG comparison
--------------------------------

The BitShares 0.x witness scheduler RNG interprets the 256-bit `sha256`
value as a large integer and uses a modulus operation.  This requires
an `sha256` operation and a large-integer modulus operation for every
random number produced; both of which are relatively expensive
operations.

Furthermore, the output of the BitShares 0.x RNG is slightly biased.
If the bound is not a power of two, then some output values will be
produced with distinct probabilities.  Admittedly, the difference is
tiny (approximately `B / 2^256`).  The sample-and-reject algorithm
is clearly superior since it has no bias; it is truly uniform.
