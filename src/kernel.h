// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2013-2014 The Peershares developers
// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef PPCOIN_KERNEL_H
#define PPCOIN_KERNEL_H

#include "main.h"

// MODIFIER_INTERVAL: time to elapse before new modifier is computed
static const unsigned int MODIFIER_INTERVAL = 4 * 60 * 60;
extern unsigned int nModifierInterval;

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Protocol switch time of v0.3 kernel protocol
extern unsigned int nProtocolV03SwitchTime;
extern unsigned int nProtocolV03TestSwitchTime;

// Whether a given coinstake is subject to new v0.3 protocol
bool IsProtocolV03(unsigned int nTimeCoinStake);
// Whether a given block is subject to new v0.4 protocol
bool IsProtocolV04(unsigned int nTimeBlock);

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexCurrent, uint64& nStakeModifier, bool& fGeneratedStakeModifier);

// Check whether stake kernel meets hash target
// Sets hashProofOfStake on success return
bool CheckStakeKernelHash(unsigned int nBits, const CBlockHeader& blockFrom, unsigned int nTxPrevOffset, const CTransaction& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake, bool fPrintProofOfStake=false, std::string* failReason=NULL);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(CValidationState &state, const CTransaction& tx, unsigned int nBits, uint256& hashProofOfStake);

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64 nTimeBlock, int64 nTimeTx);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

// Expire old stake modifier cache
void CleanStakeModifierCache();

// Empty stake modifier cache
void ClearStakeModifierCache();

#endif // PPCOIN_KERNEL_H
