#include <boost/test/unit_test.hpp>

#include "main.h"
#include "wallet.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(unit_tests)

//
// Helper: create two dummy transactions, each with
// two outputs.  The first has 11 and 50 CENT outputs
// paid to a TX_PUBKEY, the second 21 and 22 CENT outputs
// paid to a TX_PUBKEYHASH.
//
static std::vector<CTransaction>
SetupDummyInputs(CBasicKeyStore& keystoreRet, CCoinsView & coinsRet)
{
    std::vector<CTransaction> dummyTransactions;
    dummyTransactions.resize(2);

    // Add some keys to the keystore:
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    // Create some dummy input transactions
    dummyTransactions[0].cUnit = 'S';
    dummyTransactions[0].vout.resize(2);
    dummyTransactions[0].vout[0].nValue = 11*CENT;
    dummyTransactions[0].vout[0].scriptPubKey << key[0].GetPubKey() << OP_CHECKSIG;
    dummyTransactions[0].vout[1].nValue = 50*CENT;
    dummyTransactions[0].vout[1].scriptPubKey << key[1].GetPubKey() << OP_CHECKSIG;
    coinsRet.SetCoins(dummyTransactions[0].GetHash(), CCoins(dummyTransactions[0], 0));

    dummyTransactions[1].cUnit = 'S';
    dummyTransactions[1].vout.resize(2);
    dummyTransactions[1].vout[0].nValue = 21*CENT;
    dummyTransactions[1].vout[0].scriptPubKey.SetDestination(key[2].GetPubKey().GetID());
    dummyTransactions[1].vout[1].nValue = 22*CENT;
    dummyTransactions[1].vout[1].scriptPubKey.SetDestination(key[3].GetPubKey().GetID());
    coinsRet.SetCoins(dummyTransactions[1].GetHash(), CCoins(dummyTransactions[1], 0));

    return dummyTransactions;
}

BOOST_AUTO_TEST_CASE(test_same_unit_in_transaction)
{
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(coinsDummy);
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);
    CValidationState state;

    CTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[1].prevout.n = 0;
    t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[2].prevout.n = 1;
    t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vout.resize(1);
    t1.vout[0].nValue = 10*COIN;
    t1.vout[0].scriptPubKey << OP_1;

    // No unit defined
    BOOST_CHECK(!t1.CheckTransaction(state));

    // Same unit as input
    t1.cUnit = 'S';
    BOOST_CHECK(t1.CheckTransaction(state));
    BOOST_CHECK(t1.AreInputsSameUnit(coins));

    // Different unit
    t1.cUnit = 'U';
    BOOST_CHECK(!t1.AreInputsSameUnit(coins));
}

BOOST_AUTO_TEST_CASE(test_changing_unit_invalidates_signature)
{
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(coinsDummy);
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);
    CValidationState state;

    CTransaction t1;
    t1.vin.resize(1);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vout.resize(1);
    t1.vout[0].nValue = 11*COIN;
    t1.vout[0].scriptPubKey << OP_1;
    t1.cUnit = 'S';

    BOOST_CHECK(SignSignature(keystore, dummyTransactions[0], t1, 0));

    BOOST_CHECK(t1.CheckTransaction(state));
    BOOST_CHECK(VerifySignature(CCoins(dummyTransactions[0], 0), t1, 0, true, 0));

    t1.cUnit = 'U';
    BOOST_CHECK(!VerifySignature(CCoins(dummyTransactions[0], 0), t1, 0, true, 0));
}

BOOST_AUTO_TEST_CASE(address_use_unit)
{
    // Invalid unit
    BOOST_CHECK_THROW(CBitcoinAddress(CKeyID(1), '?'), runtime_error);

    BOOST_CHECK_EQUAL(CBitcoinAddress(CKeyID(0), 'B').ToString(), "B4T5ciTCkWauSqVAcVKy88ofjcSasUkSYU");
    BOOST_CHECK_EQUAL(CBitcoinAddress(CKeyID(0), 'S').ToString(), "SMJ12qn9jNCCXJnTYRz5Yu9ZenERqvYwfg");
}

BOOST_AUTO_TEST_CASE(serialize_keeps_unit)
{
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(coinsDummy);
    std::vector<CTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);
    CValidationState state;

    CTransaction t1;
    t1.vin.resize(1);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vout.resize(1);
    t1.vout[0].nValue = 11*COIN;
    t1.vout[0].scriptPubKey << OP_1;
    t1.cUnit = 'S';
    BOOST_CHECK(SignSignature(keystore, dummyTransactions[0], t1, 0));
    BOOST_CHECK(t1.CheckTransaction(state));

    CDataStream stream(SER_DISK, CLIENT_VERSION);
    stream << t1;

    vector<unsigned char> vch(stream.begin(), stream.end());
    CDataStream stream2(vch, SER_DISK, CLIENT_VERSION);
    CTransaction t2;
    stream2 >> t2;
    BOOST_CHECK_EQUAL(11*COIN, t2.vout[0].nValue);
    BOOST_CHECK_EQUAL('S', t2.cUnit);
    BOOST_CHECK(t2.CheckTransaction(state));
}

BOOST_AUTO_TEST_SUITE_END()

