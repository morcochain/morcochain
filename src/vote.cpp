// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <numeric>

#include "script.h"
#include "vote.h"
#include "main.h"

using namespace std;

CScript CVote::ToScript(int nProtocolVersion) const
{
    CScript voteScript;

    voteScript << OP_RETURN;
    voteScript << OP_1;

    CDataStream voteStream(SER_NETWORK, nProtocolVersion);
    voteStream << *this;

    vector<unsigned char> vchVote(voteStream.begin(), voteStream.end());
    voteScript << vchVote;

    return voteScript;
}

bool IsVote(const CScript& scriptPubKey)
{
    return (scriptPubKey.size() > 2 && scriptPubKey[0] == OP_RETURN && scriptPubKey[1] == OP_1);
}

bool ExtractVote(const CScript& scriptPubKey, CVote& voteRet, int nProtocolVersion)
{
    voteRet.SetNull();

    if (!IsVote(scriptPubKey))
        return false;

    CScript voteScript(scriptPubKey.begin() + 2, scriptPubKey.end());

    if (!voteScript.IsPushOnly())
        return false;

    vector<vector<unsigned char> > stack;
    CTransaction fakeTx;
    EvalScript(stack, voteScript, fakeTx, 0, SCRIPT_VERIFY_STRICTENC, 0);

    if (stack.size() != 1)
        return false;

    CDataStream voteStream(stack[0], SER_NETWORK, nProtocolVersion);

    CVote vote;
    try {
        voteStream >> vote;
    }
    catch (std::exception &e) {
        return error("vote deserialize error");
    }

    voteRet = vote;
    return true;
}

bool ExtractVote(const CBlock& block, CVote& voteRet, int nProtocolVersion)
{
    voteRet.SetNull();

    if (!block.IsProofOfStake())
        return false;

    if (block.vtx.size() < 2)
        return error("invalid transaction count in proof of stake block");

    const CTransaction& tx = block.vtx[1];
    if (!tx.IsCoinStake())
        return error("CoinStake not found in proof of stake block");

    BOOST_FOREACH (const CTxOut& txo, tx.vout)
    {
        if (ExtractVote(txo.scriptPubKey, voteRet, nProtocolVersion))
            return true;
    }

    return false;
}

CScript CParkRateVote::ToParkRateResultScript() const
{
    CScript script;

    script << OP_RETURN;
    script << OP_2;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << *this;

    vector<unsigned char> vch(stream.begin(), stream.end());
    script << vch;

    return script;
}

string CParkRateVote::ToString() const
{
    std::stringstream stream;
    stream << "ParkRateVote unit=" << cUnit;
    BOOST_FOREACH(const CParkRate& parkRate, vParkRate)
        stream << " " << (int)parkRate.nCompactDuration << ":" << parkRate.nRate;
    return stream.str();
}

bool IsParkRateResult(const CScript& scriptPubKey)
{
    return (scriptPubKey.size() > 2 && scriptPubKey[0] == OP_RETURN && scriptPubKey[1] == OP_2);
}

bool ExtractParkRateResult(const CScript& scriptPubKey, CParkRateVote& parkRateResultRet)
{
    parkRateResultRet.SetNull();

    if (!IsParkRateResult(scriptPubKey))
        return false;

    CScript script(scriptPubKey.begin() + 2, scriptPubKey.end());

    if (!script.IsPushOnly())
        return false;

    vector<vector<unsigned char> > stack;
    CTransaction fakeTx;
    EvalScript(stack, script, fakeTx, 0, SCRIPT_VERIFY_STRICTENC, 0);

    if (stack.size() != 1)
        return false;

    CDataStream stream(stack[0], SER_NETWORK, PROTOCOL_VERSION);

    CParkRateVote parkRateResult;
    try {
        stream >> parkRateResult;
    }
    catch (std::exception &e) {
        return error("park rate result deserialize error");
    }

    parkRateResultRet = parkRateResult;
    return true;
}

bool ExtractParkRateResults(const CBlock& block, vector<CParkRateVote>& vParkRateResultRet)
{
    vParkRateResultRet.clear();

    if (!block.IsProofOfStake())
        return false;

    if (block.vtx.size() < 2)
        return error("invalid transaction count in proof of stake block");

    const CTransaction& tx = block.vtx[1];
    if (!tx.IsCoinStake())
        return error("CoinStake not found in proof of stake block");

    set<unsigned char> setSeenUnit;
    BOOST_FOREACH (const CTxOut& txo, tx.vout)
    {
        CParkRateVote result;
        if (ExtractParkRateResult(txo.scriptPubKey, result))
        {
            if (setSeenUnit.count(result.cUnit))
                return error("Duplicate park rate result unit");
            vParkRateResultRet.push_back(result);
            setSeenUnit.insert(result.cUnit);
        }
    }

    return true;
}


typedef map<int64, int64> RateWeightMap;
typedef RateWeightMap::value_type RateWeight;

typedef map<unsigned char, RateWeightMap> DurationRateWeightMap;
typedef DurationRateWeightMap::value_type DurationRateWeight;

static int64 AddRateWeight(const int64& totalWeight, const RateWeight& rateWeight)
{
    return totalWeight + rateWeight.second;
}

bool CalculateParkRateVote(const std::vector<CVote>& vVote, std::vector<CParkRateVote>& results)
{
    results.clear();

    if (vVote.empty())
        return true;

    // Only one unit is supported for now
    vector<CParkRate> result;

    DurationRateWeightMap durationRateWeights;
    int64 totalVoteWeight = 0;

    BOOST_FOREACH(const CVote& vote, vVote)
    {
        if (vote.nCoinAgeDestroyed == 0)
            return error("vote with 0 coin age destroyed");

        totalVoteWeight += vote.nCoinAgeDestroyed;

        BOOST_FOREACH(const CParkRateVote& parkRateVote, vote.vParkRateVote)
        {
            BOOST_FOREACH(const CParkRate& parkRate, parkRateVote.vParkRate)
            {
                RateWeightMap &rateWeights = durationRateWeights[parkRate.nCompactDuration];
                rateWeights[parkRate.nRate] += vote.nCoinAgeDestroyed;
            }
        }
    }

    int64 halfWeight = totalVoteWeight / 2;

    BOOST_FOREACH(const DurationRateWeight& durationRateWeight, durationRateWeights)
    {
        unsigned char nCompactDuration = durationRateWeight.first;
        const RateWeightMap &rateWeights = durationRateWeight.second;

        int64 totalWeight = accumulate(rateWeights.begin(), rateWeights.end(), (int64)0, AddRateWeight);
        int64 sum = totalWeight;
        int64 median = 0;

        BOOST_FOREACH(const RateWeight& rateWeight, rateWeights)
        {
            if (sum <= halfWeight)
                break;

            sum -= rateWeight.second;
            median = rateWeight.first;
        }

        if (median != 0)
        {
            CParkRate parkRate;
            parkRate.nCompactDuration = nCompactDuration;
            parkRate.nRate = median;
            result.push_back(parkRate);
        }
    }

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'B';
    parkRateVote.vParkRate = result;
    results.push_back(parkRateVote);

    return true;
}

bool LimitParkRateChangeV0_5(std::vector<CParkRateVote>& results, const std::map<unsigned char, std::vector<const CParkRateVote*> >& mapPreviousRates)
{
    map<unsigned char, unsigned int> minPreviousRates;

    if (mapPreviousRates.count('B'))
    {
        const std::vector<const CParkRateVote*>& previousUnitRates = mapPreviousRates.find('B')->second;
        BOOST_FOREACH(const CParkRateVote* parkRateVote, previousUnitRates)
        {
            BOOST_FOREACH(const CParkRate& parkRate, parkRateVote->vParkRate)
            {
                map<unsigned char, unsigned int>::iterator it = minPreviousRates.find(parkRate.nCompactDuration);
                if (it == minPreviousRates.end())
                    minPreviousRates[parkRate.nCompactDuration] = parkRate.nRate;
                else
                    if (parkRate.nRate < it->second)
                        it->second = parkRate.nRate;
            }
        }
    }

    vector<CParkRate>* presult = NULL;
    BOOST_FOREACH(CParkRateVote& parkRateVote, results)
    {
        if (parkRateVote.cUnit == 'B')
        {
            presult = &parkRateVote.vParkRate;
            break;
        }
    }

    if (presult == NULL)
        return true;

    vector<CParkRate>& result = *presult;

    BOOST_FOREACH(CParkRate& parkRate, result)
    {
        const int64 previousMin = minPreviousRates[parkRate.nCompactDuration];
        const int64 duration = parkRate.GetDuration();
        // maximum increase is 1% of annual interest rate
        const int64 secondsInYear = (int64)60 * 60 * 24 * 36525 / 100;
        const int64 blocksInYear = secondsInYear / STAKE_TARGET_SPACING;
        const int64 parkRateEncodedPercentage = COIN_PARK_RATE / CENT;
        assert(parkRateEncodedPercentage < ((int64)1 << 62) / ((int64)1 << MAX_COMPACT_DURATION)); // to avoid overflow
        const int64 maxIncrease = parkRateEncodedPercentage * duration / blocksInYear;

        if (parkRate.nRate > previousMin + maxIncrease)
            parkRate.nRate = previousMin + maxIncrease;
    }

    return true;
}

bool LimitParkRateChangeV2_0(std::vector<CParkRateVote>& results, const std::map<unsigned char, const CParkRateVote*>& mapPreviousVotedRate)
{
    map<unsigned char, unsigned int> mapPreviousRate;
    set<unsigned char> setCompactDuration;

    if (mapPreviousVotedRate.count('B'))
    {
        const CParkRateVote* parkRateVote = mapPreviousVotedRate.find('B')->second;
        BOOST_FOREACH(const CParkRate& parkRate, parkRateVote->vParkRate)
        {
            mapPreviousRate[parkRate.nCompactDuration] = parkRate.nRate;
            setCompactDuration.insert(parkRate.nCompactDuration);
        }
    }

    vector<CParkRate>* presult = NULL;
    BOOST_FOREACH(CParkRateVote& parkRateVote, results)
    {
        if (parkRateVote.cUnit == 'B')
        {
            presult = &parkRateVote.vParkRate;
            break;
        }
    }

    if (presult == NULL)
        return true;

    vector<CParkRate>& result = *presult;

    map<unsigned char, CParkRate*> mapParkRate;
    BOOST_FOREACH(CParkRate& parkRate, result)
    {
        mapParkRate[parkRate.nCompactDuration] = &parkRate;
        setCompactDuration.insert(parkRate.nCompactDuration);
    }

    vector<CParkRate> vNewParkRate;

    BOOST_FOREACH(unsigned char nCompactDuration, setCompactDuration)
    {
        CParkRate* pparkRate = mapParkRate[nCompactDuration];
        int64 votedRate = 0;
        if (pparkRate)
            votedRate = pparkRate->nRate;

        const int64 previous = mapPreviousRate[nCompactDuration];
        const int64 duration = CompactDurationToDuration(nCompactDuration);
        const int64 secondsInYear = (int64)60 * 60 * 24 * 36525 / 100;
        const int64 blocksInYear = secondsInYear / STAKE_TARGET_SPACING;
        const int64 parkRateEncodedPercentage = COIN_PARK_RATE / CENT;

        // maximum increase per block is 0.002% of annual interest rate
        const int64 maxIncreasePercentage = parkRateEncodedPercentage * 2 / 1000; // 0.002 %
        assert(maxIncreasePercentage < ((int64)1 << 62) / ((int64)1 << MAX_COMPACT_DURATION)); // to avoid overflow
        const int64 maxIncrease = maxIncreasePercentage * duration / blocksInYear;

        // maximum decrease per block is 0.004% of annual interest rate
        const int64 maxDecreasePercentage = parkRateEncodedPercentage * 4 / 1000; // 0.004 %
        assert(maxDecreasePercentage < ((int64)1 << 62) / ((int64)1 << MAX_COMPACT_DURATION)); // to avoid overflow
        const int64 maxDecrease = maxDecreasePercentage * duration / blocksInYear;

        if (votedRate > previous + maxIncrease)
        {
            assert(pparkRate);
            pparkRate->nRate = previous + maxIncrease;
        }

        if (votedRate < previous - maxDecrease)
        {
            if (pparkRate)
                pparkRate->nRate = previous - maxDecrease;
            else
                vNewParkRate.push_back(CParkRate(nCompactDuration, previous - maxDecrease));
        }
    }

    BOOST_FOREACH(const CParkRate& parkRate, vNewParkRate)
    {
        vector<CParkRate>::iterator it = result.begin();
        while (it != result.end() && it->nCompactDuration < parkRate.nCompactDuration)
            it++;
        result.insert(it, parkRate);
    }

    return true;
}

bool CalculateParkRateResults(const CVote &vote, const CBlockIndex* pindexprev, int nProtocolVersion, std::vector<CParkRateVote>& vParkRateResult)
{
    vector<CVote> vVote;
    vVote.reserve(PARK_RATE_VOTES);
    vVote.push_back(vote);

    const CBlockIndex *pindex = pindexprev;
    for (int i=0; i<PARK_RATE_VOTES-1 && pindex; i++)
    {
        if (pindex->IsProofOfStake())
            vVote.push_back(pindex->vote);
        pindex = pindex->pprev;
    }

    if (!CalculateParkRateVote(vVote, vParkRateResult))
        return false;

    if (nProtocolVersion >= PROTOCOL_V2_0)
    {
        map<unsigned char, const CParkRateVote*> mapPreviousRate;
        BOOST_FOREACH(const CParkRateVote& previousRate, pindexprev->vParkRateResult)
        {
            mapPreviousRate[previousRate.cUnit] = &previousRate;
        }
        if (!LimitParkRateChangeV2_0(vParkRateResult, mapPreviousRate))
            return false;
    }
    else
    {
        map<unsigned char, vector<const CParkRateVote*> > mapPreviousRates;
        BOOST_FOREACH(unsigned char cUnit, sAvailableUnits)
        {
            if (cUnit != 'S')
                mapPreviousRates[cUnit].reserve(PARK_RATE_PREVIOUS_VOTES);
        }

        pindex = pindexprev;
        for (int i=0; i<PARK_RATE_PREVIOUS_VOTES && pindex; i++)
        {
            BOOST_FOREACH(const CParkRateVote& previousRate, pindex->vParkRateResult)
                mapPreviousRates[previousRate.cUnit].push_back(&previousRate);

            pindex = pindex->pprev;
        }

        if (!LimitParkRateChangeV0_5(vParkRateResult, mapPreviousRates))
            return false;
    }

    return true;
}

bool CParkRateVote::IsValid() const
{
    if (!IsValidCurrency(cUnit))
        return false;

    set<unsigned char> seenCompactDurations;
    BOOST_FOREACH(const CParkRate& parkRate, vParkRate)
    {
        if (!parkRate.IsValid())
            return false;
        if (seenCompactDurations.find(parkRate.nCompactDuration) != seenCompactDurations.end())
            return false;
        seenCompactDurations.insert(parkRate.nCompactDuration);
    }

    return true;
}

bool CParkRate::IsValid() const
{
    if (!CompactDurationRange(nCompactDuration))
        return false;
    if (!ParkRateRange(nRate))
        return false;
    return true;
}

bool CCustodianVote::IsValid(int nProtocolVersion) const
{
    // After v2.0 any unit is valid as a custodian grant
    if (!IsValidUnit(cUnit) || (nProtocolVersion < PROTOCOL_V2_0 && !IsValidCurrency(cUnit)))
        return false;
    if (!MoneyRange(nAmount))
        return false;
    return true;
}

bool CVote::IsValid(int nProtocolVersion) const
{
    set<unsigned char> seenParkVoteUnits;
    BOOST_FOREACH(const CParkRateVote& parkRateVote, vParkRateVote)
    {
        if (!parkRateVote.IsValid())
            return false;
        if (seenParkVoteUnits.count(parkRateVote.cUnit))
            return false;
        seenParkVoteUnits.insert(parkRateVote.cUnit);
    }

    set<CCustodianVote> seenCustodianVotes;
    BOOST_FOREACH(const CCustodianVote& custodianVote, vCustodianVote)
    {
        if (!custodianVote.IsValid(nProtocolVersion))
            return false;

        if (seenCustodianVotes.count(custodianVote))
            return false;
        seenCustodianVotes.insert(custodianVote);
    }

    return true;
}

bool ExtractVotes(const CBlock& block, const CBlockIndex *pindexprev, unsigned int nCount, std::vector<CVote> &vVoteRet)
{
    CVote vote;
    int nProtocolVersion = GetProtocolForNextBlock(pindexprev);
    if (!ExtractVote(block, vote, nProtocolVersion))
        return error("ExtractVotes(): ExtractVote failed");

    if (!vote.IsValid(nProtocolVersion))
        return error("ExtractVotes(): Invalid vote");

    if (!block.GetCoinAge(vote.nCoinAgeDestroyed))
        return error("ExtractVotes(): Unable to get block coin age");

    vVoteRet.reserve(nCount);
    vVoteRet.push_back(vote);

    const CBlockIndex *pindex = pindexprev;
    for (int i=0; i<(int)nCount-1 && pindex; i++)
    {
        if (pindex->IsProofOfStake())
            vVoteRet.push_back(pindex->vote);
        pindex = pindex->pprev;
    }
    return true;
}

class CCustodianVoteCounter
{
public:
    int64 nWeight;
    int nCount;
};

typedef map<CCustodianVote, CCustodianVoteCounter> CustodianVoteCounterMap;
typedef map<CBitcoinAddress, int64> GrantedAmountMap;
typedef map<unsigned char, GrantedAmountMap> GrantedAmountPerUnitMap;

bool GenerateCurrencyCoinBases(const std::vector<CVote> vVote, const std::map<CBitcoinAddress, CBlockIndex*>& mapAlreadyElected, std::vector<CTransaction>& vCurrencyCoinBaseRet)
{
    vCurrencyCoinBaseRet.clear();

    if (vVote.empty())
        return true;

    CustodianVoteCounterMap mapCustodianVoteCounter;
    int64 totalVoteWeight = 0;

    BOOST_FOREACH(const CVote& vote, vVote)
    {
        if (vote.nCoinAgeDestroyed == 0)
            return error("vote with 0 coin age destroyed");

        totalVoteWeight += vote.nCoinAgeDestroyed;

        BOOST_FOREACH(const CCustodianVote& custodianVote, vote.vCustodianVote)
        {
            CCustodianVoteCounter& counter = mapCustodianVoteCounter[custodianVote];
            counter.nWeight += vote.nCoinAgeDestroyed;
            counter.nCount++;
        }
    }

    int64 halfWeight = totalVoteWeight / 2;
    int64 halfCount = vVote.size() / 2;

    map<CBitcoinAddress, int64> mapGrantedAddressWeight;
    GrantedAmountPerUnitMap mapGrantedCustodians;

    BOOST_FOREACH(const CustodianVoteCounterMap::value_type& value, mapCustodianVoteCounter)
    {
        const CCustodianVote &custodianVote = value.first;
        const CCustodianVoteCounter& counter = value.second;

        if (counter.nWeight > halfWeight && counter.nCount > halfCount)
        {
            CBitcoinAddress address = custodianVote.GetAddress();
            if (counter.nWeight > mapGrantedAddressWeight[address])
            {
                mapGrantedAddressWeight[address] = counter.nWeight;
                mapGrantedCustodians[custodianVote.cUnit][address] = custodianVote.nAmount;
            }
        }
    }

    BOOST_FOREACH(const GrantedAmountPerUnitMap::value_type& grantedAmountPerUnit, mapGrantedCustodians)
    {
        unsigned char cUnit = grantedAmountPerUnit.first;
        const GrantedAmountMap& mapGrantedAmount = grantedAmountPerUnit.second;

        CTransaction tx;
        tx.cUnit = cUnit;
        if (cUnit == 'S')
            tx.vin.push_back(CTxIn(0, -2));
        else
            tx.vin.push_back(CTxIn());

        BOOST_FOREACH(const GrantedAmountMap::value_type& grantedAmount, mapGrantedAmount)
        {
            const CBitcoinAddress& address = grantedAmount.first;
            int64 amount = grantedAmount.second;

            const std::map<CBitcoinAddress, CBlockIndex*>::const_iterator it =
                mapAlreadyElected.find(address);
            if (it != mapAlreadyElected.end())
            {
                // Custodian already elected
                continue;
            }

            CScript scriptPubKey;
            scriptPubKey.SetDestination(address.Get());

            tx.vout.push_back(CTxOut(amount, scriptPubKey));
        }

        if (tx.vout.size() != 0)
            vCurrencyCoinBaseRet.push_back(tx);
    }

    return true;
}

int64 GetPremium(int64 nValue, int64 nDuration, unsigned char cUnit, const std::vector<CParkRateVote>& vParkRateResult)
{
    if (!MoneyRange(nValue))
        return 0;
    if (!ParkDurationRange(nDuration))
        return 0;

    BOOST_FOREACH(const CParkRateVote& parkRateVote, vParkRateResult)
    {
        if (parkRateVote.cUnit != cUnit)
            continue;

        vector<CParkRate> vSortedParkRate = parkRateVote.vParkRate;
        sort(vSortedParkRate.begin(), vSortedParkRate.end());

        for (unsigned int i = 0; i < vSortedParkRate.size(); i++)
        {
            const CParkRate& parkRate = vSortedParkRate[i];

            if (nDuration == parkRate.GetDuration())
            {
                CBigNum bnResult = CBigNum(nValue) * CBigNum(parkRate.nRate) / COIN_PARK_RATE;
                if (bnResult < 0 || bnResult > MAX_MONEY)
                    return 0;
                else
                    return (int64)bnResult.getuint64();
            }

            if (nDuration < parkRate.GetDuration())
            {
                if (i == 0)
                    return 0;

                const CParkRate& prevParkRate = vSortedParkRate[i-1];

                CBigNum bnRate(prevParkRate.nRate);
                CBigNum bnInterpolatedRate(nDuration);
                bnInterpolatedRate -= prevParkRate.GetDuration();
                bnInterpolatedRate *= CBigNum(parkRate.nRate) - CBigNum(prevParkRate.nRate);
                bnInterpolatedRate /= CBigNum(parkRate.GetDuration()) - CBigNum(prevParkRate.GetDuration());
                bnRate += bnInterpolatedRate;

                CBigNum bnResult = bnRate * CBigNum(nValue) / COIN_PARK_RATE;
                if (bnResult < 0 || bnResult > MAX_MONEY)
                    return 0;
                else
                    return (int64)bnResult.getuint64();
            }
        }
    }
    return 0;
}

/*
 * Check if the V2.0 protocol is active on the next block
 */
bool IsNuProtocolV20NextBlock(const CBlockIndex* pPrevIndex)
{
    return IsProtocolActiveForNextBlock(pPrevIndex,
            fTestNet ? PROTOCOL_V2_0_TEST_VOTE_TIME : PROTOCOL_V2_0_VOTE_TIME, PROTOCOL_V2_0);
}

/*
 * Calculate what should be the protocol version for the next block.
 * The minimum version that it will return is v0.5 (50000) so checks
 * for older versions should be done separately.
 */
int GetProtocolForNextBlock(const CBlockIndex* pPrevIndex)
{
    int nProtocol = 0;

    if (pPrevIndex != NULL)
        nProtocol = pPrevIndex->nProtocolVersion;

    if (nProtocol < PROTOCOL_V2_0 && IsNuProtocolV20NextBlock(pPrevIndex))
        nProtocol = PROTOCOL_V2_0;

    if (nProtocol < PROTOCOL_V0_5)
        nProtocol = PROTOCOL_V0_5;

    return nProtocol;
}

/*
 * Check if votes pass for the specified protocol version.
 * The new protocol applies to the block AFTER the pPrevIndex.
 */
bool IsProtocolActiveForNextBlock(const CBlockIndex* pPrevIndex,
                           unsigned int nSwitchTime, int nProtocolVersion,
                           int nRequired, int nToCheck)
{
    if (pPrevIndex == NULL)
        return false;
    // if protocol switch time arrived and no majority achieved yet
    if (pPrevIndex->nTime >= nSwitchTime && pPrevIndex->nProtocolVersion < nProtocolVersion)
    {
        int nVotes = 0;
        for (int i = 0; i < nToCheck && nVotes < nRequired && pPrevIndex != NULL; i++)
        {
            // count also votes with higher version number than the target protocol
            if (pPrevIndex->vote.nVersionVote >= nProtocolVersion)
                nVotes++;
            pPrevIndex = pPrevIndex->pprev;
        }

        // votes passed the required threshold?
        return nVotes >= nRequired;
    }
    else if (pPrevIndex->nProtocolVersion >= nProtocolVersion) // if vote already passed
        return true;
    else
        return false;
}

bool CalculateVotedFees(CBlockIndex* pindex)
{
    // pindex data should not be used here because it may be a dummy index (for example on new blocks)
    // pindex->pprev is the first valid block index

    pindex->mapVotedFee.clear();

    CBlockIndex* pvoteindex = pindex;

    map<unsigned char, map<uint32_t, int> > mapFeeVoteCount;

    for (int i = 0; i < FEE_VOTES; i++)
    {
        BOOST_FOREACH(const unsigned char cUnit, sAvailableUnits)
        {
            uint32_t vote;
            if (pvoteindex)
            {
                if (pvoteindex->vote.mapFeeVote.Has(cUnit))
                    vote = pvoteindex->vote.mapFeeVote[cUnit];
                else
                    vote = GetDefaultFee(cUnit);
            }
            else
                vote = GetDefaultFee(cUnit);

            mapFeeVoteCount[cUnit][vote]++;
        }
        if (pvoteindex)
            pvoteindex = pvoteindex->pprev;
    }

    BOOST_FOREACH(const unsigned char cUnit, sAvailableUnits)
    {
        // calculate the median fee
        const int half = FEE_VOTES / 2;
        int total = 0;

        BOOST_FOREACH(PAIRTYPE(const uint32_t, int)& item, mapFeeVoteCount[cUnit])
        {
            const uint32_t& vote = item.first;
            const int& count = item.second;

            total += count;
            if (total >= half)
            {
                pindex->mapVotedFee[cUnit] = vote;
                break;
            }
        }
    }

    return true;
}
