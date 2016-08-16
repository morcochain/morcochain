#include "blockmap.h"
#include "main.h"
#include "txdb.h"

#define BLOCK_MAP_KEPT_HEIGHT  10100

using namespace std;

void BlockMap::lazy_insert(const uint256& hash)
{
    LOCK(cs);
    setKnown.insert(hash);
}

size_t BlockMap::count(const uint256& hash) const
{
    LOCK(cs);
    return setKnown.count(hash);
}

CBlockIndex* BlockMap::operator[] (const uint256& hash)
{
    LOCK(cs);
    BlockIndexMap::iterator it = mapLoaded.find(hash);
    if (it != mapLoaded.end())
        return it->second;
    if (setKnown.count(hash))
        return load_at(hash);
    return NULL;
}

BlockIndexMap::iterator BlockMap::load(const uint256& hash)
{
    CDiskBlockIndex diskindex;
    if (!pblocktree->Read(make_pair('b', hash), diskindex))
        throw runtime_error("Failed to load block");

    return load(hash, diskindex);
}

BlockIndexMap::iterator BlockMap::load(const uint256& hash, const CDiskBlockIndex& diskindex)
{
    LOCK(cs);
    setKnown.insert(hash);
    mapPrev.erase(hash);

    pair<BlockIndexMap::iterator, bool> result = mapLoaded.insert(make_pair(hash, (CBlockIndex*)NULL));
    BlockIndexMap::iterator it = result.first;
    const bool& fInserted = result.second;

    if (fInserted)
    {
        it->second = new CBlockIndex;
        it->second->phashBlock = &it->first;
    }
    else
        *(it->second) = CBlockIndex();

    CBlockIndex* pindexNew = it->second;

    pindexNew->pprev          = CLazyBlockIndex(diskindex.hashPrev);
    pindexNew->nHeight        = diskindex.nHeight;
    pindexNew->nFile          = diskindex.nFile;
    pindexNew->nDataPos       = diskindex.nDataPos;
    pindexNew->nUndoPos       = diskindex.nUndoPos;
    pindexNew->nVersion       = diskindex.nVersion;
    pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
    pindexNew->nTime          = diskindex.nTime;
    pindexNew->nBits          = diskindex.nBits;
    pindexNew->nNonce         = diskindex.nNonce;
    pindexNew->nStatus        = diskindex.nStatus;
    pindexNew->nTx            = diskindex.nTx;

    // ppcoin related block index fields
    pindexNew->nMint          = diskindex.nMint;
    pindexNew->mapMoneySupply = diskindex.mapMoneySupply;
    pindexNew->nFlags         = diskindex.nFlags;
    pindexNew->nStakeModifier = diskindex.nStakeModifier;
    pindexNew->prevoutStake   = diskindex.prevoutStake;
    pindexNew->nStakeTime     = diskindex.nStakeTime;
    pindexNew->hashProofOfStake = diskindex.hashProofOfStake;

    // nubit related fields
    pindexNew->vote                   = diskindex.vote;
    pindexNew->vote.nCoinAgeDestroyed = diskindex.nCoinAgeDestroyed;
    pindexNew->vParkRateResult        = diskindex.vParkRateResult;
    pindexNew->nCoinAgeDestroyed      = diskindex.nCoinAgeDestroyed;
    pindexNew->vElectedCustodian      = diskindex.vElectedCustodian;
    pindexNew->nProtocolVersion       = diskindex.nProtocolVersion;
    pindexNew->mapVotedFee            = diskindex.mapVotedFee;

    // additional fields needed because we don't calculate them at each startup anymore
    pindexNew->nChainTrust            = diskindex.nChainTrust;
    pindexNew->nChainTx               = diskindex.nChainTx;
    pindexNew->nStakeModifierChecksum = diskindex.nStakeModifierChecksum;
    pindexNew->pprevElected           = CLazyBlockIndex(diskindex.hashPrevElected);

    uint256 hashNext;
    assert(pblocktree);
    if (pblocktree->ReadNext(hash, hashNext))
        pindexNew->pnext = CLazyBlockIndex(hashNext);

    return it;
}

BlockIndexMap::iterator BlockMap::find(const uint256& hash)
{
    LOCK(cs);
    BlockIndexMap::iterator it = mapLoaded.find(hash);
    if (it != mapLoaded.end())
        return it;
    if (setKnown.count(hash))
        return load(hash);
    return end();
}

std::pair<BlockIndexMap::iterator, bool> BlockMap::insert(const std::pair<uint256, CBlockIndex*> pair)
{
    LOCK(cs);
    setKnown.insert(pair.first);
    return mapLoaded.insert(pair);
}

void BlockMap::unload(const BlockIndexMap::iterator& it)
{
    LOCK(cs);

    CBlockIndex* pindex = it->second;

    if (pindex->nLocks)
        return;

    const uint256& hash = *pindex->phashBlock;

    const uint256& hashPrev = pindex->pprev.hash;
    if (hashPrev != 0)
        mapPrev[hash] = pindex->pprev.hash;

    pindex->phashBlock = NULL;
    delete pindex;
    pindex = NULL;
    mapLoaded.erase(it);
}

void BlockMap::unload(const uint256& hash)
{
    LOCK(cs);
    BlockIndexMap::iterator it = mapLoaded.find(hash);
    if (it == mapLoaded.end())
        return;

    unload(it);
}

void BlockMap::unload(CBlockIndex* pindex)
{
    unload(pindex->GetBlockHash());
}

void BlockMap::clear()
{
    LOCK(cs);
    for (BlockIndexMap::iterator it = mapLoaded.begin(); it != mapLoaded.end(); )
        unload(it++);
    setKnown.clear();
    mapPrev.clear();
}

void BlockMap::cleanup()
{
    LOCK2(cs, cs_cleanup);
    size_t nLoadedBefore = mapLoaded.size();
    for (BlockIndexMap::iterator it = mapLoaded.begin(); it != mapLoaded.end(); )
    {
        CBlockIndex* pindex = it->second;
        bool fUnload = true;

        if (pindex->nHeight >= pindexBest->nHeight - BLOCK_MAP_KEPT_HEIGHT ||
                pindex == pindexGenesisBlock)
            fUnload = false;

        if (fUnload)
            unload(it++);
        else
            ++it;
    }
    int nRemoved = nLoadedBefore - mapLoaded.size();
    if (nRemoved > 1000)
        printf("BlockMap cleanup: %"PRIszd" removed\n", nLoadedBefore - mapLoaded.size());
}

void BlockMap::assert_loaded(const CBlockIndex* pindex) const
{
    LOCK(cs);
    assert(pindex);
    assert(pindex->phashBlock);
    assert(count(*pindex->phashBlock));
    BlockIndexMap::const_iterator it = mapLoaded.find(*pindex->phashBlock);
    assert(it != mapLoaded.end());
    assert(pindex == it->second);
}

const uint256* BlockMap::prev(const uint256& hash) const
{
    LOCK(cs);
    {
        BlockIndexMap::const_iterator it = mapLoaded.find(hash);
        if (it != mapLoaded.end())
        {
            const uint256& hash = it->second->pprev.hash;
            if (hash == 0)
                return NULL;
            else
                return &hash;
        }
    }
    {
        PrevMap::const_iterator it = mapPrev.find(hash);
        if (it != mapPrev.end())
            return &(it->second);
    }
    return NULL;
}

void BlockMap::lock(const CBlockIndex* pindex)
{
    if (!pindex)
        return;
    LOCK(cs);
    pindex->nLocks++;
}

void BlockMap::unlock(const CBlockIndex* pindex)
{
    if (!pindex)
        return;
    LOCK(cs);
    pindex->nLocks--;
}

void CLazyBlockIndex::operator=(const CBlockIndex* value)
{
    if (value)
        hash = *value->phashBlock;
    else
        hash = 0;
}
