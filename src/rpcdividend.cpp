// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "scanbalance.h"
#include "distribution.h"

using namespace json_spirit;
using namespace std;

Value distribute(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "distribute <cutoff timestamp> <amount> [<proceed>]\n"
            "cutoff is date and time at which the share balances should be considered. Format is unix time.\n"
            "amount is the the number of peercoins to distribute, in double-precision floating point number.\n"
            "If proceed is not true the peercoins are not sent and the details of the distribution are returned.");

    unsigned int cutoffTime = params[0].get_int();
    bool fProceed = false;
    if (params.size() > 2)
        fProceed = params[2].get_bool();

    BalanceMap mapBalance;
    GetAddressBalances(cutoffTime, mapBalance);

    double dAmount = params[1].get_real();
    DividendDistributor distributor = GenerateDistribution(mapBalance, dAmount);

    Array results;
    if (fProceed)
        return SendDistribution(distributor);
    else
    {
        Object result;
        result.push_back(Pair("address_count", (int)mapBalance.size()));
        result.push_back(Pair("minimum_payout", GetMinimumDividendPayout()));
        result.push_back(Pair("distribution_address_count", (int)distributor.GetDistributions().size()));
        result.push_back(Pair("total_distributed", distributor.TotalDistributed()));
        Array distributions;
        BOOST_FOREACH(const Distribution &distribution, distributor.GetDistributions())
        {
            Object obj;
            obj.push_back(Pair("nu_address", distribution.GetPeershareAddress().ToString()));
            obj.push_back(Pair("balance", (double)distribution.GetBalance() / COIN));
            obj.push_back(Pair("peercoin_address", distribution.GetPeercoinAddress().ToString()));
            obj.push_back(Pair("dividends", distribution.GetDividendAmount()));
            distributions.push_back(obj);
        }
        result.push_back(Pair("distributions", distributions));
        return result;
    }
}

