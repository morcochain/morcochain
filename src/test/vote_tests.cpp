#include <boost/test/unit_test.hpp>
#include <algorithm>

#include "main.h"
#include "vote.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(vote_tests)

BOOST_AUTO_TEST_CASE(reload_vote_from_script_tests)
{
    CVote vote;

    CCustodianVote custodianVote;
    CBitcoinAddress custodianAddress(CKeyID(123465), 'B');
    custodianVote.SetAddress(custodianAddress);
    custodianVote.nAmount = 100 * COIN;
    vote.vCustodianVote.push_back(custodianVote);

    CCustodianVote custodianVote2;
    CBitcoinAddress custodianAddress2(CKeyID(555555555), 'B');
    custodianVote2.SetAddress(custodianAddress2);
    custodianVote2.nAmount = 5.5 * COIN;
    vote.vCustodianVote.push_back(custodianVote2);

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'B';
    parkRateVote.vParkRate.push_back(CParkRate(13, 3));
    parkRateVote.vParkRate.push_back(CParkRate(14, 6));
    parkRateVote.vParkRate.push_back(CParkRate(15, 13));
    vote.vParkRateVote.push_back(parkRateVote);

    vote.vMotion.push_back(uint160(123456));
    vote.vMotion.push_back(uint160(3333));

    CScript script = vote.ToScript(PROTOCOL_VERSION);

    CVote voteResult;
    BOOST_CHECK(ExtractVote(script, voteResult, PROTOCOL_VERSION));

#undef CHECK_VOTE_EQUAL
#define CHECK_VOTE_EQUAL(value) BOOST_CHECK(voteResult.value == vote.value);
    CHECK_VOTE_EQUAL(vCustodianVote.size());
    for (size_t i=0; i<vote.vCustodianVote.size(); i++)
    {
        CHECK_VOTE_EQUAL(vCustodianVote[i].cUnit);
        CHECK_VOTE_EQUAL(vCustodianVote[i].hashAddress);
        CHECK_VOTE_EQUAL(vCustodianVote[i].GetAddress().ToString());
        CHECK_VOTE_EQUAL(vCustodianVote[i].nAmount);
    }
    BOOST_CHECK_EQUAL(custodianAddress.ToString(), vote.vCustodianVote[0].GetAddress().ToString());
    BOOST_CHECK_EQUAL(custodianAddress2.ToString(), vote.vCustodianVote[1].GetAddress().ToString());

    CHECK_VOTE_EQUAL(vParkRateVote.size());
    for (size_t i=0; i<vote.vParkRateVote.size(); i++)
    {
        CHECK_VOTE_EQUAL(vParkRateVote[i].cUnit);
        CHECK_VOTE_EQUAL(vParkRateVote[i].vParkRate.size());
        for (size_t j=0; j<vote.vParkRateVote[i].vParkRate.size(); j++)
        {
            CHECK_VOTE_EQUAL(vParkRateVote[i].vParkRate[j].nCompactDuration);
            CHECK_VOTE_EQUAL(vParkRateVote[i].vParkRate[j].nRate);
        }
    }

    CHECK_VOTE_EQUAL(vMotion.size());
    CHECK_VOTE_EQUAL(vMotion[0]);
    CHECK_VOTE_EQUAL(vMotion[1]);
#undef CHECK_VOTE_EQUAL
}

BOOST_AUTO_TEST_CASE(reload_park_rates_from_script_tests)
{
    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'B';
    parkRateVote.vParkRate.push_back(CParkRate(13, 3));
    parkRateVote.vParkRate.push_back(CParkRate(14, 6));
    parkRateVote.vParkRate.push_back(CParkRate(15, 13));

    CScript script = parkRateVote.ToParkRateResultScript();
    BOOST_CHECK(IsParkRateResult(script));

    CParkRateVote parkRateVoteResult;
    BOOST_CHECK(ExtractParkRateResult(script, parkRateVoteResult));

#undef CHECK_PARK_RATE_EQUAL
#define CHECK_PARK_RATE_EQUAL(value) BOOST_CHECK(parkRateVoteResult.value == parkRateVote.value);
    CHECK_PARK_RATE_EQUAL(cUnit);
    CHECK_PARK_RATE_EQUAL(vParkRate.size());
    for (size_t i=0; i<parkRateVote.vParkRate.size(); i++)
    {
        CHECK_PARK_RATE_EQUAL(vParkRate[i].nCompactDuration);
        CHECK_PARK_RATE_EQUAL(vParkRate[i].nRate);
    }
#undef CHECK_PARK_RATE_EQUAL
}

template< class T >
static void shuffle(vector<T> v)
{
    random_shuffle(v.begin(), v.end());
}

BOOST_AUTO_TEST_CASE(rate_calculation_from_votes)
{
    vector<CVote> vVote;
    vector<CParkRateVote> results;

    // Result of empty vote is empty
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(0, results.size());

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'B';
    parkRateVote.vParkRate.push_back(CParkRate( 8, 100));

    CVote vote;
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 1000;

    vVote.push_back(vote);

    // Single vote: same result as vote
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(100, results[0].vParkRate[0].nRate);

    // New vote with same weight and bigger rate
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(8, 200));
    vote.vParkRateVote.push_back(parkRateVote);
    vVote.push_back(vote);

    // Two votes of same weight, the median is the first one
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(100, results[0].vParkRate[0].nRate);

    // Vote 2 has a little more weight
    vVote[1].nCoinAgeDestroyed = 1001;

    // Each coin age has a vote. So the median is the second vote rate.
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(200, results[0].vParkRate[0].nRate);

    // New vote with small weight and rate between the 2 first
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(8, 160));
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 3;
    vVote.push_back(vote);

    // The median is the middle rate
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(160, results[0].vParkRate[0].nRate);

    // New vote with another duration
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(9, 300));
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 100;
    vVote.push_back(vote);

    // It votes for 0 on duration 8, so the result is back to 100
    // On duration 9 everybody else vote for 0, so the median is 0, so there's no result
    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    BOOST_CHECK_EQUAL(  1, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(100, results[0].vParkRate[0].nRate);

    // New vote with multiple durations unordered
    parkRateVote.vParkRate.clear();
    vote.vParkRateVote.clear();
    parkRateVote.vParkRate.push_back(CParkRate(13, 500));
    parkRateVote.vParkRate.push_back(CParkRate(9, 400));
    parkRateVote.vParkRate.push_back(CParkRate(8, 200));
    vote.vParkRateVote.push_back(parkRateVote);
    vote.nCoinAgeDestroyed = 2050;
    vVote.push_back(vote);

    BOOST_CHECK(CalculateParkRateVote(vVote, results));
    BOOST_CHECK_EQUAL(  1, results.size());
    // On duration 8:
    // Vote weights: 0: 100, 100: 1000, 160: 3, 200: 3051
    // So median is 200
    BOOST_CHECK_EQUAL(  8, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(200, results[0].vParkRate[0].nRate);
    // On duration 9:
    // Vote weights: 0: 2004, 300: 100, 400: 2050
    // So median is 300
    BOOST_CHECK_EQUAL(  9, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(300, results[0].vParkRate[1].nRate);
    // On duration 13: only last vote is positive and it has not the majority, so median is 0
    BOOST_CHECK_EQUAL(  2, results[0].vParkRate.size());

    // Shuffle all the votes
    srand(1234);
    BOOST_FOREACH(const CVote& vote, vVote)
    {
        BOOST_FOREACH(const CParkRateVote& parkRateVote, vote.vParkRateVote)
            shuffle(parkRateVote.vParkRate);
        shuffle(vote.vParkRateVote);
    }
    shuffle(vVote);

    // The result should not be changed
    vector<CParkRateVote> newResults;
    BOOST_CHECK(CalculateParkRateVote(vVote, newResults));
    BOOST_CHECK(results == newResults);
}

BOOST_AUTO_TEST_CASE(rate_limitation_v05)
{
    vector<CParkRateVote> baseResults, results;
    map<unsigned char, vector<const CParkRateVote*> > previousRates;

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'B';
    parkRateVote.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN)); // 1 year
    baseResults.push_back(parkRateVote);

    int64 maxIncreaseDuration15 = pow(2, 15) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration18 = pow(2, 18) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration19 = pow(2, 19) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;

    // Without previous rates, the previous rates are all considered 0 so the rate increase is limited
    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With an empty previous rate (meaning the rates are all 0), the rate increase is limited
    CParkRateVote previousRate;
    previousRate.cUnit = 'B';
    previousRates['B'].push_back(&previousRate);

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With some previous rates
    previousRate.vParkRate.push_back(CParkRate(10, 100 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(15, 3 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 950 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(3 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN, results[0].vParkRate[2].nRate);

    // With multiple previous rates
    previousRate.vParkRate.push_back(CParkRate(15, 2 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 150 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(2 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(150 * COIN_PARK_RATE / COIN + maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // Decrease is not limited
    previousRate.vParkRate.clear();
    previousRate.vParkRate.push_back(CParkRate(10, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN));
    results.clear();
    parkRateVote.vParkRate.clear();
    parkRateVote.vParkRate.push_back(CParkRate(15, 10 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1 * COIN_PARK_RATE / COIN)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 0 * COIN_PARK_RATE / COIN)); // 1 year
    results.push_back(parkRateVote);

    BOOST_CHECK(LimitParkRateChangeV0_5(results, previousRates));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(10 * COIN_PARK_RATE / COIN, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(1 * COIN_PARK_RATE / COIN, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(0 * COIN_PARK_RATE / COIN, results[0].vParkRate[2].nRate);
}

BOOST_AUTO_TEST_CASE(rate_limitation_v06)
{
    vector<CParkRateVote> baseResults, results;
    map<unsigned char, const CParkRateVote*> mapPreviousRate;

    CParkRateVote parkRateVote;
    parkRateVote.cUnit = 'B';
    parkRateVote.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN)); // 1 year
    baseResults.push_back(parkRateVote);

    int64 maxIncreaseDuration15 = 0.002 * pow(2, 15) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration18 = 0.002 * pow(2, 18) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxIncreaseDuration19 = 0.002 * pow(2, 19) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;

    // Without previous rates, the previous rates are all considered 0 so the rate increase is limited
    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With an empty previous rate (meaning all rates were 0), the rate increase is limited
    CParkRateVote previousRate;
    previousRate.cUnit = 'B';
    mapPreviousRate['B'] = &previousRate;

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration19, results[0].vParkRate[2].nRate);

    // With some previous rates
    previousRate.vParkRate.push_back(CParkRate(15, 3 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 999.9 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(3 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN, results[0].vParkRate[2].nRate);

    // With multiple previous rates
    previousRate.vParkRate.push_back(CParkRate(15, 2 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 150 * COIN_PARK_RATE / COIN));

    results = baseResults;
    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(2 * COIN_PARK_RATE / COIN + maxIncreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(maxIncreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(150 * COIN_PARK_RATE / COIN + maxIncreaseDuration19, results[0].vParkRate[2].nRate);


    // Check decrease limits
    int64 maxDecreaseDuration15 = 0.004 * pow(2, 15) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxDecreaseDuration18 = 0.004 * pow(2, 18) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;
    int64 maxDecreaseDuration19 = 0.004 * pow(2, 19) / 365.25 / 24 / 60 / 60 * STAKE_TARGET_SPACING * COIN_PARK_RATE / 100;

    previousRate.vParkRate.clear();
    previousRate.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN));
    results.clear();
    parkRateVote.vParkRate.clear();
    parkRateVote.vParkRate.push_back(CParkRate(15, 10 * COIN_PARK_RATE / COIN)); // 1 month
    parkRateVote.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration18 + 5)); // 6 months
    parkRateVote.vParkRate.push_back(CParkRate(19, 0 * COIN_PARK_RATE / COIN)); // 1 year
    results.push_back(parkRateVote);

    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration18 + 5, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration19, results[0].vParkRate[2].nRate);

    // Decrease limit when no park rates are voted
    previousRate.vParkRate.clear();
    previousRate.vParkRate.push_back(CParkRate(15, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(18, 1000 * COIN_PARK_RATE / COIN));
    previousRate.vParkRate.push_back(CParkRate(19, 1000 * COIN_PARK_RATE / COIN));
    results.clear();
    parkRateVote.vParkRate.clear();
    parkRateVote.vParkRate.push_back(CParkRate(18, 0)); // 6 months
    results.push_back(parkRateVote);

    BOOST_CHECK(LimitParkRateChangeV2_0(results, mapPreviousRate));
    BOOST_CHECK_EQUAL(   1, results.size());
    BOOST_CHECK_EQUAL(   3, results[0].vParkRate.size());
    BOOST_CHECK_EQUAL(  15, results[0].vParkRate[0].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration15, results[0].vParkRate[0].nRate);
    BOOST_CHECK_EQUAL(  18, results[0].vParkRate[1].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration18, results[0].vParkRate[1].nRate);
    BOOST_CHECK_EQUAL(  19, results[0].vParkRate[2].nCompactDuration);
    BOOST_CHECK_EQUAL(1000 * COIN_PARK_RATE / COIN - maxDecreaseDuration19, results[0].vParkRate[2].nRate);

}

BOOST_AUTO_TEST_CASE(vote_validity_tests)
{
    CVote vote;
    CParkRateVote parkRateVote;

    // An empty vote is valid
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // A park rate vote on share is invalid
    parkRateVote.cUnit = 'S';
    vote.vParkRateVote.push_back(parkRateVote);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // A park rate vote on unknown unit is invalid
    vote.vParkRateVote[0].cUnit = 'A';
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // A park rate vote on nubits is valid
    vote.vParkRateVote[0].cUnit = 'B';
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Two park rate vote on nubits is invalid
    parkRateVote.cUnit = 'B';
    vote.vParkRateVote.push_back(parkRateVote);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // Park rate with duration and 0 rate is valid
    vote.vParkRateVote.erase(vote.vParkRateVote.end());
    CParkRate parkRate;
    parkRate.nCompactDuration = 0;
    parkRate.nRate = 0;
    vote.vParkRateVote[0].vParkRate.push_back(parkRate);
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Two valid park rates
    parkRate.nCompactDuration = 4;
    parkRate.nRate = 100;
    vote.vParkRateVote[0].vParkRate.push_back(parkRate);
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Two times the same duration is invalid
    parkRate.nCompactDuration = 4;
    parkRate.nRate = 200;
    vote.vParkRateVote[0].vParkRate.push_back(parkRate);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));
    vote.vParkRateVote[0].vParkRate.pop_back();

    // A valid custodian vote
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'B';
    custodianVote.hashAddress = uint160(1);
    custodianVote.nAmount = 8 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));

    // Another unit is invalid
    vote.vCustodianVote[0].cUnit = 'A';
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));
    // NuShares grants are invalid pre v2.0
    vote.vCustodianVote[0].cUnit = 'S';
    BOOST_CHECK(!vote.IsValid(PROTOCOL_V0_5));
    // But valid after v2.0
    BOOST_CHECK(vote.IsValid(PROTOCOL_V2_0));
    vote.vCustodianVote[0].cUnit = 'B';

    // Voting for the same custodian and amount twice is invalid
    vote.vCustodianVote.push_back(custodianVote);
    BOOST_CHECK(!vote.IsValid(PROTOCOL_VERSION));

    // If the amount is different it is valid
    vote.vCustodianVote[0].nAmount++;
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));
    vote.vCustodianVote[0].nAmount--;

    // If the address is different it is valid
    vote.vCustodianVote[0].hashAddress++;
    BOOST_CHECK(vote.IsValid(PROTOCOL_VERSION));
    vote.vCustodianVote[0].hashAddress--;
}

void printVotes(vector<CVote> vVote)
{
    BOOST_FOREACH(const CVote& vote, vVote)
        BOOST_FOREACH(const CCustodianVote& custodianVote, vote.vCustodianVote)
            printf("addr=%"PRI64u", amount=%"PRI64d", weight=%"PRI64d", unit=%c\n", custodianVote.hashAddress.Get64(), custodianVote.nAmount, vote.nCoinAgeDestroyed, custodianVote.cUnit);
    printf("\n");
}

BOOST_AUTO_TEST_CASE(create_currency_coin_bases)
{
    vector<CVote> vVote;
    std::map<CBitcoinAddress, CBlockIndex*> mapAlreadyElected;

    // Zero vote results in no new currency
    vector<CTransaction> vCurrencyCoinBase;
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a vote without custodian vote
    CVote vote;
    vote.nCoinAgeDestroyed = 1000;
    vVote.push_back(vote);

    // Still no currency created
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a custodian vote with the same coin age
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'B';
    custodianVote.hashAddress = uint160(1);
    custodianVote.nAmount = 8 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    vVote.push_back(vote);

    // Still no currency created
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // The last vote has a little more weight
    vVote.back().nCoinAgeDestroyed++;

    // Still no currency created because this vote does not have the majority of blocks (we have 2 votes)
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a 3rd vote for the same custodian
    vVote.back().nCoinAgeDestroyed--;
    vote.nCoinAgeDestroyed = 1;
    vVote.push_back(vote);

    // This custodian should win and currency should be created
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());
    CTransaction tx = vCurrencyCoinBase[0];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('B', tx.cUnit);
    BOOST_CHECK_EQUAL(1, tx.vout.size());
    BOOST_CHECK_EQUAL(8 * COIN, tx.vout[0].nValue);
    CTxDestination address;
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // This custodian has already been elected
    mapAlreadyElected[CBitcoinAddress(address, 'B')] = new CBlockIndex;

    // He should not receive any new currency
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(0, vCurrencyCoinBase.size());

    // Add a vote for another custodian to the existing votes
    custodianVote.hashAddress = uint160(2);
    custodianVote.nAmount = 5 * COIN;
    vVote[0].vCustodianVote.push_back(custodianVote);
    vVote[1].vCustodianVote.push_back(custodianVote);

    // And clear the already elected
    mapAlreadyElected.clear();

    // Both should receive new currency
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());
    tx = vCurrencyCoinBase[0];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('B', tx.cUnit);
    BOOST_CHECK_EQUAL(2, tx.vout.size());
    BOOST_CHECK_EQUAL(8 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[1].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[1].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(2).ToString(), boost::get<CKeyID>(address).ToString());

    // But if they have the same address
    uint160 hashAddress = vVote[1].vCustodianVote.front().hashAddress;
    vVote[0].vCustodianVote.back().hashAddress = hashAddress;
    vVote[1].vCustodianVote.back().hashAddress = hashAddress;
    vVote[2].vCustodianVote.back().hashAddress = hashAddress;

//    printVotes(vVote);

    // Only the amount with the highest coin age is granted
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());

    tx = vCurrencyCoinBase[0];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('B', tx.cUnit);
    BOOST_CHECK_EQUAL(1, tx.vout.size());
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // NuShare grants are valid now
    vVote[0].vCustodianVote.back().cUnit = 'S';
    vVote[1].vCustodianVote.back().cUnit = 'S';
    vVote[2].vCustodianVote.back().cUnit = 'S';
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(1, vCurrencyCoinBase.size());
    tx = vCurrencyCoinBase[0];
    tx.nTime = 2000000000; // set a time that is after V06 switch time
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK_EQUAL('S', tx.cUnit);
    BOOST_CHECK_GE(2, tx.vout.size()); // NSR currency coin base has an empty first output
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // Check if both NSR and NBT grants can happen
    vVote[2].vCustodianVote.back().cUnit = 'B';

    // Should NSR and NBT
    BOOST_CHECK(GenerateCurrencyCoinBases(vVote, mapAlreadyElected, vCurrencyCoinBase));
    BOOST_CHECK_EQUAL(2, vCurrencyCoinBase.size());
    tx = vCurrencyCoinBase[0];
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK_EQUAL('B', tx.cUnit);
    BOOST_CHECK_EQUAL(1, tx.vout.size());
    BOOST_CHECK_EQUAL(8 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());
    tx = vCurrencyCoinBase[1];
    tx.nTime = 2000000000; // set a time that is after V06 switch time
    BOOST_CHECK(tx.IsCustodianGrant());
    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK_EQUAL('S', tx.cUnit);
    BOOST_CHECK_GE(2, tx.vout.size()); // NSR currency coin base has an empty first output
    BOOST_CHECK_EQUAL(5 * COIN, tx.vout[0].nValue);
    BOOST_CHECK(ExtractDestination(tx.vout[0].scriptPubKey, address));
    BOOST_CHECK_EQUAL(uint160(1).ToString(), boost::get<CKeyID>(address).ToString());

    // Unknown units should fail but we can only check if the CVote is valid or not
    vVote[1].vCustodianVote.back().cUnit = '?';
    BOOST_CHECK(!vVote[1].IsValid(PROTOCOL_VERSION));
}

BOOST_AUTO_TEST_CASE(premium_calculation)
{
    vector<CParkRateVote> vParkRateResult;
    CParkRateVote parkRateResult;
    parkRateResult.cUnit = 'B';
    parkRateResult.vParkRate.push_back(CParkRate( 2,  5 * COIN_PARK_RATE / COIN));
    parkRateResult.vParkRate.push_back(CParkRate( 5, 50 * COIN_PARK_RATE / COIN));
    parkRateResult.vParkRate.push_back(CParkRate( 3, 10 * COIN_PARK_RATE / COIN));
    parkRateResult.vParkRate.push_back(CParkRate(10,  1 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(12,  2 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(13,  5 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(15, 50 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(16, 40 * COIN_PARK_RATE));
    parkRateResult.vParkRate.push_back(CParkRate(17, 50 * COIN_PARK_RATE));
    vParkRateResult.push_back(parkRateResult);

    // Below minimum rate
    BOOST_CHECK_EQUAL( 0, GetPremium( 1 * COIN, 0, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium( 1 * COIN, 1, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium( 1 * COIN, 3, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(10 * COIN, 3, 'B', vParkRateResult));

    // Above maximum rate
    BOOST_CHECK_EQUAL( 0, GetPremium(   1 * COIN, (1<<17)+1, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(1000 * COIN, (1<<17)+1, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(   1 * COIN,   1000000, 'B', vParkRateResult));

    // Exact durations
    BOOST_CHECK_EQUAL( 5, GetPremium(1   * COIN,  4, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(10, GetPremium(2   * COIN,  4, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 0, GetPremium(0.1 * COIN,  4, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(10, GetPremium(1   * COIN,  8, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(99, GetPremium(9.9 * COIN,  8, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(50, GetPremium(1   * COIN, 32, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 1 * COIN, GetPremium(1 * COIN,  1024, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 2 * COIN, GetPremium(1 * COIN,  4096, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 5 * COIN, GetPremium(1 * COIN,  8192, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(50 * COIN, GetPremium(1 * COIN, 1<<15, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(40 * COIN, GetPremium(1 * COIN, 1<<16, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(50 * COIN, GetPremium(1 * COIN, 1<<17, 'B', vParkRateResult));

    // Intermediate durations
    BOOST_CHECK_EQUAL( 6, GetPremium(1   * COIN,  5, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 9, GetPremium(1.5 * COIN,  5, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(25, GetPremium(4   * COIN,  5, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 8, GetPremium(1   * COIN,  7, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL( 8, GetPremium(1   * COIN,  7, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(21, GetPremium(1   * COIN, 15, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL(38, GetPremium(1   * COIN, 25, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL((int64)(3.39453125 * COIN), GetPremium(1 * COIN, 6000, 'B', vParkRateResult));
    BOOST_CHECK_EQUAL((int64)(49.69482421875 * COIN), GetPremium(1 * COIN, (1<<15) + 1000, 'B', vParkRateResult));
}

BOOST_AUTO_TEST_CASE(effective_park_rates_delayed_after_protocol_v2_0)
{
    CBlockIndex *pindexBase = NULL;

    int64 nValue = 1000 * COIN;
    unsigned char cUnit = 'B';
    int nCompactDuration = 5;
    int nDuration = (1 << nCompactDuration);

    // Create 100 previous blocks
    CBlockIndex *pindex = NULL;
    for (int i = 0; i < 100; i++)
    {
        if (i == 0)
        {
            pindex = new CBlockIndex;
            pindexBase = pindex;
        }
        else
        {
            pindex->pprev = new CBlockIndex;
            pindex->pprev->pnext = pindex;
            pindex = pindex->pprev;
        }

        vector<CParkRateVote> vParkRateResult;
        CParkRateVote parkRateResult;
        parkRateResult.cUnit = cUnit;
        parkRateResult.vParkRate.push_back(CParkRate(nCompactDuration, (i+1) * COIN_PARK_RATE / COIN));
        vParkRateResult.push_back(parkRateResult);
        pindex->vParkRateResult = vParkRateResult;
    }

    pindexBase->nProtocolVersion = PROTOCOL_V0_5;
    int64 expectedPremium = GetPremium(nValue, nDuration, cUnit, pindexBase->vParkRateResult);
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetPremium(nValue, nDuration, cUnit));
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetNextPremium(nValue, nDuration, cUnit));

    pindexBase->nProtocolVersion = PROTOCOL_V2_0;
    CBlockIndex* pindexEffective = pindexBase;
    for (int i = 0; i < 60; i++)
        pindexEffective = pindexEffective->pprev;
    expectedPremium = GetPremium(nValue, nDuration, cUnit, pindexEffective->vParkRateResult);
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetPremium(nValue, nDuration, cUnit));

    pindexEffective = pindexEffective->pnext;
    expectedPremium = GetPremium(nValue, nDuration, cUnit, pindexEffective->vParkRateResult);
    BOOST_CHECK_EQUAL(expectedPremium, pindexBase->GetNextPremium(nValue, nDuration, cUnit));
}

BOOST_AUTO_TEST_CASE(vote_v1_unserialization)
{
    // Serialized with vote v1 code:
    /* {
    CVote vote;
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'B';
    custodianVote.hashAddress = uint160(123465);
    custodianVote.nAmount = 100 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    printf("%s\n", vote.ToScript().ToString().c_str());
    printf("%s\n", HexStr(vote.ToScript()).c_str());
    } */
    vector<unsigned char> voteV1String = ParseHex("6a513701000000014249e201000000000000000000000000000000000040420f0000000000000000000000000000000000000000000000000000");

    CScript voteV1Script(voteV1String.begin(), voteV1String.end());

    CVote vote;
    BOOST_CHECK(ExtractVote(voteV1Script, vote, PROTOCOL_VERSION));

    BOOST_CHECK_EQUAL(CBitcoinAddress(CKeyID(123465), 'B').ToString(), vote.vCustodianVote[0].GetAddress().ToString());
}

BOOST_AUTO_TEST_CASE(vote_before_multi_motion_unserialization)
{
    // Serialized with v0.4.2 vote code:
    /* {
    CVote vote;
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'B';
    custodianVote.hashAddress = uint160(123465);
    custodianVote.nAmount = 100 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    vote.hashMotion = uint160("3f786850e387550fdab836ed7e6dc881de23001b");
    printf("%s\n", vote.ToScript().ToString().c_str());
    printf("%s\n", HexStr(vote.ToScript()).c_str());
    } */
    vector<unsigned char> oldVoteString = ParseHex("6a5138089d000001420049e201000000000000000000000000000000000040420f0000000000001b0023de81c86d7eed36b8da0f5587e35068783f");

    CScript oldVoteScript(oldVoteString.begin(), oldVoteString.end());

    CVote vote;
    BOOST_CHECK(ExtractVote(oldVoteScript, vote, PROTOCOL_VERSION));

    BOOST_CHECK_EQUAL(1, vote.vMotion.size());
    BOOST_CHECK_EQUAL(uint160("3f786850e387550fdab836ed7e6dc881de23001b").ToString(), vote.vMotion[0].ToString());
}

BOOST_AUTO_TEST_CASE(protocol_voting)
{
    unsigned int PROTOCOL_SWITCH_TIME = 100;
    int PROTOCOL_VOTES_REQ = 70;
    int PROTOCOL_VOTES_TOTAL = 80;
    int PROTOCOL_VERSION = PROTOCOL_V2_0;
    CBlockIndex* pIndexFirst = NULL;
    CBlockIndex* pIndexBest = NULL;
    CBlockIndex* pCurrent = NULL;
    // Create test indexes
    for(int i = 0; i < 300; i++)
    {
        CBlockIndex* pPrevIndex = pIndexBest;
        pIndexBest = new CBlockIndex();
        pIndexBest->nHeight = i;
        pIndexBest->nTime = i + 1;
        pIndexBest->pprev = pPrevIndex;
        if (pPrevIndex)
            pPrevIndex->pnext = pIndexBest;
        if (pIndexFirst == NULL)
            pIndexFirst = pIndexBest;
    }
    // Vote passes on switch time as majority achieved
    for(pCurrent = pIndexFirst; pCurrent != NULL; pCurrent = pCurrent->pnext)
    {
        pCurrent->vote.nVersionVote = PROTOCOL_VERSION;
        // Sets the effective protocol as the previous block
        pCurrent->nProtocolVersion = GetProtocolForNextBlock(pCurrent->pprev);
        // Simulate protocol voting, normaly in CalculateEffectiveProtocol()
        if (IsProtocolActiveForNextBlock(pCurrent->pprev, PROTOCOL_SWITCH_TIME, PROTOCOL_VERSION, PROTOCOL_VOTES_REQ, PROTOCOL_VOTES_TOTAL))
            pCurrent->nProtocolVersion = PROTOCOL_VERSION;
        // After the switch time, check that we have the new protocol
        if (pCurrent->pprev != NULL && pCurrent->pprev->nTime >= PROTOCOL_SWITCH_TIME)
        {
            BOOST_CHECK_EQUAL(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        }
        else // Before the switch time we are still using the old protocol
        {
            BOOST_CHECK_LT(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        }
    }
    // Reset
    for(pCurrent = pIndexFirst; pCurrent != NULL; pCurrent = pCurrent->pnext)
    {
        pCurrent->vote.nVersionVote = 0;
        pCurrent->nProtocolVersion = 0;
    }
    // Vote will pass after switch time as majority is not achieved immediately
    BOOST_CHECK_LT(pIndexBest->nProtocolVersion, PROTOCOL_VERSION);
    pCurrent = pIndexFirst;
    // First 100 blocks don't vote for new protocol
    for(int i = 0; i < 99; i++)
    {
        pCurrent->vote.nVersionVote = 0;
        // Sets the effective protocol as the previous block
        pCurrent->nProtocolVersion = GetProtocolForNextBlock(pCurrent->pprev);
        // Simulate protocol voting, normaly in CalculateEffectiveProtocol()
        if (IsProtocolActiveForNextBlock(pCurrent->pprev, PROTOCOL_SWITCH_TIME, PROTOCOL_VERSION, PROTOCOL_VOTES_REQ, PROTOCOL_VOTES_TOTAL))
            pCurrent->nProtocolVersion = PROTOCOL_VERSION;
        BOOST_CHECK_LT(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        pCurrent = pCurrent->pnext;
    }
    // The rest of the blocks vote for the new protocol so on time 170 we switch protocol
    for(; pCurrent != NULL; pCurrent = pCurrent->pnext)
    {
        pCurrent->vote.nVersionVote = PROTOCOL_VERSION;
        // Sets the effective protocol as the previous block
        pCurrent->nProtocolVersion = GetProtocolForNextBlock(pCurrent->pprev);
        // Simulate protocol voting, normaly in CalculateEffectiveProtocol()
        if (IsProtocolActiveForNextBlock(pCurrent->pprev, PROTOCOL_SWITCH_TIME, PROTOCOL_VERSION, PROTOCOL_VOTES_REQ, PROTOCOL_VOTES_TOTAL))
            pCurrent->nProtocolVersion = PROTOCOL_VERSION;
        if (pCurrent->nTime >= 170)
            BOOST_CHECK_EQUAL(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
        else
            BOOST_CHECK_LT(pCurrent->nProtocolVersion, PROTOCOL_VERSION);
    }
    BOOST_CHECK_EQUAL(pIndexBest->nProtocolVersion, PROTOCOL_VERSION);
    // Free mem
    while(pIndexBest != NULL)
    {
        CBlockIndex* pIndex = pIndexBest;
        pIndexBest = pIndexBest->pprev;
        delete pIndex;
    }
}

BOOST_AUTO_TEST_CASE(vote_v50000_unserialization)
{
    // Serialized with v0.5.2 vote code:
    /* {
    CVote vote;
    CCustodianVote custodianVote;
    custodianVote.cUnit = 'B';
    custodianVote.hashAddress = uint160(123465);
    custodianVote.nAmount = 100 * COIN;
    vote.vCustodianVote.push_back(custodianVote);
    vote.vMotion.push_back(uint160(123456));
    vote.vMotion.push_back(uint160(3333));
    printf("%s\n", vote.ToScript().ToString().c_str());
    printf("%s\n", HexStr(vote.ToScript()).c_str());
    } */
    vector<unsigned char> oldVoteString = ParseHex("6a514c4d50c3000001420049e201000000000000000000000000000000000040420f0000000000000240e2010000000000000000000000000000000000050d000000000000000000000000000000000000");

    CScript oldVoteScript(oldVoteString.begin(), oldVoteString.end());

    CVote vote;
    BOOST_CHECK(ExtractVote(oldVoteScript, vote, PROTOCOL_VERSION));

    BOOST_CHECK_EQUAL(0, vote.mapFeeVote.size());
}

vector<CBlockIndex*> feeVoteIndexes;
int lastFeeVoteIndex = -1;

void AddFeeVoteBlocks(int nCount, int64 nFeeVoteS, int64 nFeeVoteB)
{
    for (int i = 0; i < nCount; i++)
    {
        CBlockIndex* pindex = new CBlockIndex();
        if (lastFeeVoteIndex >= 0)
        {
            pindex->pprev = feeVoteIndexes[lastFeeVoteIndex];
            pindex->pprev->pnext = pindex;
        }
        feeVoteIndexes.push_back(pindex);
        lastFeeVoteIndex++;
        pindex->nHeight = lastFeeVoteIndex;
        if (nFeeVoteS != -1)
            pindex->vote.mapFeeVote['S'] = nFeeVoteS;
        if (nFeeVoteB != -1)
            pindex->vote.mapFeeVote['B'] = nFeeVoteB;
        BOOST_CHECK(CalculateVotedFees(pindex));
    }
}

#define CHECK_VOTED_MIN_FEE(nIndex, nExpectedSFee, nExpectedBFee) { \
    BOOST_CHECK_EQUAL(nExpectedSFee, feeVoteIndexes[nIndex]->GetVotedMinFee('S')); \
    BOOST_CHECK_EQUAL(nExpectedBFee, feeVoteIndexes[nIndex]->GetVotedMinFee('B')); \
}

#define CHECK_EFFECTIVE_MIN_FEE(nIndex, nExpectedSFee, nExpectedBFee) { \
    BOOST_CHECK_EQUAL(nExpectedSFee, feeVoteIndexes[nIndex]->GetMinFee('S')); \
    BOOST_CHECK_EQUAL(nExpectedBFee, feeVoteIndexes[nIndex]->GetMinFee('B')); \
}

#define CHECK_SAFE_MIN_FEE(nIndex, nExpectedSFee, nExpectedBFee) { \
    BOOST_CHECK_EQUAL(nExpectedSFee, feeVoteIndexes[nIndex]->GetSafeMinFee('S')); \
    BOOST_CHECK_EQUAL(nExpectedBFee, feeVoteIndexes[nIndex]->GetSafeMinFee('B')); \
}

void ResetFeeVoteBlocks()
{
    BOOST_FOREACH(CBlockIndex* pindex, feeVoteIndexes)
        delete pindex;
    feeVoteIndexes.clear();
    lastFeeVoteIndex = -1;
}

BOOST_AUTO_TEST_CASE(fee_vote_calculation)
{
    // 3000 blocks not including any fee vote
    AddFeeVoteBlocks(3000,     -1, -1);
    // 3500 blocks voting for a new NSR fee and no vote for the NBT fee
    AddFeeVoteBlocks( 500, 2*COIN, -1);
    AddFeeVoteBlocks(1000, 3*COIN, -1);
    AddFeeVoteBlocks(2000, 1*CENT, -1);

    // The first blocks have the default fee
    for (int i = 0; i < 4000; i++)
    {
        CHECK_VOTED_MIN_FEE    (i, COIN, CENT);
        CHECK_EFFECTIVE_MIN_FEE(i, COIN, CENT);
        CHECK_SAFE_MIN_FEE     (i, COIN, CENT);
    }

    // The votes started at block 3000, so at block 4000 there are 1001 votes so the voted fee changes
    CHECK_VOTED_MIN_FEE    (4000, 2*COIN, CENT);
    // But not the other ones
    CHECK_EFFECTIVE_MIN_FEE(4000,   COIN, CENT);
    CHECK_SAFE_MIN_FEE     (4000,   COIN, CENT);

    // The effective fee doesn't change until 60 blocks have passed
    for (int i = 4000; i < 4060; i++)
        CHECK_EFFECTIVE_MIN_FEE(i, COIN, CENT);
    CHECK_EFFECTIVE_MIN_FEE(4060, 2*COIN, CENT);

    // The safe fee changes 10 blocks before the effective fee
    for (int i = 4000; i < 4050; i++)
        CHECK_SAFE_MIN_FEE(i, COIN, CENT);
    CHECK_SAFE_MIN_FEE(4050, 2*COIN, CENT);


    ResetFeeVoteBlocks();

    AddFeeVoteBlocks(1001, -1, 10*CENT);
    AddFeeVoteBlocks( 500, -1,  3*CENT);
    AddFeeVoteBlocks( 500, -1,  5*CENT);
    AddFeeVoteBlocks(1000, -1,  1*CENT);

    CHECK_VOTED_MIN_FEE(2000, 1*COIN, 5*CENT);
    CHECK_VOTED_MIN_FEE(2499, 1*COIN, 5*CENT);
    CHECK_VOTED_MIN_FEE(2500, 1*COIN, 3*CENT);

    CHECK_EFFECTIVE_MIN_FEE(2559, 1*COIN, 5*CENT);
    CHECK_EFFECTIVE_MIN_FEE(2560, 1*COIN, 3*CENT);

    CHECK_SAFE_MIN_FEE(2549, 1*COIN, 5*CENT);
    CHECK_SAFE_MIN_FEE(2550, 1*COIN, 5*CENT);
    CHECK_SAFE_MIN_FEE(2558, 1*COIN, 5*CENT); // the next block still has a 5 cents fee
    CHECK_SAFE_MIN_FEE(2559, 1*COIN, 3*CENT); // there's no more 5 cents blocks

    ResetFeeVoteBlocks();
}

BOOST_AUTO_TEST_SUITE_END()
