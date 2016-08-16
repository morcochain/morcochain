#include <boost/test/unit_test.hpp>
#include <algorithm>

#include "blockmap.h"
#include "main.h"
#include "txdb.h"

using namespace std;


BOOST_AUTO_TEST_SUITE(blockmap_tests)

BOOST_AUTO_TEST_CASE(blockmap_returns_the_same_pointer_for_the_same_hash)
{
    mapBlockIndex.clear();

    CDiskBlockIndex disk;
    disk.nNonce = 1;
    uint256 hash = disk.GetBlockHash();

    BOOST_CHECK(pblocktree->WriteBlockIndex(disk));
    CDiskBlockIndex diskIndex;
    BOOST_CHECK(pblocktree->Read(make_pair('b', hash), diskIndex));

    mapBlockIndex.lazy_insert(hash);

    CBlockIndex* pindex1 = mapBlockIndex[hash];
    CBlockIndex* pindex2 = mapBlockIndex[hash];

    BOOST_CHECK_EQUAL(pindex1, pindex2);

    mapBlockIndex.clear();
    pblocktree->Erase(make_pair('b', hash));
}

BOOST_AUTO_TEST_CASE(blockmap_on_unknown_hash)
{
    mapBlockIndex.clear();

    CDiskBlockIndex disk;
    disk.nNonce = 1;
    uint256 hash = disk.GetBlockHash();

    BOOST_CHECK(mapBlockIndex[hash] == NULL);
    BOOST_CHECK(mapBlockIndex[hash] ? false : true);
    BOOST_CHECK(!mapBlockIndex[hash]);

    mapBlockIndex.clear();
}

BOOST_AUTO_TEST_CASE(pprev_pointer_works)
{
    mapBlockIndex.clear();

    CDiskBlockIndex disk1;
    CDiskBlockIndex disk2;

    disk1.nNonce = 1;
    disk2.nNonce = 2;

    disk2.hashPrev = disk1.GetBlockHash();

    uint256 hash1 = disk1.GetBlockHash();
    uint256 hash2 = disk2.GetBlockHash();

    CDiskBlockIndex diskIndex;

    BOOST_CHECK(pblocktree->WriteBlockIndex(disk1));
    BOOST_CHECK(pblocktree->Read(make_pair('b', hash1), diskIndex));

    BOOST_CHECK(pblocktree->WriteBlockIndex(disk2));
    BOOST_CHECK(pblocktree->Read(make_pair('b', hash2), diskIndex));
    BOOST_CHECK_EQUAL(hash1.ToString(), diskIndex.hashPrev.ToString());

    mapBlockIndex.lazy_insert(hash1);
    mapBlockIndex.lazy_insert(hash2);

    BOOST_CHECK_EQUAL(mapBlockIndex[hash1], mapBlockIndex[hash2]->pprev);

    mapBlockIndex.clear();
    pblocktree->Erase(make_pair('b', hash1));
    pblocktree->Erase(make_pair('b', hash2));
}

BOOST_AUTO_TEST_SUITE_END()
