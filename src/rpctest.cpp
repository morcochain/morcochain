#ifdef TESTING

#include "bitcoinrpc.h"
#include "main.h"
#include "wallet.h"
#include "threadwallet.h"

using namespace json_spirit;
using namespace std;

Value generatestake(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "generatestake\n"
            "generate a single proof of stake block"
            );

    if (GetBoolArg("-stakegen", true))
        throw JSONRPCError(-3, "Stake generation enabled. Won't start another generation.");

    CWallet *pwallet = GetWallet('S');
    BitcoinMiner(pwallet, true, true);
    return hashSingleStakeBlock.ToString();
}


Value timetravel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "timetravel <seconds>\n"
            "change relative time"
            );

    nTimeShift += params[0].get_int();

    return "";
}


unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake);

Value duplicateblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "duplicateblock <original hash> [<parent hash>]\n"
            "propagate a new block with the same stake as block <original hash> on top of <parent hash> (default: same parent)"
            );

    uint256 originalHash;
    originalHash.SetHex(params[0].get_str());
    if (!mapBlockIndex.count(originalHash))
        throw JSONRPCError(-3, "Original hash not in main chain");
    CBlockIndex *original = mapBlockIndex[originalHash];

    CBlockIndex *pindexPrev;
    if (params.size() > 1)
    {
        uint256 parentHash;
        parentHash.SetHex(params[1].get_str());
        if (!mapBlockIndex.count(parentHash))
            throw JSONRPCError(-3, "Parent hash not in main chain");
        pindexPrev = mapBlockIndex[parentHash];
    }
    else
        pindexPrev = original->pprev;

    CBlock block;
    block.ReadFromDisk(original);

    // Update parent block
    block.hashPrevBlock  = pindexPrev->GetBlockHash();
    block.nBits = GetNextTargetRequired(pindexPrev, block.IsProofOfStake());

    // Increment nonce to make sure the hash changes
    block.nNonce++;

    block.SignBlock(*pwalletMain); // We ignore errors to be able to test duplicate blocks with invalid signature

    // Process this block the same as if we had received it from another node
    // But do not check for errors as this is expected to fail
    CValidationState state;
    ProcessBlock(state, NULL, &block);

    return block.GetHash().ToString();
}

Value ignorenextblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "ignorenextblock [all|0]"
            );

    if (params.size() > 0)
    {
        if (params[0].get_str() == "all")
            nBlocksToIgnore = -1;
        else if (params[0].get_str() == "0")
            nBlocksToIgnore = 0;
        else
            throw JSONRPCError(-3, "invalid parameter");
    }
    else
    {
        nBlocksToIgnore++;
    }

    return "";
}

Value manualunpark(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "manualunpark <park tx hash> <output> <unpark address> <amount>\n"
            );

    uint256 hash = 0;
    hash.SetHex(params[0].get_str());

    int nOutput = params[1].get_int();

    CBitcoinAddress unparkAddress(params[2].get_str());

    int64 nAmount = params[3].get_real() * COIN;

    CWalletTx wtx;
    pwalletMain->CreateUnparkTransaction(hash, nOutput, unparkAddress, nAmount, wtx);

    mempool.addUnchecked(wtx.GetHash(), wtx);
    wtx.RelayWalletTransaction();

    return wtx.GetHash().ToString();
}

Value disconnect(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "disconnect <ip>\n"
            );

    const CNetAddr ip(params[0].get_str().c_str(), true);
    CNode* node = FindNode(ip);
    if (!node)
        throw JSONRPCError(-3, "node not found");

    node->Misbehaving(GetArg("-banscore", 100));

    return Value::null;
}

// in net.cpp
void ConnectToAddress(CNetAddr addr, unsigned short port);
Value connect(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "connect <ip> <port>\n"
            );

    const CNetAddr ip(params[0].get_str().c_str(), true);
    const unsigned short port = params[1].get_int();

    CNode::ConnectToAddress(ip, port);

    return Value::null;
}


Value clearbanned(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "clearbanned\n"
            );

    CNode::ClearBanned();

    return Value::null;
}


#endif
