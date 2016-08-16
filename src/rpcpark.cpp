// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/lexical_cast.hpp>
#include "main.h"
#include "bitcoinrpc.h"
#include "threadwallet.h"
#include "wallet.h"
#include "walletdb.h"

using namespace json_spirit;
using namespace std;

// in rpcwallet.cpp
extern string AccountFromValue(const Value& value);
extern CBitcoinAddress GetAccountAddress(string strAccount, bool bForceNew=false);
extern int64 GetAccountBalance(const string& strAccount, int nMinDepth);

Value getparkrates(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getparkrates [<height>] [<currency>]\n"
            "Returns an object containing the park rates in the block at height <height> (default: the last block).\n"
            "The default <currency> is the currency of the RPC server's wallet.");

    const CBlockIndex *pindex = pindexBest;

    if (params.size() > 0)
    {
        int nHeight = params[0].get_int();

        if (nHeight < 0 || nHeight > pindex->nHeight)
            throw JSONRPCError(-12, "Error: Invalid height");

        while (pindex->nHeight != nHeight)
            pindex = pindex->pprev;
    }

    unsigned char cUnit;
    if (params.size() > 1)
        cUnit = params[1].get_str()[0];
    else
        cUnit = pwalletMain->Unit();

    if (!IsValidUnit(cUnit))
        throw JSONRPCError(-12, "Error: Invalid currency");

    if (cUnit == 'S')
        throw JSONRPCError(-12, "Error: Park rates are not available on NuShares");

    pindex = pindex->GetIndexWithEffectiveParkRates();

    BOOST_FOREACH(const CParkRateVote& parkRateVote, pindex->vParkRateResult)
    {
        if (parkRateVote.cUnit != cUnit)
            continue;

        Object obj;
        BOOST_FOREACH(const CParkRate& parkRate, parkRateVote.vParkRate)
        {
            string label = boost::lexical_cast<std::string>(parkRate.GetDuration()) + " blocks";
            obj.push_back(Pair(label, ValueFromParkRate(parkRate.nRate)));
        }
        return obj;
    }

    throw JSONRPCError(-12, "Error: Park rates not found");
}

Value park(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() < 2 || params.size() > 5))
        throw runtime_error(
            "park <amount> <duration> [account=\"\"] [unparkaddress] [minconf=1]\n"
            "<amount> is a real and is rounded to the nearest 0.0001\n"
            "<duration> is the number of blocks during which the amount will be parked\n"
            "<unparkaddress> is the address to which the amount will be returned when they are unparked (default is the main address of the account)\n"
            "requires wallet passphrase to be set with walletpassphrase first");
    if (!pwalletMain->IsCrypted() && (fHelp || params.size() < 2 || params.size() > 5))
        throw runtime_error(
            "park <amount> <duration> [account=\"\"] [unparkaddress] [minconf=1]\n"
            "<amount> is a real and is rounded to the nearest 0.0001\n"
            "<duration> is the number of blocks during which the amount will be parked\n"
            "<unparkaddress> is the address to which the amount will be returned when they are unparked (default is the main address of the account)\n");

    int64 nAmount = AmountFromValue(params[0]);

    int64 nDuration = params[1].get_int();
    if (!ParkDurationRange(nDuration))
        throw JSONRPCError(-5, "Invalid duration");

    string strAccount;
    if (params.size() > 2)
        strAccount = AccountFromValue(params[2]);
    else
        strAccount = "";

    CBitcoinAddress unparkAddress(params.size() > 3 ? CBitcoinAddress(params[3].get_str()) : GetAccountAddress(strAccount));
    if (!unparkAddress.IsValid(pwalletMain->GetUnit()))
        throw JSONRPCError(-5, "Invalid address");

    int nMinDepth = 1;
    if (params.size() > 4)
        nMinDepth = params[4].get_int();

    if (pwalletMain->IsLocked())
        throw JSONRPCError(-13, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    // Check funds
    int64 nBalance = GetAccountBalance(strAccount, nMinDepth);
    if (nAmount > nBalance)
        throw JSONRPCError(-6, "Account has insufficient funds");

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;

    string strError = pwalletMain->Park(nAmount, nDuration, unparkAddress, wtx);
    if (strError != "")
        throw JSONRPCError(-4, strError);

    return wtx.GetHash().GetHex();
}

Value getpremium(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "getpremium <amount> <duration> [hash]\n"
            "<amount> is a real and is rounded to the nearest 0.0001\n"
            "<duration> is the number of blocks during which the amount would be parked\n"
            "If [hash] is not specified, returns current expected parking premium");


    int64 nAmount = AmountFromValue(params[0]);

    int64 nDuration = params[1].get_int();
    if (!ParkDurationRange(nDuration))
        throw JSONRPCError(-5, "Invalid duration");

    if (params.size() == 3)
    {
        //Case where hash is given; retrieves premium at particular block
        std::string strHash = params[2].get_str();
        uint256 hash(strHash);

        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(-5, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];

        int64 nPremium = pblockindex->GetPremium(nAmount, nDuration, pwalletMain->GetUnit());

        return FormatMoney(nPremium);
    }
    else
    {
        //Case where hash is not given; does same thing as old getpremium
        int64 nPremium = pindexBest->GetNextPremium(nAmount, nDuration, pwalletMain->GetUnit());

        return FormatMoney(nPremium);
    }

}

Value unpark(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "unpark\n"
            "unpark all transaction that have reached duration");

    Array ret;
    vector<CWalletTx> vTransaction;

    if (!pwalletMain->SendUnparkTransactions(vTransaction))
        throw JSONRPCError(-1, "SendUnparkTransactions failed");

    BOOST_FOREACH(const CWalletTx& wtxUnpark, vTransaction)
        ret.push_back(wtxUnpark.GetHash().GetHex());

    return ret;
}

Value listparked(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "listparked [account]\n"
            "Returns the list of parked coins.");

    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();

    bool fAllAccounts = (strAccount == string("*"));

    Array ret;
    for (set<COutPoint>::const_iterator it = pwalletMain->setParked.begin(); it != pwalletMain->setParked.end(); ++it)
    {
        const CWalletTx& wtx = pwalletMain->mapWallet[it->hash];
        if (wtx.vout.size() <= it->n)
            throw JSONRPCError(-2, "Invalid output");
        const CTxOut& txo = wtx.vout[it->n];

        Object park;
        int64 nDuration;

        CTxDestination unparkDestination;
        if (!ExtractPark(txo.scriptPubKey, nDuration, unparkDestination))
            throw JSONRPCError(-2, "Invalid scriptPubKey");
        CBitcoinAddress unparkAddress(unparkDestination, wtx.cUnit);

        if (!fAllAccounts && pwalletMain->mapAddressBook[unparkDestination] != strAccount)
            continue;

        park.push_back(Pair("txid", wtx.GetHash().GetHex()));
        park.push_back(Pair("output", (boost::int64_t)it->n));
        park.push_back(Pair("time", DateTimeStrFormat(wtx.GetTxTime())));
        park.push_back(Pair("amount", ValueFromAmount(txo.nValue)));
        park.push_back(Pair("duration", (boost::int64_t)nDuration));
        park.push_back(Pair("unparkaddress", unparkAddress.ToString()));

        CBlockIndex* pindex = NULL;
        int nDepth = wtx.GetDepthInMainChain(pindex);
        park.push_back(Pair("depth", (boost::int64_t)nDepth));

        boost::int64_t nRemaining = nDuration;
        nRemaining -= nDepth;
        if (nRemaining < 0)
            nRemaining = 0;

        park.push_back(Pair("remainingblocks", nRemaining));

        if (pindex)
        {
            int64 nPremium = pindex->GetPremium(txo.nValue, nDuration, wtx.cUnit);
            park.push_back(Pair("premium", ValueFromAmount(nPremium)));
        }

        ret.push_back(park);
    }

    return ret;
}
