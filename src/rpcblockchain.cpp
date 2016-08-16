// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include <boost/lexical_cast.hpp>

using namespace json_spirit;
using namespace std;

void ScriptPubKeyToJSON(const CScript& scriptPubKey, Object& out, unsigned char cUnit);

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (pindexBest == NULL)
            return 1.0;
        else
            blockindex = GetLastBlockIndex(pindexBest, false);
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

void TxToJSON(const CTransaction& tx, Object& txdata)
{
    // tx data
    txdata.push_back(Pair("txid", tx.GetHash().ToString().c_str()));
    txdata.push_back(Pair("version", (int)tx.nVersion));
    txdata.push_back(Pair("locktime", (int)tx.nLockTime));
    txdata.push_back(Pair("is_coinbase", tx.IsCoinBase()));
    txdata.push_back(Pair("is_coinstake", tx.IsCoinStake()));

    // add inputs
    Array vins;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        Object vin;

        if (txin.prevout.IsNull())
        {
            vin.push_back(Pair("coinbase", HexStr(txin.scriptSig).c_str()));
        }
        else
        {
            vin.push_back(Pair("txid", txin.prevout.hash.ToString().c_str()));
            vin.push_back(Pair("vout", (int)txin.prevout.n));
        }

        vin.push_back(Pair("sequence", (boost::uint64_t)txin.nSequence));

        vins.push_back(vin);
    }
    txdata.push_back(Pair("vin", vins));

    // add outputs
    Array vouts;
    int n = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        Object vout;

        std::vector<CTxDestination> addresses;
        txnouttype txtype;
        int nRequired;

        vout.push_back(Pair("value", ValueFromAmount(txout.nValue)));
        vout.push_back(Pair("n", n));

        Object scriptpubkey;

        scriptpubkey.push_back(Pair("asm", txout.scriptPubKey.ToString()));
        scriptpubkey.push_back(Pair("hex", HexStr(txout.scriptPubKey.begin(), txout.scriptPubKey.end())));

        if (ExtractDestinations(txout.scriptPubKey, txtype, addresses, nRequired))
        {
            scriptpubkey.push_back(Pair("type", GetTxnOutputType(txtype)));
            scriptpubkey.push_back(Pair("reqSig", nRequired));

            Array addrs;
            BOOST_FOREACH(const CTxDestination& addr, addresses)
                addrs.push_back(CBitcoinAddress(addr, tx.cUnit).ToString());
            scriptpubkey.push_back(Pair("addresses", addrs));
        }
        else
        {
            scriptpubkey.push_back(Pair("type", GetTxnOutputType(TX_NONSTANDARD)));
        }

        vout.push_back(Pair("scriptPubKey",scriptpubkey));

        vouts.push_back(vout);
        n++;
    }
    txdata.push_back(Pair("vout", vouts));
}


Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fTxInfo, bool fTxDetails)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);
    result.push_back(Pair("confirmations", (int)txGen.GetDepthInMainChain()));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    result.push_back(Pair("time", DateTimeStrFormat(block.GetBlockTime())));
    result.push_back(Pair("nonce", (boost::uint64_t)block.nNonce));
    result.push_back(Pair("bits", HexBits(block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("mint", ValueFromAmount(blockindex->nMint)));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    if (blockindex->pnext)
        result.push_back(Pair("nextblockhash", blockindex->pnext->GetBlockHash().GetHex()));

    result.push_back(Pair("flags", strprintf("%s%s", blockindex->IsProofOfStake()? "proof-of-stake" : "proof-of-work", blockindex->GeneratedStakeModifier()? " stake-modifier": "")));
    result.push_back(Pair("proofhash", blockindex->IsProofOfStake()? blockindex->hashProofOfStake.GetHex() : blockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016"PRI64x, blockindex->nStakeModifier)));
    result.push_back(Pair("modifierchecksum", strprintf("%08x", blockindex->nStakeModifierChecksum)));

    Array txinfo;
    BOOST_FOREACH (const CTransaction& tx, block.vtx)
    {
        if (fTxInfo && !fTxDetails)
        {
            txinfo.push_back(tx.ToStringShort());
            txinfo.push_back(DateTimeStrFormat(tx.nTime));
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
                txinfo.push_back(txin.ToStringShort());
            BOOST_FOREACH(const CTxOut& txout, tx.vout)
                txinfo.push_back(txout.ToStringShort());
        }
        else if (fTxDetails)
        {
            Object txdata;
            TxToJSON(tx, txdata);
            txinfo.push_back(txdata);
        }
        else
            txinfo.push_back(tx.GetHash().GetHex());
    }
    result.push_back(Pair("tx", txinfo));

    result.push_back(Pair("coinagedestroyed", (boost::int64_t)blockindex->nCoinAgeDestroyed));
    result.push_back(Pair("vote", voteToJSON(blockindex->vote)));
    Array parkRateResults;
    BOOST_FOREACH(const CParkRateVote& parkRateResult, blockindex->vParkRateResult)
        parkRateResults.push_back(parkRateVoteToJSON(parkRateResult));
    result.push_back(Pair("parkrates", parkRateResults));
    if (blockindex->vElectedCustodian.size() > 0)
    {
        Object electedCustodians;
        BOOST_FOREACH(const CCustodianVote& custodianVote, blockindex->vElectedCustodian)
        {
            electedCustodians.push_back(Pair(custodianVote.GetAddress().ToString(), (double)custodianVote.nAmount / COIN));
        }
        result.push_back(Pair("electedcustodians", electedCustodians));
    }

    return result;
}


Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "Returns the number of blocks in the longest block chain.");

    return nBestHeight;
}


Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "Returns difficulty as a multiple of the minimum difficulty.");

    Object obj;
    obj.push_back(Pair("proof-of-work",        GetDifficulty()));
    obj.push_back(Pair("proof-of-stake",       GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("search-interval",      (int)nLastCoinStakeSearchInterval));
    return obj;
}


Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "settxfee is disabled\n");

    return "settxfee is disabled";
}

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getrawmempool\n"
            "Returns all transaction ids in memory pool.");

    vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    Array a;
    BOOST_FOREACH(const uint256& hash, vtxid)
        a.push_back(hash.ToString());

    return a;
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    return pblockindex->phashBlock->GetHex();
}

Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "getblock <hash> [txinfo] [txdetails]\n"
            "txinfo optional to print more detailed tx info\n"
            "txdetails optional to print even more detailed tx info\n"
            "Returns details of a block with given block-hash.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex);

    bool fTxInfo = params.size() > 1 ? params[1].get_bool() : false;
    bool fTxDetails = params.size() > 2 ? params[2].get_bool() : false;

    return blockToJSON(block, pblockindex, fTxInfo, fTxDetails);
}

Value gettxoutsetinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gettxoutsetinfo\n"
            "Returns statistics about the unspent transaction output set.");

    Object ret;

    CCoinsStats stats;
    if (pcoinsTip->GetStats(stats)) {
        ret.push_back(Pair("height", (boost::int64_t)stats.nHeight));
        ret.push_back(Pair("bestblock", stats.hashBlock.GetHex()));
        ret.push_back(Pair("transactions", (boost::int64_t)stats.nTransactions));
        ret.push_back(Pair("txouts", (boost::int64_t)stats.nTransactionOutputs));
        ret.push_back(Pair("bytes_serialized", (boost::int64_t)stats.nSerializedSize));
        ret.push_back(Pair("hash_serialized", stats.hashSerialized.GetHex()));
        ret.push_back(Pair("total_amount", ValueFromAmount(stats.nTotalAmount)));
    }
    return ret;
}

Value gettxout(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "gettxout <txid> <n> [includemempool=true]\n"
            "Returns details about an unspent transaction output.");

    Object ret;

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    int n = params[1].get_int();
    bool fMempool = true;
    if (params.size() > 2)
        fMempool = params[2].get_bool();

    CCoins coins;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(*pcoinsTip, mempool);
        if (!view.GetCoins(hash, coins))
            return Value::null;
        mempool.pruneSpent(hash, coins); // TODO: this should be done by the CCoinsViewMemPool
    } else {
        if (!pcoinsTip->GetCoins(hash, coins))
            return Value::null;
    }
    if (n<0 || (unsigned int)n>=coins.vout.size() || coins.vout[n].IsNull())
        return Value::null;

    ret.push_back(Pair("bestblock", pcoinsTip->GetBestBlock()->GetBlockHash().GetHex()));
    if ((unsigned int)coins.nHeight == MEMPOOL_HEIGHT)
        ret.push_back(Pair("confirmations", 0));
    else
        ret.push_back(Pair("confirmations", pcoinsTip->GetBestBlock()->nHeight - coins.nHeight + 1));
    ret.push_back(Pair("value", ValueFromAmount(coins.vout[n].nValue)));
    Object o;
    ScriptPubKeyToJSON(coins.vout[n].scriptPubKey, o, coins.cUnit);
    ret.push_back(Pair("scriptPubKey", o));
    ret.push_back(Pair("version", coins.nVersion));
    ret.push_back(Pair("coinbase", coins.fCoinBase));

    return ret;
}

CBlockIndex *getOptionalBlockHeight(const Array& params, const int blockHeightIndex)
{
    CBlockIndex *pindex = pindexBest;
    if (params.size() > 0)
    {
        int nHeight = params[0].get_int();

        if (nHeight < 0 || nHeight > nBestHeight)
            throw runtime_error("Invalid height\n");

        for (int i = nBestHeight; i > nHeight; i--)
            pindex = pindex->pprev;
    }
    return pindex;
}


Value getprotocolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getprotocolinfo [<block height>]\n"
            "Returns an object containing the current active protocol and votes.");

    Object obj;

    CBlockIndex *pindex = getOptionalBlockHeight(params, 0);

    obj.push_back(Pair("version",       pindex->nProtocolVersion));
    obj.push_back(Pair("max_version",    PROTOCOL_VERSION));
    obj.push_back(Pair("switch_percent", (double)PROTOCOL_SWITCH_REQUIRE_VOTES/PROTOCOL_SWITCH_COUNT_VOTES*100));

    map<int, int> mapProtocolVotes;
    for (unsigned int i = 0; i < PROTOCOL_SWITCH_COUNT_VOTES && pindex; i++, pindex = pindex->pprev)
        mapProtocolVotes[pindex->vote.nVersionVote]++;

    Object protocolVotesObject;
    BOOST_FOREACH(const PAIRTYPE(int, int)& protocolVote, mapProtocolVotes)
    {
        string version = boost::lexical_cast<std::string>(protocolVote.first);
        Object voteDetails;
        voteDetails.push_back(Pair("votes", protocolVote.second));
        voteDetails.push_back(Pair("percentage", (double)protocolVote.second/PROTOCOL_SWITCH_COUNT_VOTES*100));
        protocolVotesObject.push_back(Pair(version, voteDetails));
    }
    obj.push_back(Pair("protocol_votes", protocolVotesObject));

    return obj;
}
