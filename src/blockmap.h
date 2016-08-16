#ifndef BLOCKMAP_H
#define BLOCKMAP_H

#include <set>
#include <map>
#include "uint256.h"
#include "sync.h"

class CBlockIndex;
class CDiskBlockIndex;
class CBlockTreeDB;

typedef std::map<uint256, CBlockIndex*> BlockIndexMap;
typedef std::map<uint256, uint256> PrevMap;

// BlockMap behaves mostly like std::map<uint256, CBlockIndex*> but it loads the block indexes on demand
class BlockMap
{
private:
    // The currenly loaded blocks
    BlockIndexMap mapLoaded;

    // The hash of all the existing block indexes
    std::set<uint256> setKnown;

    // The prev hash of each block hash that's not currently loaded
    // Enables finding a previous block hash without loading all the intermediate indexes
    PrevMap mapPrev;

    // All operations are protected by this mutex
    mutable CCriticalSection cs;

public:
    // The block database from which indexes are loaded
    CBlockTreeDB *pblocktree;

    // Cleanup can be prevented by locking this mutex
    mutable CCriticalSection cs_cleanup;

    BlockMap(CBlockTreeDB *pblocktree = NULL) : pblocktree(pblocktree)
    {
    }

    // Add a hash as known but do not actually load it
    void lazy_insert(const uint256& hash);

    // Insert a new CBlockIndex
    std::pair<BlockIndexMap::iterator, bool> insert(const std::pair<uint256, CBlockIndex*> pair);

    // Find a loaded block index, or load it
    BlockIndexMap::iterator find(const uint256& hash);
    CBlockIndex* operator[] (const uint256& hash);

    // The find() method return end() if the hash is not known
    BlockIndexMap::iterator end()
    {
        LOCK(cs);
        return mapLoaded.end();
    }
    BlockIndexMap::const_iterator end() const
    {
        LOCK(cs);
        return mapLoaded.end();
    }

    // Load a block index from the database
    BlockIndexMap::iterator load(const uint256& hash);

    // Add a block index from a database record already loaded
    BlockIndexMap::iterator load(const uint256& hash, const CDiskBlockIndex& diskindex);

    // Same as load but returns the loaded CBlockIndex* instead of the iterator
    CBlockIndex* load_at(const uint256& hash)
    {
        LOCK(cs);
        return load(hash)->second;
    }
    CBlockIndex* load_at(const uint256& hash, const CDiskBlockIndex& diskindex)
    {
        LOCK(cs);
        return load(hash, diskindex)->second;
    }

    // Remove a block index from memory
    void unload(const BlockIndexMap::iterator& it);
    void unload(const uint256& hash);
    void unload(CBlockIndex* pindex);

    // Assert a block index is currently loaded in memory
    void assert_loaded(const CBlockIndex* pindex) const;

    // Return 1 if the hash is known (a block index exists either in memory or in the database)
    size_t count(const uint256& hash) const;

    // Return the total number of block indexes (loaded or not)
    size_t size() const
    {
        LOCK(cs);
        return setKnown.size();
    }

    // Iterator to the first known hash
    std::set<uint256>::const_iterator known_begin()
    {
        return setKnown.begin();
    }

    // Iterator to past-the-end of the known hashes
    std::set<uint256>::const_iterator known_end()
    {
        return setKnown.end();
    }

    // Return the prev hash of a block hash without loading the block index
    const uint256* prev(const uint256& hash) const;

    // Prevent a block index from being unloaded during the cleanup
    void lock(const CBlockIndex* pindex);
    void unlock(const CBlockIndex* pindex);

    // Remove some block indexes from memory
    void cleanup();

    // Remove all the block indexes from memory and clear the other data
    void clear();
};

extern BlockMap mapBlockIndex;

// A CLazyBlockIndex behaves like a CBlockIndex* but it only loads its data on demand
class CLazyBlockIndex
{
public:
    uint256 hash;

    CLazyBlockIndex(const uint256 hash) : hash(hash)
    {
    }

    CLazyBlockIndex() : hash(0)
    {
    }

    CBlockIndex *GetBlockIndex() const
    {
        if (hash == 0)
            return NULL;
        return mapBlockIndex[hash];
    }

    operator CBlockIndex*() const
    {
        return GetBlockIndex();
    }

    CBlockIndex* operator->() const
    {
        return GetBlockIndex();
    }

    void operator=(const CBlockIndex* value);
};

// A CLockedBlockIndex behaves like a CBlockIndex* but while it exists it prevents the block index from being unloaded
class CLockedBlockIndex
{
    CBlockIndex* pindex;

public:
    CLockedBlockIndex(CBlockIndex* pindex) : pindex(pindex)
    {
        mapBlockIndex.lock(pindex);
    }

    ~CLockedBlockIndex()
    {
        mapBlockIndex.unlock(pindex);
    }

    operator CBlockIndex*() const
    {
        return pindex;
    }

    CBlockIndex* operator->() const
    {
        return pindex;
    }

    void operator=(CBlockIndex* value)
    {
        mapBlockIndex.unlock(pindex);
        pindex = value;
        mapBlockIndex.lock(pindex);
    }
};

#endif
