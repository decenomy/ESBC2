// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "spork.h"
#include "tinyformat.h"

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nSize)
{
    if (nSize > 0)
        nSatoshisPerK = nFeePaid * 1000 / nSize;
    else
        nSatoshisPerK = 0;
}

CAmount CFeeRate::GetFee(size_t nSize) const
{
    CAmount nFeePerK = nSatoshisPerK;
    if (ActiveProtocol() >= CONSENSUS_FORK_PROTO && nFeePerK == 10000)
        nFeePerK = 250000;

    CAmount nFee = nFeePerK * nSize / 1000;

    if (nFee == 0 && nFeePerK > 0)
        nFee = nFeePerK;

    return nFee;
}

std::string CFeeRate::ToString() const
{
    return strprintf("%d.%08d esbcoin/kB", nSatoshisPerK / COIN, nSatoshisPerK % COIN);
}
