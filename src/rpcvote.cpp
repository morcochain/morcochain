// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "main.h"
#include "bitcoinrpc.h"
#include "threadwallet.h"
#include "wallet.h"

using namespace json_spirit;
using namespace std;

Object parkRateVoteToJSON(const CParkRateVote& parkRateVote)
{
    Object object;
    object.push_back(Pair("unit", string(1, parkRateVote.cUnit)));

    Array rates;
    BOOST_FOREACH(const CParkRate& parkRate, parkRateVote.vParkRate)
    {
        Object rate;
        rate.push_back(Pair("blocks", (boost::int64_t)parkRate.GetDuration()));
        rate.push_back(Pair("rate", ValueFromParkRate(parkRate.nRate)));
        rates.push_back(rate);
    }
    object.push_back(Pair("rates", rates));

    return object;
}


Object voteToJSON(const CVote& vote)
{
    Object result;

    Array custodianVotes;
    BOOST_FOREACH(const CCustodianVote& custodianVote, vote.vCustodianVote)
    {
        Object object;
        object.push_back(Pair("address", custodianVote.GetAddress().ToString()));
        object.push_back(Pair("amount", (double)custodianVote.nAmount / COIN));
        custodianVotes.push_back(object);
    }
    result.push_back(Pair("custodians", custodianVotes));

    Array parkRateVotes;
    BOOST_FOREACH(const CParkRateVote& parkRateVote, vote.vParkRateVote)
    {
        Object object = parkRateVoteToJSON(parkRateVote);
        parkRateVotes.push_back(object);
    }
    result.push_back(Pair("parkrates", parkRateVotes));

    Array motionVotes;
    BOOST_FOREACH(const uint160& motionVote, vote.vMotion)
    {
        motionVotes.push_back(motionVote.GetHex());
    }
    result.push_back(Pair("motions", motionVotes));

    Object feeVotes;
    BOOST_FOREACH(const PAIRTYPE(unsigned char, uint32_t)& feeVote, vote.mapFeeVote.GetMap())
        feeVotes.push_back(Pair(string(1, feeVote.first), (double)feeVote.second / COIN));
    result.push_back(Pair("fees", feeVotes));

    return result;
}

Value getvote(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getvote\n"
            "Returns the vote that will be inserted in the next proof of stake block.");

    return voteToJSON(pwalletMain->vote);
}


CVote SampleVote()
{
    CVote sample;

    CCustodianVote custodianVote;
    CBitcoinAddress custodianAddress(CKeyID(uint160(123)), 'B');
    custodianVote.SetAddress(custodianAddress);
    custodianVote.nAmount = 100 * COIN;
    sample.vCustodianVote.push_back(custodianVote);

    CCustodianVote custodianVote2;
    CBitcoinAddress custodianAddress2(CScriptID(uint160(555555555)), 'B');
    custodianVote2.SetAddress(custodianAddress2);
    custodianVote2.nAmount = 5.5 * COIN;
    sample.vCustodianVote.push_back(custodianVote2);

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'B';
    parkRateVote.vParkRate.push_back(CParkRate(13, 3 * COIN_PARK_RATE / COIN));
    parkRateVote.vParkRate.push_back(CParkRate(14, 6 * COIN_PARK_RATE / COIN));
    parkRateVote.vParkRate.push_back(CParkRate(15, 13 * COIN_PARK_RATE / COIN));
    sample.vParkRateVote.push_back(parkRateVote);

    sample.vMotion.push_back(uint160("8151325dcdbae9e0ff95f9f9658432dbedfdb209"));
    sample.vMotion.push_back(uint160("3f786850e387550fdab836ed7e6dc881de23001b"));

    return sample;
}

Value setvote(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            string("setvote <vote>\n"
            "<vote> is the complete vote in JSON. Example:\n") +
            write_string(Value(voteToJSON(SampleVote())), false)
            );

    Object objVote = params[0].get_obj();
    CVote vote = ParseVote(objVote);
    if (!vote.IsValid(pindexBest->nProtocolVersion))
        throw runtime_error("The vote is invalid\n");

    pwalletMain->SetVote(vote);

    return voteToJSON(pwalletMain->vote);
}

struct MotionResult
{
    int nBlocks;
    int64 nShareDaysDestroyed;

    MotionResult() :
        nBlocks(0),
        nShareDaysDestroyed(0)
    {
    }
};

Value getmotions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getmotions [<block height>] [<block quantity>]\n"
            "Returns an object containing the motion vote results.");

    Object obj;

    CBlockIndex *pindex = pindexBest;

    if (params.size() > 0)
    {
        int nHeight = params[0].get_int();

        if (nHeight < 0 || nHeight > nBestHeight)
            throw runtime_error("Invalid height\n");

        for (int i = nBestHeight; i > nHeight; i--)
            pindex = pindex->pprev;
    }

    int nQuantity;
    if (params.size() > 1)
        nQuantity = params[1].get_int();
    else
        nQuantity = MOTION_VOTES;

    if (nQuantity <= 0)
        throw runtime_error("Invalid quantity\n");

    map<const uint160, MotionResult> mapMotion;

    MotionResult total;
    for (int i = 0; i < nQuantity && pindex; i++, pindex = pindex->pprev)
    {
        if (!pindex->IsProofOfStake())
            continue;

        const CVote& vote = pindex->vote;

        // Converting to set to avoid the possibility of a double vote.
        const std::set<uint160> setMotion(vote.vMotion.begin(), vote.vMotion.end());

        BOOST_FOREACH(const uint160& hashMotion, setMotion)
        {
            MotionResult& result = mapMotion[hashMotion];
            result.nBlocks++;
            result.nShareDaysDestroyed += vote.nCoinAgeDestroyed;
        }

        total.nBlocks++;
        total.nShareDaysDestroyed += vote.nCoinAgeDestroyed;
    }

    BOOST_FOREACH(const PAIRTYPE(uint160, MotionResult)& resultPair, mapMotion)
    {
        const uint160& hashMotion = resultPair.first;
        const MotionResult& result = resultPair.second;
        Object resultObject;
        resultObject.push_back(Pair("blocks", result.nBlocks));
        resultObject.push_back(Pair("block_percentage", (double)result.nBlocks / total.nBlocks * 100.0));
        resultObject.push_back(Pair("sharedays", (boost::int64_t)result.nShareDaysDestroyed));
        resultObject.push_back(Pair("shareday_percentage", (double)result.nShareDaysDestroyed / total.nShareDaysDestroyed * 100.0));
        obj.push_back(Pair(hashMotion.ToString(), resultObject));
    }
    return obj;
}


struct CustodianResult
{
    int nBlocks;
    int64 nShareDaysDestroyed;

    CustodianResult() :
        nBlocks(0),
        nShareDaysDestroyed(0)
    {
    }
};

typedef map<int64, CustodianResult> CustodianAmountResultMap;
typedef map<CBitcoinAddress, CustodianAmountResultMap> CustodianResultMap;

Value getcustodianvotes(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getcustodianvotes [<block height>] [<block quantity>]\n"
            "Returns an object containing the custodian vote results.");

    Object obj;

    CBlockIndex *pindex = pindexBest;

    if (params.size() > 0)
    {
        int nHeight = params[0].get_int();

        if (nHeight < 0 || nHeight > nBestHeight)
            throw runtime_error("Invalid height\n");

        for (int i = nBestHeight; i > nHeight; i--)
            pindex = pindex->pprev;
    }

    int nQuantity;
    if (params.size() > 1)
        nQuantity = params[1].get_int();
    else
        nQuantity = CUSTODIAN_VOTES;

    if (nQuantity <= 0)
        throw runtime_error("Invalid quantity\n");

    CustodianResultMap mapCustodian;

    CustodianResult total;
    for (int i = 0; i < nQuantity && pindex; i++, pindex = pindex->pprev)
    {
        if (!pindex->IsProofOfStake())
            continue;

        const CVote& vote = pindex->vote;

        BOOST_FOREACH(const CCustodianVote& custodianVote, vote.vCustodianVote)
        {
            CustodianResult& result = mapCustodian[custodianVote.GetAddress()][custodianVote.nAmount];
            result.nBlocks++;
            result.nShareDaysDestroyed += vote.nCoinAgeDestroyed;
        }

        total.nBlocks++;
        total.nShareDaysDestroyed += vote.nCoinAgeDestroyed;
    }

    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CustodianAmountResultMap)& custodianResultPair, mapCustodian)
    {
        const CBitcoinAddress& address = custodianResultPair.first;
        Object custodianObject;
        BOOST_FOREACH(const PAIRTYPE(int64, CustodianResult)& resultPair, custodianResultPair.second)
        {
            int64 nAmount = resultPair.first;
            const CustodianResult& result = resultPair.second;
            Object resultObject;
            resultObject.push_back(Pair("blocks", result.nBlocks));
            resultObject.push_back(Pair("block_percentage", (double)result.nBlocks / total.nBlocks * 100.0));
            resultObject.push_back(Pair("sharedays", (boost::int64_t)result.nShareDaysDestroyed));
            resultObject.push_back(Pair("shareday_percentage", (double)result.nShareDaysDestroyed / total.nShareDaysDestroyed * 100.0));
            custodianObject.push_back(Pair(FormatMoney(nAmount), resultObject));
        }
        obj.push_back(Pair(address.ToString(), custodianObject));
    }

    Object totalObject;
    totalObject.push_back(Pair("blocks", total.nBlocks));
    totalObject.push_back(Pair("sharedays", (boost::int64_t)total.nShareDaysDestroyed));
    obj.push_back(Pair("total", totalObject));

    return obj;
}


Value getelectedcustodians(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getelectedcustodians\n"
            "Returns an object containing the elected custodians.");

    Array result;

    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CBlockIndex*)& pair, pindexBest->GetElectedCustodians())
    {
        const CBitcoinAddress address = pair.first;
        const CBlockIndex* pindex = pair.second;

        BOOST_FOREACH(const CCustodianVote& custodianVote, pindex->vElectedCustodian)
        {
            if (custodianVote.GetAddress() != address)
                continue;

            Object custodianObject;
            custodianObject.push_back(Pair("unit", string(1, custodianVote.cUnit)));
            custodianObject.push_back(Pair("address", address.ToString()));
            custodianObject.push_back(Pair("amount", ValueFromAmount(custodianVote.nAmount)));
            custodianObject.push_back(Pair("block", pindex->GetBlockHash().ToString()));
            custodianObject.push_back(Pair("time", DateTimeStrFormat(pindex->nTime)));
            custodianObject.push_back(Pair("height", pindex->nHeight));

            result.push_back(custodianObject);
        }
    }

    return result;
}


typedef map<int64, int64> RateWeightMap;
typedef RateWeightMap::value_type RateWeight;

typedef map<unsigned char, RateWeightMap> DurationRateWeightMap;
typedef DurationRateWeightMap::value_type DurationRateWeight;

Value getparkvotes(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getparkvotes [<block height>] [<block quantity>]\n"
            "Returns an object containing a summary of the park rate votes.");

    Object obj;

    CBlockIndex *pindex = pindexBest;

    if (params.size() > 0)
    {
        int nHeight = params[0].get_int();

        if (nHeight < 0 || nHeight > nBestHeight)
            throw runtime_error("Invalid height\n");

        for (int i = nBestHeight; i > nHeight; i--)
            pindex = pindex->pprev;
    }

    int nQuantity;
    if (params.size() > 1)
        nQuantity = params[1].get_int();
    else
        nQuantity = PARK_RATE_VOTES;

    if (nQuantity <= 0)
        throw runtime_error("Invalid quantity\n");

    DurationRateWeightMap durationRateWeights;
    int64 totalVoteWeight = 0;
    map<unsigned char, int64> coinAgeDestroyedPerDuration;

    for (int i = 0; i < nQuantity && pindex; i++, pindex = pindex->pprev)
    {
        if (!pindex->IsProofOfStake())
            continue;

        const CVote& vote = pindex->vote;

        totalVoteWeight += vote.nCoinAgeDestroyed;

        BOOST_FOREACH(const CParkRateVote& parkRateVote, vote.vParkRateVote)
        {
            BOOST_FOREACH(const CParkRate& parkRate, parkRateVote.vParkRate)
            {
                RateWeightMap &rateWeights = durationRateWeights[parkRate.nCompactDuration];
                rateWeights[parkRate.nRate] += vote.nCoinAgeDestroyed;
                coinAgeDestroyedPerDuration[parkRate.nCompactDuration] += vote.nCoinAgeDestroyed;
            }
        }
    }

    // Add abstension (= vote for a rate of 0)
    BOOST_FOREACH(DurationRateWeight& durationRateWeight, durationRateWeights)
    {
        unsigned char nCompactDuration = durationRateWeight.first;
        RateWeightMap &rateWeights = durationRateWeight.second;
        const int64 coinAgeDestroyed = coinAgeDestroyedPerDuration[nCompactDuration];

        assert(coinAgeDestroyed <= totalVoteWeight);
        int64 abstainedCoinAge = totalVoteWeight - coinAgeDestroyed;
        if (abstainedCoinAge)
            rateWeights[0] += abstainedCoinAge;
    }

    Object unitResult;
    BOOST_FOREACH(const DurationRateWeight& durationRateWeight, durationRateWeights)
    {
        unsigned char nCompactDuration = durationRateWeight.first;
        const RateWeightMap &rateWeights = durationRateWeight.second;

        Object durationObject;
        boost::int64_t blocks = (int64)1<<nCompactDuration;
        durationObject.push_back(Pair("blocks", blocks));
        durationObject.push_back(Pair("estimated_duration", BlocksToTime(blocks)));

        int64 accumulatedWeight = 0;

        Array votes;
        BOOST_FOREACH(const RateWeight& rateWeight, rateWeights)
        {
            Object rateVoteObject;
            boost::int64_t rate = rateWeight.first;
            boost::int64_t weight = rateWeight.second;

            double shareDays = (double)weight / (24 * 60 * 60);
            double shareDayPercentage = (double)weight / (double)totalVoteWeight * 100;

            accumulatedWeight += weight;
            double accumulatedPercentage = (double)accumulatedWeight / (double)totalVoteWeight * 100;

            rateVoteObject.push_back(Pair("rate", ValueFromParkRate(rate)));
            rateVoteObject.push_back(Pair("annual_percentage", AnnualInterestRatePercentage(rate, blocks)));
            rateVoteObject.push_back(Pair("sharedays", shareDays));
            rateVoteObject.push_back(Pair("shareday_percentage", shareDayPercentage));
            rateVoteObject.push_back(Pair("accumulated_percentage", accumulatedPercentage));

            votes.push_back(rateVoteObject);
        }
        durationObject.push_back(Pair("votes", votes));


        string durationLabel = boost::lexical_cast<std::string>((int)nCompactDuration);
        unitResult.push_back(Pair(durationLabel, durationObject));
    }
    obj.push_back(Pair("B", unitResult));

    return obj;
}

Value setdatafeed(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 1 && params.size() != 3 && params.size() != 4))
        throw runtime_error(
            "setdatafeed <url> [<signature url> <address>] [<parts>]\n"
            "Change the vote data feed. Set <url> to an empty string to disable.\n"
            "If <signature url> and <address> are specified and not empty strings a signature will also be retrieved at <signature url> and verified.\n"
            "Parts is the list of the top level vote parts that will be taken from the feed, separated by a coma. The other parts will not affect the vote. Default is \"custodians,parkrates,motions,fees\".");

    string sURL = params[0].get_str();

    string sSignatureURL;
    if (params.size() > 1)
        sSignatureURL = params[1].get_str();

    string sAddress;
    if (params.size() > 2)
        sAddress = params[2].get_str();

    string sParts("custodians,parkrates,motions,fees");
    if (params.size() > 3)
        sParts = params[3].get_str();
    vector<string> vParts;
    boost::split(vParts, sParts, boost::is_any_of(","));

    BOOST_FOREACH(const string sPart, vParts)
    {
        if (sPart != "custodians" && sPart != "parkrates" && sPart != "motions" && sPart != "fees")
            throw runtime_error("Invalid parts");
    }

    CWallet* pwallet = GetWallet('S');
    pwallet->SetDataFeed(CDataFeed(sURL, sSignatureURL, sAddress, vParts));

    try
    {
        UpdateFromDataFeed();
    }
    catch (std::exception& e)
    {
        strDataFeedError = e.what();
        return (boost::format("Warning: data feed was changed but the initial fetch failed: %1%") % e.what()).str();
    }

    return "";
}

Value getdatafeed(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdatafeed\n"
            "Return the current data feed.");

    CWallet* pwallet = GetWallet('S');
    const CDataFeed& dataFeed(pwallet->GetDataFeed());

    Object result;
    result.push_back(Pair("url", dataFeed.sURL));
    result.push_back(Pair("signatureurl", dataFeed.sSignatureURL));
    result.push_back(Pair("signatureaddress", dataFeed.sSignatureAddress));
    result.push_back(Pair("parts", boost::algorithm::join(dataFeed.vParts, ",")));

    return result;
}

