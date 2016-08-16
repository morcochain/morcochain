// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "liquidityinfo.h"
#include "wallet.h"

using namespace json_spirit;
using namespace std;

static map<string, CLiquidityInfo> mapLiquidity;
static CCriticalSection cs_mapLiquidity;

Value liquidityinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 5)
        throw runtime_error(
            "liquidityinfo <currency> <buyamount> <sellamount> <grantaddress> [<indentifier>]\n"
            "Broadcast liquidity information.\n"
            "currency is the single letter of the currency (currently only 'B')\n"
            "grantaddress is the custodian address that was granted. The private key of this address must be in the wallet."
            );

    if (params[0].get_str().size() != 1)
        throw JSONRPCError(-3, "Invalid currency");

    unsigned char cUnit = params[0].get_str()[0];

    if (!IsValidCurrency(cUnit))
        throw JSONRPCError(-3, "Invalid currency");

    CBitcoinAddress address(params[3].get_str());

    if (!address.IsValid())
        throw JSONRPCError(-3, "Invalid address");

    unsigned char cCustodianUnit = address.GetUnit();

    if (!IsValidCurrency(cCustodianUnit))
        throw JSONRPCError(-3, "Invalid custodian unit");

    CWallet* wallet = GetWallet(cCustodianUnit);

    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(-3, "Address does not refer to key");

    CKey key;
    {
        LOCK(wallet->cs_wallet);

        if (!wallet->GetKey(keyID, key))
            throw JSONRPCError(-4, "Private key not available");
    }

    CLiquidityInfo info;
    if (pindexBest->nProtocolVersion >= PROTOCOL_V2_0)
        info.nVersion = PROTOCOL_V2_0;
    else
        info.nVersion = 50000;
    info.cUnit = cUnit;
    info.nTime = GetAdjustedTime();
    info.nBuyAmount = roundint64(params[1].get_real() * COIN);
    info.nSellAmount = roundint64(params[2].get_real() * COIN);

    string sIdentifier;
    if (params.size() > 4)
        sIdentifier = params[4].get_str();
    if (info.nVersion >= PROTOCOL_V2_0)
        info.sIdentifier = sIdentifier;

    // Old process: liquidity identifiers were kept in memory and summed before the info was sent
    // To remove when the network definitively switched to 2.0
    if (info.nVersion < PROTOCOL_V2_0)
    {
        LOCK(cs_mapLiquidity);
        mapLiquidity[sIdentifier] = info;

        BOOST_FOREACH(const PAIRTYPE(string, CLiquidityInfo)& otherInfoPair, mapLiquidity)
        {
            const string& otherIdentifier = otherInfoPair.first;
            if (otherIdentifier != sIdentifier)
            {
                const CLiquidityInfo& otherInfo = otherInfoPair.second;
                info.nBuyAmount += otherInfo.nBuyAmount;
                info.nSellAmount += otherInfo.nSellAmount;
            }
        }
    }

    if (info.nBuyAmount < 0 || info.nSellAmount < 0)
        throw JSONRPCError(-3, "Invalid amount");

    info.cCustodianUnit = address.GetUnit();
    info.vchCustodianPubKey = key.GetPubKey().Raw();

    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedLiquidityInfo)info;
    info.vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());

    if (!key.Sign(Hash(info.vchMsg.begin(), info.vchMsg.end()), info.vchSig))
        throw runtime_error(
            "Unable to sign liquidity info, check private key?\n");
    if(!info.ProcessLiquidityInfo())
        throw runtime_error(
            "Failed to process info.\n");
    // Relay info
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            info.RelayTo(pnode);
    }

    return "";
}

struct CLiquidity
{
    int64 nBuy;
    int64 nSell;

    CLiquidity() :
        nBuy(0),
        nSell(0)
    {
    }

    CLiquidity(int64 nBuy, int64 nSell) :
        nBuy(nBuy),
        nSell(nSell)
    {
    }

    CLiquidity(const CLiquidityInfo& info) :
        nBuy(info.nBuyAmount),
        nSell(info.nSellAmount)
    {
    }

    CLiquidity operator+=(const CLiquidity& other)
    {
        nBuy += other.nBuy;
        nSell += other.nSell;
        return *this;
    }

    friend CLiquidity operator+(const CLiquidity& a, const CLiquidity& b)
    {
        return CLiquidity(a.nBuy + b.nBuy, a.nSell + b.nSell);
    }

    Object ToObject() const
    {
        Object object;
        object.push_back(Pair("buy", ValueFromAmount(nBuy)));
        object.push_back(Pair("sell", ValueFromAmount(nSell)));
        return object;
    }
};

Value getliquidityinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getliquidityinfo <currency>\n"
            "currency is the single letter of the currency (currently only 'B')"
            );

    if (params[0].get_str().size() != 1)
        throw JSONRPCError(-3, "Invalid currency");

    unsigned char cUnit = params[0].get_str()[0];

    if (!IsValidCurrency(cUnit))
        throw JSONRPCError(-3, "Invalid currency");

    Object result;
    CLiquidity total;
    map<string, CLiquidity> mapTier;
    map<string, CLiquidity> mapCustodian;
    {
        LOCK(cs_mapLiquidityInfo);

        BOOST_FOREACH(const PAIRTYPE(const CLiquiditySource, CLiquidityInfo)& item, mapLiquidityInfo)
        {
            const CLiquidityInfo& info = item.second;
            if (info.cUnit == cUnit)
            {
                CLiquidity liquidity(info);
                total += liquidity;

                mapCustodian[info.GetCustodianAddress().ToString()] += liquidity;

                string tierString = info.GetTier();
                if (tierString != "")
                    mapTier[tierString] += liquidity;
            }
        }
    }
    result.push_back(Pair("total", total.ToObject()));

    Object tierResult;
    BOOST_FOREACH(const PAIRTYPE(string, CLiquidity)& item, mapTier)
    {
        const string& tier = item.first;
        const CLiquidity& liquidity = item.second;
        tierResult.push_back(Pair(tier, liquidity.ToObject()));
    }
    result.push_back(Pair("tier", tierResult));

    Object custodianResult;
    BOOST_FOREACH(const PAIRTYPE(string, CLiquidity)& item, mapCustodian)
    {
        const string& tier = item.first;
        const CLiquidity& liquidity = item.second;
        custodianResult.push_back(Pair(tier, liquidity.ToObject()));
    }
    result.push_back(Pair("custodian", custodianResult));


    return result;
}

Value getliquiditydetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getliquiditydetails <currency>\n"
            "currency is the single letter of the currency (currently only 'B')"
            );

    if (params[0].get_str().size() != 1)
        throw JSONRPCError(-3, "Invalid currency");

    unsigned char cUnit = params[0].get_str()[0];

    if (!IsValidCurrency(cUnit))
        throw JSONRPCError(-3, "Invalid currency");

    map<CBitcoinAddress, Object> mapCustodianResult;
    {
        LOCK(cs_mapLiquidityInfo);

        BOOST_FOREACH(const PAIRTYPE(const CLiquiditySource, CLiquidityInfo)& item, mapLiquidityInfo)
        {
            const CLiquidityInfo& info = item.second;
            if (info.cUnit == cUnit)
            {
                CLiquidity liquidity(info);
                Object liquidityObject = liquidity.ToObject();;

                Object& custodianObject = mapCustodianResult[info.GetCustodianAddress()];
                custodianObject.push_back(Pair(info.sIdentifier, liquidityObject));
            }
        }
    }

    Object result;
    BOOST_FOREACH(const PAIRTYPE(const CBitcoinAddress, Object)& item, mapCustodianResult)
    {
        const CBitcoinAddress& address = item.first;
        const Object& object = item.second;
        result.push_back(Pair(address.ToString(), object));
    }

    return result;
}

