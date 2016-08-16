// Copyright (c) 2014-2015 The Nu developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "db.h"
#include "walletdb.h"
#include "net.h"
#include "init.h"
#include "util.h"
#include "scanbalance.h"
#include "txdb.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

#ifndef WIN32
#include <signal.h>
#endif

using namespace std;
using namespace boost;

static void ScanTransactionInputs(CCoinsViewCache& view, const CTransaction& tx, BalanceMap& mapBalance)
{
    if (tx.IsCoinBase() || tx.IsCustodianGrant()) return;

    BOOST_FOREACH(const CTxIn& txi, tx.vin)
    {
        CDiskTxPos postx;
        CTransaction txPrev;
        if (pblocktree->ReadTxIndex(txi.prevout.hash, postx))
        {
            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            CBlockHeader header;
            try {
                file >> header;
                fseek(file, postx.nTxOffset, SEEK_CUR);
                file >> txPrev;
            } catch (std::exception &e) {
                throw runtime_error("deserialize or I/O error in ScanTransactionInputs");
            }
            if (txPrev.GetHash() != txi.prevout.hash)
                throw runtime_error("txid mismatch in ScanTransactionInputs");
        }
        else
            throw runtime_error("tx missing in tx index in ScanTransactionInputs");

        const CTransaction& ti = txPrev;
        const CTxOut& prevOut = ti.vout[txi.prevout.n];
        CTxDestination dest;
        ExtractDestination(prevOut.scriptPubKey, dest);
        CBitcoinAddress addr(dest, 'S');
        if (prevOut.nValue > 0)
        {
            if (mapBalance.count(addr) == 0) {
                string s = strprintf("Missing input address: %s", txi.ToStringShort().c_str());
                throw runtime_error(s);
            }
            if (prevOut.nValue > mapBalance[addr]) {
                string s = strprintf("Input address would have negative balance: %s", txi.ToStringShort().c_str());
                throw runtime_error(s);
            }
            mapBalance[addr] -= prevOut.nValue;
            if (mapBalance[addr] == 0) mapBalance.erase(addr);
        }
    }
}

static void ScanTransactionOutputs(const CTransaction& tx, BalanceMap& mapBalance)
{
    BOOST_FOREACH(const CTxOut& txo, tx.vout)
    {
        if (txo.nValue > 0 && !txo.scriptPubKey.empty())
        {
            CTxDestination dest;
            ExtractDestination(txo.scriptPubKey, dest);
            CBitcoinAddress addr(dest, 'S');
            mapBalance[addr] += txo.nValue;
        }
    }
}

void GetAddressBalances(unsigned int cutoffTime, BalanceMap& mapBalance)
{
    if (!fTxIndex)
        throw runtime_error("Transaction index is required to get balance at a particular time");

    CBlockIndex* pblk0 = pindexGenesisBlock, *pblk1 = pindexBest;
    if (!pblk0) throw runtime_error("No genesis block.");
    if (!pblk1) throw runtime_error("No best block chain.");

    if (cutoffTime>pblk1->nTime)
        throw runtime_error("Cutoff date later than most recent block.");

    LOCK(cs_main);
    CCoinsViewCache view(*pcoinsTip, true);
    int nBlks = 0;
    while (pblk0 != pblk1)
    {
        if (pblk0->nTime >= cutoffTime) break;

        CBlock block;
        block.ReadFromDisk(pblk0);

        BOOST_FOREACH(const CTransaction& tx, block.vtx)
        {
            ScanTransactionInputs(view, tx, mapBalance);
            ScanTransactionOutputs(tx, mapBalance);
        }

        pblk0 = pblk0->pnext;
        nBlks++;
    }
}
