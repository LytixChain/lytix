// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"

#include "addrman.h"
#include "maxnode-budget.h"
#include "maxnode-sync.h"
#include "maxnode.h"
#include "maxnodeman.h"
#include "obfuscation.h"
#include "util.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

CMAXBudgetManager maxbudget;
CCriticalSection cs_maxbudget;

std::map<uint256, int64_t> askedForMaxSourceProposalOrBudget;
std::vector<CMAXBudgetProposalBroadcast> vecMAXImmatureBudgetProposals;
std::vector<CMAXFinalizedBudgetBroadcast> vecMAXImmatureFinalizedBudgets;

int nMaxSubmittedFinalBudget;

int GetMaxBudgetPaymentCycleBlocks()
{
    // Amount of blocks in a months period of time (using 1 minutes per) = (60*24*30)
    if (Params().NetworkID() == CBaseChainParams::MAIN) return 43200;
    //for testing purposes

    return 144; //ten times per day
}

bool IsMaxBudgetCollateralValid(uint256 nTxCollateralHash, uint256 nExpectedHash, std::string& strError, int64_t& nTime, int& nConf, bool fBudgetFinalization)
{
    CTransaction txCollateral;
    uint256 nBlockHash;
    if (!GetTransaction(nTxCollateralHash, txCollateral, nBlockHash, true)) {
        strError = strprintf("Can't find collateral tx %s", txCollateral.ToString());
        LogPrint("maxbudget","CMAXBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    if (txCollateral.vout.size() < 1) return false;
    if (txCollateral.nLockTime != 0) return false;

    CScript findScript;
    findScript << OP_RETURN << ToByteVector(nExpectedHash);

    bool foundOpReturn = false;
    BOOST_FOREACH (const CTxOut o, txCollateral.vout) {
        if (!o.scriptPubKey.IsNormalPaymentScript() && !o.scriptPubKey.IsUnspendable()) {
            strError = strprintf("Invalid Script %s", txCollateral.ToString());
            LogPrint("maxbudget","CMAXBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
            return false;
        }
        if (fBudgetFinalization) {
            // Collateral for maxbudget finalization
            // Note: there are still old valid maxbudgets out there, but the check for the new 5 PIV finalization collateral
            //       will also cover the old 50 PIV finalization collateral.
            LogPrint("maxbudget", "Final Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", o.scriptPubKey.ToString(), findScript.ToString());
            if (o.scriptPubKey == findScript) {
                LogPrint("maxbudget", "Final Budget: o.nValue(%ld) >= MAX_BUDGET_FEE_TX(%ld) ?\n", o.nValue, MAX_BUDGET_FEE_TX);
                if(o.nValue >= MAX_BUDGET_FEE_TX) {
                    foundOpReturn = true;
                }
            }
        }
        else {
            // Collateral for normal maxbudget proposal
            LogPrint("maxbudget", "Normal Budget: o.scriptPubKey(%s) == findScript(%s) ?\n", o.scriptPubKey.ToString(), findScript.ToString());
            if (o.scriptPubKey == findScript) {
                LogPrint("maxbudget", "Normal Budget: o.nValue(%ld) >= MAX_PROPOSAL_FEE_TX(%ld) ?\n", o.nValue, MAX_PROPOSAL_FEE_TX);
                if(o.nValue >= MAX_PROPOSAL_FEE_TX) {
                    foundOpReturn = true;
                }
            }
        }
    }
    if (!foundOpReturn) {
        strError = strprintf("Couldn't find opReturn %s in %s", nExpectedHash.ToString(), txCollateral.ToString());
        LogPrint("maxbudget","CMAXBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    // RETRIEVE CONFIRMATIONS AND NTIME
    /*
        - nTime starts as zero and is passed-by-reference out of this function and stored in the external proposal
        - nTime is never validated via the hashing mechanism and comes from a full-validated source (the blockchain)
    */

    int conf = GetIXConfirmations(nTxCollateralHash);
    if (nBlockHash != uint256(0)) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
                nTime = pindex->nTime;
            }
        }
    }

    nConf = conf;

    //if we're syncing we won't have swiftTX information, so accept 1 confirmation
    if (conf >= Params().Budget_Fee_Confirmations()) {
        return true;
    } else {
        strError = strprintf("Collateral requires at least %d confirmations - %d confirmations", Params().Budget_Fee_Confirmations(), conf);
        LogPrint("maxbudget","CMAXBudgetProposalBroadcast::IsBudgetCollateralValid - %s - %d confirmations\n", strError, conf);
        return false;
    }
}

void CMAXBudgetManager::CheckOrphanVotes()
{
    LOCK(cs);


    std::string strError = "";
    std::map<uint256, CMAXBudgetVote>::iterator it1 = mapOrphanMaxnodeBudgetVotes.begin();
    while (it1 != mapOrphanMaxnodeBudgetVotes.end()) {
        if (maxbudget.UpdateProposal(((*it1).second), NULL, strError)) {
            LogPrint("maxbudget","CMAXBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanMaxnodeBudgetVotes.erase(it1++);
        } else {
            ++it1;
        }
    }
    std::map<uint256, CMAXFinalizedBudgetVote>::iterator it2 = mapOrphanFinalizedBudgetVotes.begin();
    while (it2 != mapOrphanFinalizedBudgetVotes.end()) {
        if (maxbudget.UpdateFinalizedBudget(((*it2).second), NULL, strError)) {
            LogPrint("maxbudget","CMAXBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanFinalizedBudgetVotes.erase(it2++);
        } else {
            ++it2;
        }
    }
    LogPrint("maxbudget","CMAXBudgetManager::CheckOrphanVotes - Done\n");
}

void CMAXBudgetManager::SubmitFinalBudget()
{
    static int nSubmittedHeight = 0; // height at which final maxbudget was submitted last time
    int nCurrentHeight;

    {
        TRY_LOCK(cs_main, locked);
        if (!locked) return;
        if (!chainActive.Tip()) return;
        nCurrentHeight = chainActive.Height();
    }

    int nBlockStart = nCurrentHeight - nCurrentHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();
    if (nSubmittedHeight >= nBlockStart){
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - nSubmittedHeight(=%ld) < nBlockStart(=%ld) condition not fulfilled.\n", nSubmittedHeight, nBlockStart);
        return;
    }

     // Submit final maxbudget during the last 2 days (2880 blocks) before payment for Mainnet, about 9 minutes (9 blocks) for Testnet
    int finalizationWindow = ((GetMaxBudgetPaymentCycleBlocks() / 30) * 2);

    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        // NOTE: 9 blocks for testnet is way to short to have any maxnode submit an automatic vote on the finalized(!) maxbudget,
        //       because those votes are only submitted/relayed once every 56 blocks in CMAXFinalizedBudget::AutoCheck()

        finalizationWindow = 64; // 56 + 4 finalization confirmations + 4 minutes buffer for propagation
    }

    int nFinalizationStart = nBlockStart - finalizationWindow;
 
    int nOffsetToStart = nFinalizationStart - nCurrentHeight;

    if (nBlockStart - nCurrentHeight > finalizationWindow) {
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Too early for finalization. Current block is %ld, next Superblock is %ld.\n", nCurrentHeight, nBlockStart);
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - First possible block for finalization: %ld. Last possible block for finalization: %ld. You have to wait for %ld block(s) until Budget finalization will be possible\n", nFinalizationStart, nBlockStart, nOffsetToStart);

        return;
    }

    std::vector<CMAXBudgetProposal*> vBudgetProposals = maxbudget.GetBudget();
    std::string strBudgetName = "main";
    std::vector<CMAXTxBudgetPayment> vecTxBudgetPayments;

    for (unsigned int i = 0; i < vBudgetProposals.size(); i++) {
        CMAXTxBudgetPayment txBudgetPayment;
        txBudgetPayment.nProposalHash = vBudgetProposals[i]->GetHash();
        txBudgetPayment.payee = vBudgetProposals[i]->GetPayee();
        txBudgetPayment.nAmount = vBudgetProposals[i]->GetAllotted();
        vecTxBudgetPayments.push_back(txBudgetPayment);
    }

    if (vecTxBudgetPayments.size() < 1) {
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Found No Proposals For Period\n");
        return;
    }

    CMAXFinalizedBudgetBroadcast tempBudget(strBudgetName, nBlockStart, vecTxBudgetPayments, 0);
    if (mapSeenFinalizedBudgets.count(tempBudget.GetHash())) {
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Budget already exists - %s\n", tempBudget.GetHash().ToString());
        nSubmittedHeight = nCurrentHeight;
        return; //already exists
    }

    //create fee tx
    CTransaction tx;
    uint256 txidCollateral;

    if (!mapCollateralTxids.count(tempBudget.GetHash())) {
        CWalletTx wtx;
        if (!pwalletMain->GetBudgetFinalizationCollateralTX(wtx, tempBudget.GetHash(), false)) {
            LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Can't make collateral transaction\n");
            return;
        }

        // Get our change address
        CReserveKey reservekey(pwalletMain);
        // Send the tx to the network. Do NOT use SwiftTx, locking might need too much time to propagate, especially for testnet
        pwalletMain->CommitTransaction(wtx, reservekey, "NO-ix");
        tx = (CTransaction)wtx;
        txidCollateral = tx.GetHash();
        mapCollateralTxids.insert(make_pair(tempBudget.GetHash(), txidCollateral));
    } else {
        txidCollateral = mapCollateralTxids[tempBudget.GetHash()];
    }

    int conf = GetIXConfirmations(txidCollateral);
    CTransaction txCollateral;
    uint256 nBlockHash;

    if (!GetTransaction(txidCollateral, txCollateral, nBlockHash, true)) {
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Can't find collateral tx %s", txidCollateral.ToString());
        return;
    }

    if (nBlockHash != uint256(0)) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
            }
        }
    }

    /*
        Wait will we have 1 extra confirmation, otherwise some clients might reject this feeTX
        -- This function is tied to NewBlock, so we will propagate this maxbudget while the block is also propagating
    */
    if (conf < Params().Budget_Fee_Confirmations() + 1) {
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Collateral requires at least %d confirmations - %s - %d confirmations\n", Params().Budget_Fee_Confirmations() + 1, txidCollateral.ToString(), conf);
        return;
    }

    //create the proposal incase we're the first to make it
    CMAXFinalizedBudgetBroadcast finalizedBudgetBroadcast(strBudgetName, nBlockStart, vecTxBudgetPayments, txidCollateral);

    std::string strError = "";
    if (!finalizedBudgetBroadcast.IsValid(strError)) {
        LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Invalid finalized maxbudget - %s \n", strError);
        return;
    }

    LOCK(cs);
    mapSeenFinalizedBudgets.insert(make_pair(finalizedBudgetBroadcast.GetHash(), finalizedBudgetBroadcast));
    finalizedBudgetBroadcast.Relay();
    maxbudget.AddFinalizedBudget(finalizedBudgetBroadcast);
    nSubmittedHeight = nCurrentHeight;
    LogPrint("maxbudget","CMAXBudgetManager::SubmitFinalBudget - Done! %s\n", finalizedBudgetBroadcast.GetHash().ToString());
}

//
// CMAXBudgetDB
//

CMAXBudgetDB::CMAXBudgetDB()
{
    pathDB = GetDataDir() / "maxbudget.dat";
    strMagicMessage = "MaxnodeBudget";
}

bool CMAXBudgetDB::Write(const CMAXBudgetManager& objToSave)
{
    LOCK(objToSave.cs);

    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << strMagicMessage;                   // maxnode cache file specific magic message
    ssObj << FLATDATA(Params().MessageStart()); // network specific magic number
    ssObj << objToSave;
    uint256 hash = Hash(ssObj.begin(), ssObj.end());
    ssObj << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fopen(pathDB.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathDB.string());

    // Write and commit header, data
    try {
        fileout << ssObj;
    } catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    fileout.fclose();

    LogPrint("maxbudget","Written info to maxbudget.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CMAXBudgetDB::ReadResult CMAXBudgetDB::Read(CMAXBudgetManager& objToLoad, bool fDryRun)
{
    LOCK(objToLoad.cs);

    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fopen(pathDB.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathDB.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathDB);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    } catch (std::exception& e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return HashReadError;
    }
    filein.fclose();

    CDataStream ssObj(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssObj.begin(), ssObj.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }


    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (maxnode cache file specific magic message) and ..
        ssObj >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid maxnode cache magic message", __func__);
            return IncorrectMagicMessage;
        }


        // de-serialize file header (network specific magic number) and ..
        ssObj >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CMAXBudgetManager object
        ssObj >> objToLoad;
    } catch (std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint("maxbudget","Loaded info from maxbudget.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint("maxbudget","  %s\n", objToLoad.ToString());
    if (!fDryRun) {
        LogPrint("maxbudget","Budget manager - cleaning....\n");
        objToLoad.CheckAndRemove();
        LogPrint("maxbudget","Budget manager - result:\n");
        LogPrint("maxbudget","  %s\n", objToLoad.ToString());
    }

    return Ok;
}

void DumpMaxBudgets()
{
    int64_t nStart = GetTimeMillis();

    CMAXBudgetDB maxbudgetdb;
    CMAXBudgetManager tempBudget;

    LogPrint("maxbudget","Verifying maxbudget.dat format...\n");
    CMAXBudgetDB::ReadResult readResult = maxbudgetdb.Read(tempBudget, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CMAXBudgetDB::FileError)
        LogPrint("maxbudget","Missing maxbudgets file - maxbudget.dat, will try to recreate\n");
    else if (readResult != CMAXBudgetDB::Ok) {
        LogPrint("maxbudget","Error reading maxbudget.dat: ");
        if (readResult == CMAXBudgetDB::IncorrectFormat)
            LogPrint("maxbudget","magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrint("maxbudget","file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrint("maxbudget","Writting info to maxbudget.dat...\n");
    maxbudgetdb.Write(maxbudget);

    LogPrint("maxbudget","Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool CMAXBudgetManager::AddFinalizedBudget(CMAXFinalizedBudget& finalizedBudget)
{
    std::string strError = "";
    if (!finalizedBudget.IsValid(strError)) return false;

    if (mapFinalizedBudgets.count(finalizedBudget.GetHash())) {
        return false;
    }

    mapFinalizedBudgets.insert(make_pair(finalizedBudget.GetHash(), finalizedBudget));
    return true;
}

bool CMAXBudgetManager::AddProposal(CMAXBudgetProposal& maxbudgetProposal)
{
    LOCK(cs);
    std::string strError = "";
    if (!maxbudgetProposal.IsValid(strError)) {
        LogPrint("maxbudget","CMAXBudgetManager::AddProposal - invalid maxbudget proposal - %s\n", strError);
        return false;
    }

    if (mapProposals.count(maxbudgetProposal.GetHash())) {
        return false;
    }

    mapProposals.insert(make_pair(maxbudgetProposal.GetHash(), maxbudgetProposal));
    LogPrint("maxbudget","CMAXBudgetManager::AddProposal - proposal %s added\n", maxbudgetProposal.GetName ().c_str ());
    return true;
}

void CMAXBudgetManager::CheckAndRemove()
{
    int nHeight = 0;

    // Add some verbosity once loading blocks from files has finished
    {
        TRY_LOCK(cs_main, locked);
        if ((locked) && (chainActive.Tip() != NULL)) {
            CBlockIndex* pindexPrev = chainActive.Tip();
            if (pindexPrev) {
                nHeight = pindexPrev->nHeight;
            }
        }
    }

    LogPrint("maxbudget", "CMAXBudgetManager::CheckAndRemove at Height=%d\n", nHeight);

    map<uint256, CMAXFinalizedBudget> tmpMapFinalizedBudgets;
    map<uint256, CMAXBudgetProposal> tmpMapProposals;

    std::string strError = "";

    LogPrint("maxbudget", "CMAXBudgetManager::CheckAndRemove - mapFinalizedBudgets cleanup - size before: %d\n", mapFinalizedBudgets.size());
    std::map<uint256, CMAXFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = &((*it).second);

        pfinalizedBudget->fValid = pfinalizedBudget->IsValid(strError);
        if (!strError.empty ()) {
            LogPrint("maxbudget","CMAXBudgetManager::CheckAndRemove - Invalid finalized maxbudget: %s\n", strError);
        }
        else {
            LogPrint("maxbudget","CMAXBudgetManager::CheckAndRemove - Found valid finalized maxbudget: %s %s\n",
                      pfinalizedBudget->strBudgetName.c_str(), pfinalizedBudget->nFeeTXHash.ToString().c_str());
        }

        if (pfinalizedBudget->fValid) {
            pfinalizedBudget->AutoCheck();
            tmpMapFinalizedBudgets.insert(make_pair(pfinalizedBudget->GetHash(), *pfinalizedBudget));
        }

        ++it;
    }

    LogPrint("maxbudget", "CMAXBudgetManager::CheckAndRemove - mapProposals cleanup - size before: %d\n", mapProposals.size());
    std::map<uint256, CMAXBudgetProposal>::iterator it2 = mapProposals.begin();
    while (it2 != mapProposals.end()) {
        CMAXBudgetProposal* pmaxbudgetProposal = &((*it2).second);
        pmaxbudgetProposal->fValid = pmaxbudgetProposal->IsValid(strError);
        if (!strError.empty ()) {
            LogPrint("maxbudget","CMAXBudgetManager::CheckAndRemove - Invalid maxbudget proposal - %s\n", strError);
            strError = "";
        }
        else {
             LogPrint("maxbudget","CMAXBudgetManager::CheckAndRemove - Found valid maxbudget proposal: %s %s\n",
                      pmaxbudgetProposal->strProposalName.c_str(), pmaxbudgetProposal->nFeeTXHash.ToString().c_str());
        }
        if (pmaxbudgetProposal->fValid) {
            tmpMapProposals.insert(make_pair(pmaxbudgetProposal->GetHash(), *pmaxbudgetProposal));
        }

        ++it2;
    }
    // Remove invalid entries by overwriting complete map
    mapFinalizedBudgets.swap(tmpMapFinalizedBudgets);
    mapProposals.swap(tmpMapProposals);

    // clang doesn't accept copy assignemnts :-/
    // mapFinalizedBudgets = tmpMapFinalizedBudgets;
    // mapProposals = tmpMapProposals;

    LogPrint("maxbudget", "CMAXBudgetManager::CheckAndRemove - mapFinalizedBudgets cleanup - size after: %d\n", mapFinalizedBudgets.size());
    LogPrint("maxbudget", "CMAXBudgetManager::CheckAndRemove - mapProposals cleanup - size after: %d\n", mapProposals.size());
    LogPrint("maxbudget","CMAXBudgetManager::CheckAndRemove - PASSED\n");

}

void CMAXBudgetManager::FillMaxBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake)
{
    LOCK(cs);

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    int nHighestCount = 0;
    CScript payee;
    CAmount nAmount = 0;

    // ------- Grab The Highest Count

    std::map<uint256, CMAXFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (pfinalizedBudget->GetMaxVoteCount() > nHighestCount &&
            pindexPrev->nHeight + 1 >= pfinalizedBudget->GetMaxBlockStart() &&
            pindexPrev->nHeight + 1 <= pfinalizedBudget->GetMaxBlockEnd() &&
            pfinalizedBudget->GetMaxPayeeAndAmount(pindexPrev->nHeight + 1, payee, nAmount)) {
            nHighestCount = pfinalizedBudget->GetMaxVoteCount();
        }

        ++it;
    }

    CAmount blockValue = GetBlockValue(pindexPrev->nHeight);

    if (fProofOfStake) {
        if (nHighestCount > 0) {
            unsigned int i = txNew.vout.size();
            txNew.vout.resize(i + 1);
            txNew.vout[i].scriptPubKey = payee;
            txNew.vout[i].nValue = nAmount;

            CTxDestination address1;
            ExtractDestination(payee, address1);
            CBitcoinAddress address2(address1);
            LogPrint("maxbudget","CMAXBudgetManager::FillMaxBlockPayee - Budget payment to %s for %lld, nHighestCount = %d\n", address2.ToString(), nAmount, nHighestCount);
        }
        else {
            LogPrint("maxbudget","CMAXBudgetManager::FillMaxBlockPayee - No Budget payment, nHighestCount = %d\n", nHighestCount);
        }
    } else {
        //miners get the full amount on these blocks
        txNew.vout[0].nValue = blockValue;

        if (nHighestCount > 0) {
            txNew.vout.resize(2);

            //these are super blocks, so their value can be much larger than normal
            txNew.vout[1].scriptPubKey = payee;
            txNew.vout[1].nValue = nAmount;

            CTxDestination address1;
            ExtractDestination(payee, address1);
            CBitcoinAddress address2(address1);

            LogPrint("maxbudget","CMAXBudgetManager::FillMaxBlockPayee - Budget payment to %s for %lld\n", address2.ToString(), nAmount);
        }
    }
}

CMAXFinalizedBudget* CMAXBudgetManager::FindFinalizedBudget(uint256 nHash)
{
    if (mapFinalizedBudgets.count(nHash))
        return &mapFinalizedBudgets[nHash];

    return NULL;
}

CMAXBudgetProposal* CMAXBudgetManager::FindMaxProposal(const std::string& strProposalName)
{
    //find the prop with the highest yes count

    int nYesCount = -99999;
    CMAXBudgetProposal* pmaxbudgetProposal = NULL;

    std::map<uint256, CMAXBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        if ((*it).second.strProposalName == strProposalName && (*it).second.GetMaxYeas() > nYesCount) {
            pmaxbudgetProposal = &((*it).second);
            nYesCount = pmaxbudgetProposal->GetMaxYeas();
        }
        ++it;
    }

    if (nYesCount == -99999) return NULL;

    return pmaxbudgetProposal;
}

CMAXBudgetProposal* CMAXBudgetManager::FindMaxProposal(uint256 nHash)
{
    LOCK(cs);

    if (mapProposals.count(nHash))
        return &mapProposals[nHash];

    return NULL;
}

bool CMAXBudgetManager::IsBudgetPaymentBlock(int nBlockHeight)
{
    int nHighestCount = -1;
    int nFivePercent = maxnodeman.CountEnabled(ActiveProtocol()) / 20;

    std::map<uint256, CMAXFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (pfinalizedBudget->GetMaxVoteCount() > nHighestCount &&
            nBlockHeight >= pfinalizedBudget->GetMaxBlockStart() &&
            nBlockHeight <= pfinalizedBudget->GetMaxBlockEnd()) {
            nHighestCount = pfinalizedBudget->GetMaxVoteCount();
        }

        ++it;
    }

    LogPrint("maxbudget","CMAXBudgetManager::IsBudgetPaymentBlock() - nHighestCount: %lli, 5%% of Maxnodes: %lli. Number of finalized maxbudgets: %lli\n", 
              nHighestCount, nFivePercent, mapFinalizedBudgets.size());

    // If maxbudget doesn't have 5% of the network votes, then we should pay a maxnode instead
    if (nHighestCount > nFivePercent) return true;

    return false;
}

MAXTrxValidationStatus CMAXBudgetManager::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs);

    MAXTrxValidationStatus transactionStatus = MAXTrxValidationStatus::InValid;
    int nHighestCount = 0;
    int nFivePercent = maxnodeman.CountEnabled(ActiveProtocol()) / 20;
    std::vector<CMAXFinalizedBudget*> ret;

    LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid - checking %lli finalized maxbudgets\n", mapFinalizedBudgets.size());

    // ------- Grab The Highest Count

    std::map<uint256, CMAXFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = &((*it).second);

        if (pfinalizedBudget->GetMaxVoteCount() > nHighestCount &&
            nBlockHeight >= pfinalizedBudget->GetMaxBlockStart() &&
            nBlockHeight <= pfinalizedBudget->GetMaxBlockEnd()) {
            nHighestCount = pfinalizedBudget->GetMaxVoteCount();
        }

        ++it;
    }

    LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid() - nHighestCount: %lli, 5%% of Maxnodes: %lli mapFinalizedBudgets.size(): %ld\n", 
              nHighestCount, nFivePercent, mapFinalizedBudgets.size());
    /*
        If maxbudget doesn't have 5% of the network votes, then we should pay a maxnode instead
    */
    if (nHighestCount < nFivePercent) return MAXTrxValidationStatus::InValid;

    // check the highest finalized maxbudgets (+/- 10% to assist in consensus)

    std::string strProposals = "";
    int nCountThreshold = nHighestCount - maxnodeman.CountEnabled(ActiveProtocol()) / 10;
    bool fThreshold = false;
    it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = &((*it).second);
        strProposals = pfinalizedBudget->GetMaxProposals();

        LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid - checking maxbudget (%s) with blockstart %lli, blockend %lli, nBlockHeight %lli, votes %lli, nCountThreshold %lli\n",
                 strProposals.c_str(), pfinalizedBudget->GetMaxBlockStart(), pfinalizedBudget->GetMaxBlockEnd(), 
                 nBlockHeight, pfinalizedBudget->GetMaxVoteCount(), nCountThreshold);

        if (pfinalizedBudget->GetMaxVoteCount() > nCountThreshold) {
            fThreshold = true;
            LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid - GetMaxVoteCount() > nCountThreshold passed\n");
            if (nBlockHeight >= pfinalizedBudget->GetMaxBlockStart() && nBlockHeight <= pfinalizedBudget->GetMaxBlockEnd()) {
                LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid - GetMaxBlockStart() passed\n");
                transactionStatus = pfinalizedBudget->IsTransactionValid(txNew, nBlockHeight);
                if (transactionStatus == MAXTrxValidationStatus::Valid) {
                    LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid - pfinalizedBudget->IsTransactionValid() passed\n");
                    return MAXTrxValidationStatus::Valid;
                }
                else {
                    LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid - pfinalizedBudget->IsTransactionValid() error\n");
                }
            }
            else {
                LogPrint("maxbudget","CMAXBudgetManager::IsTransactionValid - GetMaxBlockStart() failed, maxbudget is outside current payment cycle and will be ignored.\n");
            }
               
        }

        ++it;
    }

    // If not enough maxnodes autovoted for any of the finalized maxbudgets pay a maxnode instead
    if(!fThreshold) {
        transactionStatus = MAXTrxValidationStatus::VoteThreshold;
    }
    
    // We looked through all of the known maxbudgets
    return transactionStatus;
}

std::vector<CMAXBudgetProposal*> CMAXBudgetManager::GetAllProposals()
{
    LOCK(cs);

    std::vector<CMAXBudgetProposal*> vBudgetProposalRet;

    std::map<uint256, CMAXBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        (*it).second.CleanAndRemove(false);

        CMAXBudgetProposal* pmaxbudgetProposal = &((*it).second);
        vBudgetProposalRet.push_back(pmaxbudgetProposal);

        ++it;
    }

    return vBudgetProposalRet;
}

//
// Sort by votes, if there's a tie sort by their feeHash TX
//
struct sortProposalsByVotes {
    bool operator()(const std::pair<CMAXBudgetProposal*, int>& left, const std::pair<CMAXBudgetProposal*, int>& right)
    {
        if (left.second != right.second)
            return (left.second > right.second);
        return (left.first->nFeeTXHash > right.first->nFeeTXHash);
    }
};

//Need to review this function
std::vector<CMAXBudgetProposal*> CMAXBudgetManager::GetBudget()
{
    LOCK(cs);

    // ------- Sort maxbudgets by Yes Count

    std::vector<std::pair<CMAXBudgetProposal*, int> > vBudgetPorposalsSort;

    std::map<uint256, CMAXBudgetProposal>::iterator it = mapProposals.begin();
    while (it != mapProposals.end()) {
        (*it).second.CleanAndRemove(false);
        vBudgetPorposalsSort.push_back(make_pair(&((*it).second), (*it).second.GetMaxYeas() - (*it).second.GetMaxNays()));
        ++it;
    }

    std::sort(vBudgetPorposalsSort.begin(), vBudgetPorposalsSort.end(), sortProposalsByVotes());

    // ------- Grab The Budgets In Order

    std::vector<CMAXBudgetProposal*> vBudgetProposalsRet;

    CAmount nBudgetAllocated = 0;
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return vBudgetProposalsRet;

    int nBlockStart = pindexPrev->nHeight - pindexPrev->nHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();
    int nBlockEnd = nBlockStart + GetMaxBudgetPaymentCycleBlocks() - 1;
    CAmount nTotalBudget = GetMaxTotalBudget(nBlockStart);


    std::vector<std::pair<CMAXBudgetProposal*, int> >::iterator it2 = vBudgetPorposalsSort.begin();
    while (it2 != vBudgetPorposalsSort.end()) {
        CMAXBudgetProposal* pmaxbudgetProposal = (*it2).first;

        LogPrint("maxbudget","CMAXBudgetManager::GetBudget() - Processing Budget %s\n", pmaxbudgetProposal->strProposalName.c_str());
        //prop start/end should be inside this period
        if (pmaxbudgetProposal->fValid && pmaxbudgetProposal->nBlockStart <= nBlockStart &&
            pmaxbudgetProposal->nBlockEnd >= nBlockEnd &&
            pmaxbudgetProposal->GetMaxYeas() - pmaxbudgetProposal->GetMaxNays() > maxnodeman.CountEnabled(ActiveProtocol()) / 10 &&
            pmaxbudgetProposal->IsEstablished()) {

            LogPrint("maxbudget","CMAXBudgetManager::GetBudget() -   Check 1 passed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                      pmaxbudgetProposal->fValid, pmaxbudgetProposal->nBlockStart, nBlockStart, pmaxbudgetProposal->nBlockEnd,
                      nBlockEnd, pmaxbudgetProposal->GetMaxYeas(), pmaxbudgetProposal->GetMaxNays(), maxnodeman.CountEnabled(ActiveProtocol()) / 10,
                      pmaxbudgetProposal->IsEstablished());

            if (pmaxbudgetProposal->GetAmount() + nBudgetAllocated <= nTotalBudget) {
                pmaxbudgetProposal->SetAllotted(pmaxbudgetProposal->GetAmount());
                nBudgetAllocated += pmaxbudgetProposal->GetAmount();
                vBudgetProposalsRet.push_back(pmaxbudgetProposal);
                LogPrint("maxbudget","CMAXBudgetManager::GetBudget() -     Check 2 passed: Budget added\n");
            } else {
                pmaxbudgetProposal->SetAllotted(0);
                LogPrint("maxbudget","CMAXBudgetManager::GetBudget() -     Check 2 failed: no amount allotted\n");
            }
        }
        else {
            LogPrint("maxbudget","CMAXBudgetManager::GetBudget() -   Check 1 failed: valid=%d | %ld <= %ld | %ld >= %ld | Yeas=%d Nays=%d Count=%d | established=%d\n",
                      pmaxbudgetProposal->fValid, pmaxbudgetProposal->nBlockStart, nBlockStart, pmaxbudgetProposal->nBlockEnd,
                      nBlockEnd, pmaxbudgetProposal->GetMaxYeas(), pmaxbudgetProposal->GetMaxNays(), maxnodeman.CountEnabled(ActiveProtocol()) / 10,
                      pmaxbudgetProposal->IsEstablished());
        }

        ++it2;
    }

    return vBudgetProposalsRet;
}

// Sort by votes, if there's a tie sort by their feeHash TX
struct sortFinalizedBudgetsByVotes {
    bool operator()(const std::pair<CMAXFinalizedBudget*, int>& left, const std::pair<CMAXFinalizedBudget*, int>& right)
    {
        if (left.second != right.second)
            return left.second > right.second;
        return (left.first->nFeeTXHash > right.first->nFeeTXHash);
    }
};

std::vector<CMAXFinalizedBudget*> CMAXBudgetManager::GetFinalizedBudgets()
{
    LOCK(cs);

    std::vector<CMAXFinalizedBudget*> vFinalizedBudgetsRet;
    std::vector<std::pair<CMAXFinalizedBudget*, int> > vFinalizedBudgetsSort;

    // ------- Grab The Budgets In Order

    std::map<uint256, CMAXFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = &((*it).second);

        vFinalizedBudgetsSort.push_back(make_pair(pfinalizedBudget, pfinalizedBudget->GetMaxVoteCount()));
        ++it;
    }
    std::sort(vFinalizedBudgetsSort.begin(), vFinalizedBudgetsSort.end(), sortFinalizedBudgetsByVotes());

    std::vector<std::pair<CMAXFinalizedBudget*, int> >::iterator it2 = vFinalizedBudgetsSort.begin();
    while (it2 != vFinalizedBudgetsSort.end()) {
        vFinalizedBudgetsRet.push_back((*it2).first);
        ++it2;
    }

    return vFinalizedBudgetsRet;
}

std::string CMAXBudgetManager::GetMaxRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs);

    std::string ret = "unknown-maxbudget";

    std::map<uint256, CMAXFinalizedBudget>::iterator it = mapFinalizedBudgets.begin();
    while (it != mapFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = &((*it).second);
        if (nBlockHeight >= pfinalizedBudget->GetMaxBlockStart() && nBlockHeight <= pfinalizedBudget->GetMaxBlockEnd()) {
            CMAXTxBudgetPayment payment;
            if (pfinalizedBudget->GetMaxBudgetPaymentByBlock(nBlockHeight, payment)) {
                if (ret == "unknown-maxbudget") {
                    ret = payment.nProposalHash.ToString();
                } else {
                    ret += ",";
                    ret += payment.nProposalHash.ToString();
                }
            } else {
                LogPrint("maxbudget","CMAXBudgetManager::GetMaxRequiredPaymentsString - Couldn't find maxbudget payment for block %d\n", nBlockHeight);
            }
        }

        ++it;
    }

    return ret;
}

CAmount CMAXBudgetManager::GetMaxTotalBudget(int nHeight)
{
    if (chainActive.Tip() == NULL) return 0;

    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        CAmount nSubsidy = 250 * COIN;
        return ((nSubsidy / 100) * 10) * 146;
    }

    // Get block value and calculate from that
    CAmount nSubsidy = GetBlockValue(nHeight);

    // No maxbudget until after first two weeks of Proof of Stake
    if (nHeight <= 120160) {
        return 0 * COIN;
    } else {
       // Total maxbudget is based on 10% of block reward for a 30 day period (using 1 minutes per) = (60*24*30)
       // should be 1440 * 30 *0.3 (LYTX reward for a while) = 129,600 LYTX
       return ((nSubsidy / 100) * 7.2) * 60 * 24 * 30;
    }
}

void CMAXBudgetManager::NewBlock()
{
    TRY_LOCK(cs, fBudgetNewBlock);
    if (!fBudgetNewBlock) return;

    //if (maxnodeSync.RequestedMaxnodeAssets <= MAXNODE_SYNC_BUDGET) return;

    if (strBudgetMode == "suggest") { //suggest the maxbudget we see
        SubmitFinalBudget();
    }

    //this function should be called 1/14 blocks, allowing up to 100 votes per day on all proposals
    if (chainActive.Height() % 14 != 0) return;

    // incremental sync with our peers
    /**if (maxnodeSync.IsSynced()) {
        LogPrint("maxbudget","CMAXBudgetManager::NewBlock - incremental sync started\n");
        if (chainActive.Height() % 1440 == rand() % 1440) {
            ClearSeen();
            ResetSync();
        }

        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
            if (pnode->nVersion >= ActiveProtocol())
                Sync(pnode, 0, true);

        MarkSynced();
    }**/


    CheckAndRemove();

    //remove invalid votes once in a while (we have to check the signatures and validity of every vote, somewhat CPU intensive)

    LogPrint("maxbudget","CMAXBudgetManager::NewBlock - askedForMaxSourceProposalOrBudget cleanup - size: %d\n", askedForMaxSourceProposalOrBudget.size());
    std::map<uint256, int64_t>::iterator it = askedForMaxSourceProposalOrBudget.begin();
    while (it != askedForMaxSourceProposalOrBudget.end()) {
        if ((*it).second > GetTime() - (60 * 60 * 24)) {
            ++it;
        } else {
            askedForMaxSourceProposalOrBudget.erase(it++);
        }
    }

    LogPrint("maxbudget","CMAXBudgetManager::NewBlock - mapProposals cleanup - size: %d\n", mapProposals.size());
    std::map<uint256, CMAXBudgetProposal>::iterator it2 = mapProposals.begin();
    while (it2 != mapProposals.end()) {
        (*it2).second.CleanAndRemove(false);
        ++it2;
    }

    LogPrint("maxbudget","CMAXBudgetManager::NewBlock - mapFinalizedBudgets cleanup - size: %d\n", mapFinalizedBudgets.size());
    std::map<uint256, CMAXFinalizedBudget>::iterator it3 = mapFinalizedBudgets.begin();
    while (it3 != mapFinalizedBudgets.end()) {
        (*it3).second.CleanAndRemove(false);
        ++it3;
    }

    LogPrint("maxbudget","CMAXBudgetManager::NewBlock - vecMAXImmatureBudgetProposals cleanup - size: %d\n", vecMAXImmatureBudgetProposals.size());
    std::vector<CMAXBudgetProposalBroadcast>::iterator it4 = vecMAXImmatureBudgetProposals.begin();
    while (it4 != vecMAXImmatureBudgetProposals.end()) {
        std::string strError = "";
        int nConf = 0;
        if (!IsMaxBudgetCollateralValid((*it4).nFeeTXHash, (*it4).GetHash(), strError, (*it4).nTime, nConf)) {
            ++it4;
            continue;
        }

        if (!(*it4).IsValid(strError)) {
            LogPrint("maxbudget","mprop (immature) - invalid maxbudget proposal - %s\n", strError);
            it4 = vecMAXImmatureBudgetProposals.erase(it4);
            continue;
        }

        CMAXBudgetProposal maxbudgetProposal((*it4));
        if (AddProposal(maxbudgetProposal)) {
            (*it4).Relay();
        }

        LogPrint("maxbudget","mprop (immature) - new maxbudget - %s\n", (*it4).GetHash().ToString());
        it4 = vecMAXImmatureBudgetProposals.erase(it4);
    }

    LogPrint("maxbudget","CMAXBudgetManager::NewBlock - vecMAXImmatureFinalizedBudgets cleanup - size: %d\n", vecMAXImmatureFinalizedBudgets.size());
    std::vector<CMAXFinalizedBudgetBroadcast>::iterator it5 = vecMAXImmatureFinalizedBudgets.begin();
    while (it5 != vecMAXImmatureFinalizedBudgets.end()) {
        std::string strError = "";
        int nConf = 0;
        if (!IsMaxBudgetCollateralValid((*it5).nFeeTXHash, (*it5).GetHash(), strError, (*it5).nTime, nConf, true)) {
            ++it5;
            continue;
        }

        if (!(*it5).IsValid(strError)) {
            LogPrint("maxbudget","fbs (immature) - invalid finalized maxbudget - %s\n", strError);
            it5 = vecMAXImmatureFinalizedBudgets.erase(it5);
            continue;
        }

        LogPrint("maxbudget","fbs (immature) - new finalized maxbudget - %s\n", (*it5).GetHash().ToString());

        CMAXFinalizedBudget finalizedBudget((*it5));
        if (AddFinalizedBudget(finalizedBudget)) {
            (*it5).Relay();
        }

        it5 = vecMAXImmatureFinalizedBudgets.erase(it5);
    }
    LogPrint("maxbudget","CMAXBudgetManager::NewBlock - PASSED\n");
}

void CMAXBudgetManager::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    // lite mode is not supported
    if (fLiteMode) return;
    //if (!maxnodeSync.IsBlockchainSynced()) return;

    LOCK(cs_maxbudget);

    if (strCommand == "maxvs") { //Maxnode vote sync
        uint256 nProp;
        vRecv >> nProp;

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (nProp == 0) {
                if (pfrom->HasFulfilledRequest("maxvs")) {
                    LogPrint("maxbudget","maxvs - peer already asked me for the list\n");
                    Misbehaving(pfrom->GetId(), 20);
                    return;
                }
                pfrom->FulfilledRequest("maxvs");
            }
        }

        Sync(pfrom, nProp);
        LogPrint("maxbudget", "maxvs - Sent Maxnode votes to peer %i\n", pfrom->GetId());
    }

    if (strCommand == "mprop") { //Maxnode Proposal
        CMAXBudgetProposalBroadcast maxbudgetProposalBroadcast;
        vRecv >> maxbudgetProposalBroadcast;

        if (mapSeenMaxnodeBudgetProposals.count(maxbudgetProposalBroadcast.GetHash())) {
            //maxnodeSync.AddedBudgetItem(maxbudgetProposalBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        if (!IsMaxBudgetCollateralValid(maxbudgetProposalBroadcast.nFeeTXHash, maxbudgetProposalBroadcast.GetHash(), strError, maxbudgetProposalBroadcast.nTime, nConf)) {
            LogPrint("maxbudget","Proposal FeeTX is not valid - %s - %s\n", maxbudgetProposalBroadcast.nFeeTXHash.ToString(), strError);
            if (nConf >= 1) vecMAXImmatureBudgetProposals.push_back(maxbudgetProposalBroadcast);
            return;
        }

        mapSeenMaxnodeBudgetProposals.insert(make_pair(maxbudgetProposalBroadcast.GetHash(), maxbudgetProposalBroadcast));

        if (!maxbudgetProposalBroadcast.IsValid(strError)) {
            LogPrint("maxbudget","mprop - invalid maxbudget proposal - %s\n", strError);
            return;
        }

        CMAXBudgetProposal maxbudgetProposal(maxbudgetProposalBroadcast);
        if (AddProposal(maxbudgetProposal)) {
            maxbudgetProposalBroadcast.Relay();
        }
        //maxnodeSync.AddedBudgetItem(maxbudgetProposalBroadcast.GetHash());

        LogPrint("maxbudget","mprop - new maxbudget - %s\n", maxbudgetProposalBroadcast.GetHash().ToString());

        //We might have active votes for this proposal that are valid now
        CheckOrphanVotes();
    }

    if (strCommand == "mvote") { //Maxnode Vote
        CMAXBudgetVote vote;
        vRecv >> vote;
        vote.fValid = true;

        if (mapSeenMaxnodeBudgetVotes.count(vote.GetHash())) {
            //maxnodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        CMaxnode* pmax = maxnodeman.Find(vote.vin);
        if (pmax == NULL) {
            LogPrint("maxbudget","mvote - unknown maxnode - vin: %s\n", vote.vin.prevout.hash.ToString());
            maxnodeman.AskForMAX(pfrom, vote.vin);
            return;
        }


        /**mapSeenMaxnodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        if (!vote.SignatureValid(true)) {
            if (maxnodeSync.IsSynced()) {
                LogPrintf("CMAXBudgetManager::ProcessMessage() : mvote - signature invalid\n");
                Misbehaving(pfrom->GetId(), 20);
            }
            // it could just be a non-synced maxnode
            maxnodeman.AskForMAX(pfrom, vote.vin);
            return;
        }**/

        std::string strError = "";
        if (UpdateProposal(vote, pfrom, strError)) {
            vote.Relay();
            //maxnodeSync.AddedBudgetItem(vote.GetHash());
        }

        LogPrint("maxbudget","mvote - new maxbudget vote for maxbudget %s - %s\n", vote.nProposalHash.ToString(),  vote.GetHash().ToString());
    }

    if (strCommand == "fbs") { //Finalized Budget Suggestion
        CMAXFinalizedBudgetBroadcast finalizedBudgetBroadcast;
        vRecv >> finalizedBudgetBroadcast;

        if (mapSeenFinalizedBudgets.count(finalizedBudgetBroadcast.GetHash())) {
            //maxnodeSync.AddedBudgetItem(finalizedBudgetBroadcast.GetHash());
            return;
        }

        std::string strError = "";
        int nConf = 0;
        if (!IsMaxBudgetCollateralValid(finalizedBudgetBroadcast.nFeeTXHash, finalizedBudgetBroadcast.GetHash(), strError, finalizedBudgetBroadcast.nTime, nConf, true)) {
            LogPrint("maxbudget","fbs - Finalized Budget FeeTX is not valid - %s - %s\n", finalizedBudgetBroadcast.nFeeTXHash.ToString(), strError);

            if (nConf >= 1) vecMAXImmatureFinalizedBudgets.push_back(finalizedBudgetBroadcast);
            return;
        }

        mapSeenFinalizedBudgets.insert(make_pair(finalizedBudgetBroadcast.GetHash(), finalizedBudgetBroadcast));

        if (!finalizedBudgetBroadcast.IsValid(strError)) {
            LogPrint("maxbudget","fbs - invalid finalized maxbudget - %s\n", strError);
            return;
        }

        LogPrint("maxbudget","fbs - new finalized maxbudget - %s\n", finalizedBudgetBroadcast.GetHash().ToString());

        CMAXFinalizedBudget finalizedBudget(finalizedBudgetBroadcast);
        if (AddFinalizedBudget(finalizedBudget)) {
            finalizedBudgetBroadcast.Relay();
        }
        //maxnodeSync.AddedBudgetItem(finalizedBudgetBroadcast.GetHash());

        //we might have active votes for this maxbudget that are now valid
        CheckOrphanVotes();
    }

    if (strCommand == "fbvote") { //Finalized Budget Vote
        CMAXFinalizedBudgetVote vote;
        vRecv >> vote;
        vote.fValid = true;

        if (mapSeenFinalizedBudgetVotes.count(vote.GetHash())) {
            //maxnodeSync.AddedBudgetItem(vote.GetHash());
            return;
        }

        CMaxnode* pmax = maxnodeman.Find(vote.vin);
        if (pmax == NULL) {
            LogPrint("maxbudget", "fbvote - unknown maxnode - vin: %s\n", vote.vin.prevout.hash.ToString());
            maxnodeman.AskForMAX(pfrom, vote.vin);
            return;
        }

        mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        if (!vote.SignatureValid(true)) {
            /**if (maxnodeSync.IsSynced()) {
                LogPrintf("CMAXBudgetManager::ProcessMessage() : fbvote - signature invalid\n");
                Misbehaving(pfrom->GetId(), 20);
            }**/
            // it could just be a non-synced maxnode
            maxnodeman.AskForMAX(pfrom, vote.vin);
            return;
        }

        std::string strError = "";
        if (UpdateFinalizedBudget(vote, pfrom, strError)) {
            vote.Relay();
            //maxnodeSync.AddedBudgetItem(vote.GetHash());

            LogPrint("maxbudget","fbvote - new finalized maxbudget vote - %s\n", vote.GetHash().ToString());
        } else {
            LogPrint("maxbudget","fbvote - rejected finalized maxbudget vote - %s - %s\n", vote.GetHash().ToString(), strError);
        }
    }
}

bool CMAXBudgetManager::PropExists(uint256 nHash)
{
    if (mapProposals.count(nHash)) return true;
    return false;
}

//mark that a full sync is needed
void CMAXBudgetManager::ResetSync()
{
    LOCK(cs);


    std::map<uint256, CMAXBudgetProposalBroadcast>::iterator it1 = mapSeenMaxnodeBudgetProposals.begin();
    while (it1 != mapSeenMaxnodeBudgetProposals.end()) {
        CMAXBudgetProposal* pmaxbudgetProposal = FindMaxProposal((*it1).first);
        if (pmaxbudgetProposal && pmaxbudgetProposal->fValid) {
            //mark votes
            std::map<uint256, CMAXBudgetVote>::iterator it2 = pmaxbudgetProposal->mapVotes.begin();
            while (it2 != pmaxbudgetProposal->mapVotes.end()) {
                (*it2).second.fSynced = false;
                ++it2;
            }
        }
        ++it1;
    }

    std::map<uint256, CMAXFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid) {
            //send votes
            std::map<uint256, CMAXFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                (*it4).second.fSynced = false;
                ++it4;
            }
        }
        ++it3;
    }
}

void CMAXBudgetManager::MarkSynced()
{
    LOCK(cs);

    /*
        Mark that we've sent all valid items
    */

    std::map<uint256, CMAXBudgetProposalBroadcast>::iterator it1 = mapSeenMaxnodeBudgetProposals.begin();
    while (it1 != mapSeenMaxnodeBudgetProposals.end()) {
        CMAXBudgetProposal* pmaxbudgetProposal = FindMaxProposal((*it1).first);
        if (pmaxbudgetProposal && pmaxbudgetProposal->fValid) {
            //mark votes
            std::map<uint256, CMAXBudgetVote>::iterator it2 = pmaxbudgetProposal->mapVotes.begin();
            while (it2 != pmaxbudgetProposal->mapVotes.end()) {
                if ((*it2).second.fValid)
                    (*it2).second.fSynced = true;
                ++it2;
            }
        }
        ++it1;
    }

    std::map<uint256, CMAXFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid) {
            //mark votes
            std::map<uint256, CMAXFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                if ((*it4).second.fValid)
                    (*it4).second.fSynced = true;
                ++it4;
            }
        }
        ++it3;
    }
}


void CMAXBudgetManager::Sync(CNode* pfrom, uint256 nProp, bool fPartial)
{
    LOCK(cs);

    /*
        Sync with a client on the network

        --

        This code checks each of the hash maps for all known maxbudget proposals and finalized maxbudget proposals, then checks them against the
        maxbudget object to see if they're OK. If all checks pass, we'll send it to the peer.

    */

    int nInvCount = 0;

    std::map<uint256, CMAXBudgetProposalBroadcast>::iterator it1 = mapSeenMaxnodeBudgetProposals.begin();
    while (it1 != mapSeenMaxnodeBudgetProposals.end()) {
        CMAXBudgetProposal* pmaxbudgetProposal = FindMaxProposal((*it1).first);
        if (pmaxbudgetProposal && pmaxbudgetProposal->fValid && (nProp == 0 || (*it1).first == nProp)) {
            pfrom->PushInventory(CInv(MSG_MAX_BUDGET_PROPOSAL, (*it1).second.GetHash()));
            nInvCount++;

            //send votes
            std::map<uint256, CMAXBudgetVote>::iterator it2 = pmaxbudgetProposal->mapVotes.begin();
            while (it2 != pmaxbudgetProposal->mapVotes.end()) {
                if ((*it2).second.fValid) {
                    if ((fPartial && !(*it2).second.fSynced) || !fPartial) {
                        pfrom->PushInventory(CInv(MSG_MAX_BUDGET_VOTE, (*it2).second.GetHash()));
                        nInvCount++;
                    }
                }
                ++it2;
            }
        }
        ++it1;
    }

    pfrom->PushMessage("smaxsc", MAXNODE_SYNC_BUDGET_PROP, nInvCount);

    LogPrint("maxbudget", "CMAXBudgetManager::Sync - sent %d items\n", nInvCount);

    nInvCount = 0;

    std::map<uint256, CMAXFinalizedBudgetBroadcast>::iterator it3 = mapSeenFinalizedBudgets.begin();
    while (it3 != mapSeenFinalizedBudgets.end()) {
        CMAXFinalizedBudget* pfinalizedBudget = FindFinalizedBudget((*it3).first);
        if (pfinalizedBudget && pfinalizedBudget->fValid && (nProp == 0 || (*it3).first == nProp)) {
            pfrom->PushInventory(CInv(MSG_MAX_BUDGET_FINALIZED, (*it3).second.GetHash()));
            nInvCount++;

            //send votes
            std::map<uint256, CMAXFinalizedBudgetVote>::iterator it4 = pfinalizedBudget->mapVotes.begin();
            while (it4 != pfinalizedBudget->mapVotes.end()) {
                if ((*it4).second.fValid) {
                    if ((fPartial && !(*it4).second.fSynced) || !fPartial) {
                        pfrom->PushInventory(CInv(MSG_MAX_BUDGET_FINALIZED_VOTE, (*it4).second.GetHash()));
                        nInvCount++;
                    }
                }
                ++it4;
            }
        }
        ++it3;
    }

    pfrom->PushMessage("smaxsc", MAXNODE_SYNC_BUDGET_FIN, nInvCount);
    LogPrint("maxbudget", "CMAXBudgetManager::Sync - sent %d items\n", nInvCount);
}

bool CMAXBudgetManager::UpdateProposal(CMAXBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    if (!mapProposals.count(vote.nProposalHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            //if (!maxnodeSync.IsSynced()) return false;

            LogPrint("maxbudget","CMAXBudgetManager::UpdateProposal - Unknown proposal %d, asking for source proposal\n", vote.nProposalHash.ToString());
            mapOrphanMaxnodeBudgetVotes[vote.nProposalHash] = vote;

            if (!askedForMaxSourceProposalOrBudget.count(vote.nProposalHash)) {
                pfrom->PushMessage("maxvs", vote.nProposalHash);
                askedForMaxSourceProposalOrBudget[vote.nProposalHash] = GetTime();
            }
        }

        strError = "Proposal not found!";
        return false;
    }


    return mapProposals[vote.nProposalHash].AddOrUpdateVote(vote, strError);
}

bool CMAXBudgetManager::UpdateFinalizedBudget(CMAXFinalizedBudgetVote& vote, CNode* pfrom, std::string& strError)
{
    LOCK(cs);

    if (!mapFinalizedBudgets.count(vote.nBudgetHash)) {
        if (pfrom) {
            // only ask for missing items after our syncing process is complete --
            //   otherwise we'll think a full sync succeeded when they return a result
            //if (!maxnodeSync.IsSynced()) return false;

            LogPrint("maxbudget","CMAXBudgetManager::UpdateFinalizedBudget - Unknown Finalized Proposal %s, asking for source maxbudget\n", vote.nBudgetHash.ToString());
            mapOrphanFinalizedBudgetVotes[vote.nBudgetHash] = vote;

            if (!askedForMaxSourceProposalOrBudget.count(vote.nBudgetHash)) {
                pfrom->PushMessage("maxvs", vote.nBudgetHash);
                askedForMaxSourceProposalOrBudget[vote.nBudgetHash] = GetTime();
            }
        }

        strError = "Finalized Budget " + vote.nBudgetHash.ToString() +  " not found!";
        return false;
    }
    LogPrint("maxbudget","CMAXBudgetManager::UpdateFinalizedBudget - Finalized Proposal %s added\n", vote.nBudgetHash.ToString());
    return mapFinalizedBudgets[vote.nBudgetHash].AddOrUpdateVote(vote, strError);
}

CMAXBudgetProposal::CMAXBudgetProposal()
{
    strProposalName = "unknown";
    nBlockStart = 0;
    nBlockEnd = 0;
    nAmount = 0;
    nTime = 0;
    fValid = true;
}

CMAXBudgetProposal::CMAXBudgetProposal(std::string strProposalNameIn, std::string strURLIn, int nBlockStartIn, int nBlockEndIn, CScript addressIn, CAmount nAmountIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;
    nBlockStart = nBlockStartIn;
    nBlockEnd = nBlockEndIn;
    address = addressIn;
    nAmount = nAmountIn;
    nFeeTXHash = nFeeTXHashIn;
    fValid = true;
}

CMAXBudgetProposal::CMAXBudgetProposal(const CMAXBudgetProposal& other)
{
    strProposalName = other.strProposalName;
    strURL = other.strURL;
    nBlockStart = other.nBlockStart;
    nBlockEnd = other.nBlockEnd;
    address = other.address;
    nAmount = other.nAmount;
    nTime = other.nTime;
    nFeeTXHash = other.nFeeTXHash;
    mapVotes = other.mapVotes;
    fValid = true;
}

bool CMAXBudgetProposal::IsValid(std::string& strError, bool fCheckCollateral)
{
    if (GetMaxNays() - GetMaxYeas() > maxnodeman.CountEnabled(ActiveProtocol()) / 10) {
        strError = "Proposal " + strProposalName + ": Active removal";
        return false;
    }

    if (nBlockStart < 0) {
        strError = "Invalid Proposal";
        return false;
    }

    if (nBlockEnd < nBlockStart) {
        strError = "Proposal " + strProposalName + ": Invalid nBlockEnd (end before start)";
        return false;
    }

    if (nAmount < 10 * COIN) {
        strError = "Proposal " + strProposalName + ": Invalid nAmount";
        return false;
    }

    if (address == CScript()) {
        strError = "Proposal " + strProposalName + ": Invalid Payment Address";
        return false;
    }

    if (fCheckCollateral) {
        int nConf = 0;
        if (!IsMaxBudgetCollateralValid(nFeeTXHash, GetHash(), strError, nTime, nConf)) {
            strError = "Proposal " + strProposalName + ": Invalid collateral";
            return false;
        }
    }

    /*
        TODO: There might be an issue with multisig in the coinbase on mainnet, we will add support for it in a future release.
    */
    if (address.IsPayToScriptHash()) {
        strError = "Proposal " + strProposalName + ": Multisig is not currently supported.";
        return false;
    }

    //if proposal doesn't gain traction within 2 weeks, remove it
    // nTime not being saved correctly
    // -- TODO: We should keep track of the last time the proposal was valid, if it's invalid for 2 weeks, erase it
    // if(nTime + (60*60*24*2) < GetAdjustedTime()) {
    //     if(GetMaxYeas()-GetMaxNays() < (maxnodeman.CountEnabled(ActiveProtocol())/10)) {
    //         strError = "Not enough support";
    //         return false;
    //     }
    // }

    //can only pay out 10% of the possible coins (min value of coins)
    if (nAmount > maxbudget.GetMaxTotalBudget(nBlockStart)) {
        strError = "Proposal " + strProposalName + ": Payment more than max";
        return false;
    }

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) {
        strError = "Proposal " + strProposalName + ": Tip is NULL";
        return true;
    }

    // Calculate maximum block this proposal will be valid, which is start of proposal + (number of payments * cycle)
    int nProposalEnd = GetMaxBlockStart() + (GetMaxBudgetPaymentCycleBlocks() * GetMaxTotalPaymentCount());

    // if (GetMaxBlockEnd() < pindexPrev->nHeight - GetMaxBudgetPaymentCycleBlocks() / 2) {
    if(nProposalEnd < pindexPrev->nHeight){
        strError = "Proposal " + strProposalName + ": Invalid nBlockEnd (" + std::to_string(nProposalEnd) + ") < current height (" + std::to_string(pindexPrev->nHeight) + ")";
        return false;
    }

    return true;
}

bool CMAXBudgetProposal::AddOrUpdateVote(CMAXBudgetVote& vote, std::string& strError)
{
    std::string strAction = "New vote inserted:";
    LOCK(cs);

    uint256 hash = vote.vin.prevout.GetHash();

    if (mapVotes.count(hash)) {
        if (mapVotes[hash].nTime > vote.nTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint("maxbudget", "CMAXBudgetProposal::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        if (vote.nTime - mapVotes[hash].nTime < MAX_BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli sec < %lli sec\n", vote.GetHash().ToString(), vote.nTime - mapVotes[hash].nTime,MAX_BUDGET_VOTE_UPDATE_MIN);
            LogPrint("maxbudget", "CMAXBudgetProposal::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        strAction = "Existing vote updated:";
    }

    if (vote.nTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n", vote.GetHash().ToString(), vote.nTime, GetTime() + (60 * 60));
        LogPrint("maxbudget", "CMAXBudgetProposal::AddOrUpdateVote - %s\n", strError);
        return false;
    }

    mapVotes[hash] = vote;
    LogPrint("maxbudget", "CMAXBudgetProposal::AddOrUpdateVote - %s %s\n", strAction.c_str(), vote.GetHash().ToString().c_str());

    return true;
}

// If maxnode voted for a proposal, but is now invalid -- remove the vote
void CMAXBudgetProposal::CleanAndRemove(bool fSignatureCheck)
{
    std::map<uint256, CMAXBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        (*it).second.fValid = (*it).second.SignatureValid(fSignatureCheck);
        ++it;
    }
}

double CMAXBudgetProposal::GetMaxRatio()
{
    int yeas = 0;
    int nays = 0;

    std::map<uint256, CMAXBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_YES) yeas++;
        if ((*it).second.nVote == VOTE_NO) nays++;
        ++it;
    }

    if (yeas + nays == 0) return 0.0f;

    return ((double)(yeas) / (double)(yeas + nays));
}

int CMAXBudgetProposal::GetMaxYeas()
{
    int ret = 0;

    std::map<uint256, CMAXBudgetVote>::iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_YES && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CMAXBudgetProposal::GetMaxNays()
{
    int ret = 0;

    std::map<uint256, CMAXBudgetVote>::iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_NO && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CMAXBudgetProposal::GetMaxAbstains()
{
    int ret = 0;

    std::map<uint256, CMAXBudgetVote>::iterator it = mapVotes.begin();
    while (it != mapVotes.end()) {
        if ((*it).second.nVote == VOTE_ABSTAIN && (*it).second.fValid) ret++;
        ++it;
    }

    return ret;
}

int CMAXBudgetProposal::GetMaxBlockStartCycle()
{
    //end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)

    return nBlockStart - nBlockStart % GetMaxBudgetPaymentCycleBlocks();
}

int CMAXBudgetProposal::GetMaxBlockCurrentCycle()
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return -1;

    if (pindexPrev->nHeight >= GetMaxBlockEndCycle()) return -1;

    return pindexPrev->nHeight - pindexPrev->nHeight % GetMaxBudgetPaymentCycleBlocks();
}

int CMAXBudgetProposal::GetMaxBlockEndCycle()
{
    // Right now single payment proposals have nBlockEnd have a cycle too early!
    // switch back if it break something else
    // end block is half way through the next cycle (so the proposal will be removed much after the payment is sent)
    // return nBlockEnd - GetMaxBudgetPaymentCycleBlocks() / 2;

    // End block is half way through the next cycle (so the proposal will be removed much after the payment is sent)
    return nBlockEnd;

}

int CMAXBudgetProposal::GetMaxTotalPaymentCount()
{
    return (GetMaxBlockEndCycle() - GetMaxBlockStartCycle()) / GetMaxBudgetPaymentCycleBlocks();
}

int CMAXBudgetProposal::GetMaxRemainingPaymentCount()
{
    // If this maxbudget starts in the future, this value will be wrong
    int nPayments = (GetMaxBlockEndCycle() - GetMaxBlockCurrentCycle()) / GetMaxBudgetPaymentCycleBlocks() - 1;
    // Take the lowest value
    return std::min(nPayments, GetMaxTotalPaymentCount());
}

CMAXBudgetProposalBroadcast::CMAXBudgetProposalBroadcast(std::string strProposalNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;

    nBlockStart = nBlockStartIn;

    int nCycleStart = nBlockStart - nBlockStart % GetMaxBudgetPaymentCycleBlocks();

    // Right now single payment proposals have nBlockEnd have a cycle too early!
    // switch back if it break something else
    // calculate the end of the cycle for this vote, add half a cycle (vote will be deleted after that block)
    // nBlockEnd = nCycleStart + GetMaxBudgetPaymentCycleBlocks() * nPaymentCount + GetMaxBudgetPaymentCycleBlocks() / 2;

    // Calculate the end of the cycle for this vote, vote will be deleted after next cycle
    nBlockEnd = nCycleStart + (GetMaxBudgetPaymentCycleBlocks() + 1)  * nPaymentCount;

    address = addressIn;
    nAmount = nAmountIn;

    nFeeTXHash = nFeeTXHashIn;
}

void CMAXBudgetProposalBroadcast::Relay()
{
    CInv inv(MSG_MAX_BUDGET_PROPOSAL, GetHash());
    RelayInv(inv);
}

CMAXBudgetVote::CMAXBudgetVote()
{
    vin = CTxIn();
    nProposalHash = 0;
    nVote = VOTE_ABSTAIN;
    nTime = 0;
    fValid = true;
    fSynced = false;
}

CMAXBudgetVote::CMAXBudgetVote(CTxIn vinIn, uint256 nProposalHashIn, int nVoteIn)
{
    vin = vinIn;
    nProposalHash = nProposalHashIn;
    nVote = nVoteIn;
    nTime = GetAdjustedTime();
    fValid = true;
    fSynced = false;
}

void CMAXBudgetVote::Relay()
{
    CInv inv(MSG_MAX_BUDGET_VOTE, GetHash());
    RelayInv(inv);
}

bool CMAXBudgetVote::Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode)
{
    // Choose coins to use
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;

    std::string errorMessage;
    std::string strMessage = vin.prevout.ToStringShort() + nProposalHash.ToString() + boost::lexical_cast<std::string>(nVote) + boost::lexical_cast<std::string>(nTime);

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyMaxnode)) {
        LogPrint("maxbudget","CMAXBudgetVote::Sign - Error upon calling SignMessage");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
        LogPrint("maxbudget","CMAXBudgetVote::Sign - Error upon calling VerifyMessage");
        return false;
    }

    return true;
}

bool CMAXBudgetVote::SignatureValid(bool fSignatureCheck)
{
    std::string errorMessage;
    std::string strMessage = vin.prevout.ToStringShort() + nProposalHash.ToString() + boost::lexical_cast<std::string>(nVote) + boost::lexical_cast<std::string>(nTime);

    CMaxnode* pmax = maxnodeman.Find(vin);

    if (pmax == NULL) {
        if (fDebug){
            LogPrint("maxbudget","CMAXBudgetVote::SignatureValid() - Unknown Maxnode - %s\n", vin.prevout.hash.ToString());
        }
        return false;
    }

    if (!fSignatureCheck) return true;

    if (!obfuScationSigner.VerifyMessage(pmax->pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
        LogPrint("maxbudget","CMAXBudgetVote::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

CMAXFinalizedBudget::CMAXFinalizedBudget()
{
    strBudgetName = "";
    nBlockStart = 0;
    vecBudgetPayments.clear();
    mapVotes.clear();
    nFeeTXHash = 0;
    nTime = 0;
    fValid = true;
    fAutoChecked = false;
}

CMAXFinalizedBudget::CMAXFinalizedBudget(const CMAXFinalizedBudget& other)
{
    strBudgetName = other.strBudgetName;
    nBlockStart = other.nBlockStart;
    vecBudgetPayments = other.vecBudgetPayments;
    mapVotes = other.mapVotes;
    nFeeTXHash = other.nFeeTXHash;
    nTime = other.nTime;
    fValid = true;
    fAutoChecked = false;
}

bool CMAXFinalizedBudget::AddOrUpdateVote(CMAXFinalizedBudgetVote& vote, std::string& strError)
{
    LOCK(cs);

    uint256 hash = vote.vin.prevout.GetHash();
    std::string strAction = "New vote inserted:";

    if (mapVotes.count(hash)) {
        if (mapVotes[hash].nTime > vote.nTime) {
            strError = strprintf("new vote older than existing vote - %s\n", vote.GetHash().ToString());
            LogPrint("maxbudget", "CMAXFinalizedBudget::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        if (vote.nTime - mapVotes[hash].nTime < MAX_BUDGET_VOTE_UPDATE_MIN) {
            strError = strprintf("time between votes is too soon - %s - %lli sec < %lli sec\n", vote.GetHash().ToString(), vote.nTime - mapVotes[hash].nTime,MAX_BUDGET_VOTE_UPDATE_MIN);
            LogPrint("maxbudget", "CMAXFinalizedBudget::AddOrUpdateVote - %s\n", strError);
            return false;
        }
        strAction = "Existing vote updated:";
    }

    if (vote.nTime > GetTime() + (60 * 60)) {
        strError = strprintf("new vote is too far ahead of current time - %s - nTime %lli - Max Time %lli\n", vote.GetHash().ToString(), vote.nTime, GetTime() + (60 * 60));
        LogPrint("maxbudget", "CMAXFinalizedBudget::AddOrUpdateVote - %s\n", strError);
        return false;
    }

    mapVotes[hash] = vote;
    LogPrint("maxbudget", "CMAXFinalizedBudget::AddOrUpdateVote - %s %s\n", strAction.c_str(), vote.GetHash().ToString().c_str());
    return true;
}

//evaluate if we should vote for this. Maxnode only
void CMAXFinalizedBudget::AutoCheck()
{
    LOCK(cs);

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - %lli - %d\n", pindexPrev->nHeight, fAutoChecked);

    if (!fMaxNode || fAutoChecked) {
        LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck fMaxNode=%d fAutoChecked=%d\n", fMaxNode, fAutoChecked);
        return;
    }

    /**if (!fMaxNodeT2 || fAutoChecked) {
        LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck fMaxNodeT2=%d fAutoChecked=%d\n", fMaxNodeT2, fAutoChecked);
        return;
    }

    if (!fMaxNodeT3 || fAutoChecked) {
        LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck fMaxNodeT3=%d fAutoChecked=%d\n", fMaxNodeT3, fAutoChecked);
        return;
    }**/


    // Do this 1 in 4 blocks -- spread out the voting activity
    // -- this function is only called every fourteenth block, so this is really 1 in 56 blocks
    if (rand() % 4 != 0) {
        LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - waiting\n");
        return;
    }

    fAutoChecked = true; //we only need to check this once


    if (strBudgetMode == "auto") //only vote for exact matches
    {
        std::vector<CMAXBudgetProposal*> vBudgetProposals = maxbudget.GetBudget();


        for (unsigned int i = 0; i < vecBudgetPayments.size(); i++) {
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck Budget-Payments - nProp %d %s\n", i, vecBudgetPayments[i].nProposalHash.ToString());
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck Budget-Payments - Payee %d %s\n", i, vecBudgetPayments[i].payee.ToString());
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck Budget-Payments - nAmount %d %lli\n", i, vecBudgetPayments[i].nAmount);
        }

        for (unsigned int i = 0; i < vBudgetProposals.size(); i++) {
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck Budget-Proposals - nProp %d %s\n", i, vBudgetProposals[i]->GetHash().ToString());
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck Budget-Proposals - Payee %d %s\n", i, vBudgetProposals[i]->GetPayee().ToString());
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck Budget-Proposals - nAmount %d %lli\n", i, vBudgetProposals[i]->GetAmount());
        }

        if (vBudgetProposals.size() == 0) {
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - No Budget-Proposals found, aborting\n");
            return;
        }

        if (vBudgetProposals.size() != vecBudgetPayments.size()) {
            LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - Budget-Proposal length (%ld) doesn't match Budget-Payment length (%ld).\n",
                      vBudgetProposals.size(), vecBudgetPayments.size());
            return;
        }


        for (unsigned int i = 0; i < vecBudgetPayments.size(); i++) {
            if (i > vBudgetProposals.size() - 1) {
                LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - Proposal size mismatch, i=%d > (vBudgetProposals.size() - 1)=%d\n", i, vBudgetProposals.size() - 1);
                return;
            }

            if (vecBudgetPayments[i].nProposalHash != vBudgetProposals[i]->GetHash()) {
                LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - item #%d doesn't match %s %s\n", i, vecBudgetPayments[i].nProposalHash.ToString(), vBudgetProposals[i]->GetHash().ToString());
                return;
            }

            // if(vecBudgetPayments[i].payee != vBudgetProposals[i]->GetPayee()){ -- triggered with false positive
            if (vecBudgetPayments[i].payee.ToString() != vBudgetProposals[i]->GetPayee().ToString()) {
                LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - item #%d payee doesn't match %s %s\n", i, vecBudgetPayments[i].payee.ToString(), vBudgetProposals[i]->GetPayee().ToString());
                return;
            }

            if (vecBudgetPayments[i].nAmount != vBudgetProposals[i]->GetAmount()) {
                LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - item #%d payee doesn't match %lli %lli\n", i, vecBudgetPayments[i].nAmount, vBudgetProposals[i]->GetAmount());
                return;
            }
        }

        LogPrint("maxbudget","CMAXFinalizedBudget::AutoCheck - Finalized Budget Matches! Submitting Vote.\n");
        SubmitVote();
    }
}
// If maxnode voted for a proposal, but is now invalid -- remove the vote
void CMAXFinalizedBudget::CleanAndRemove(bool fSignatureCheck)
{
    std::map<uint256, CMAXFinalizedBudgetVote>::iterator it = mapVotes.begin();

    while (it != mapVotes.end()) {
        (*it).second.fValid = (*it).second.SignatureValid(fSignatureCheck);
        ++it;
    }
}


CAmount CMAXFinalizedBudget::GetMaxTotalPayout()
{
    CAmount ret = 0;

    for (unsigned int i = 0; i < vecBudgetPayments.size(); i++) {
        ret += vecBudgetPayments[i].nAmount;
    }

    return ret;
}

std::string CMAXFinalizedBudget::GetMaxProposals()
{
    LOCK(cs);
    std::string ret = "";

    BOOST_FOREACH (CMAXTxBudgetPayment& maxbudgetPayment, vecBudgetPayments) {
        CMAXBudgetProposal* pmaxbudgetProposal = maxbudget.FindMaxProposal(maxbudgetPayment.nProposalHash);

        std::string token = maxbudgetPayment.nProposalHash.ToString();

        if (pmaxbudgetProposal) token = pmaxbudgetProposal->GetName();
        if (ret == "") {
            ret = token;
        } else {
            ret += "," + token;
        }
    }
    return ret;
}

std::string CMAXFinalizedBudget::GetStatus()
{
    std::string retBadHashes = "";
    std::string retBadPayeeOrAmount = "";

    for (int nBlockHeight = GetMaxBlockStart(); nBlockHeight <= GetMaxBlockEnd(); nBlockHeight++) {
        CMAXTxBudgetPayment maxbudgetPayment;
        if (!GetMaxBudgetPaymentByBlock(nBlockHeight, maxbudgetPayment)) {
            LogPrint("maxbudget","CMAXFinalizedBudget::GetStatus - Couldn't find maxbudget payment for block %lld\n", nBlockHeight);
            continue;
        }

        CMAXBudgetProposal* pmaxbudgetProposal = maxbudget.FindMaxProposal(maxbudgetPayment.nProposalHash);
        if (!pmaxbudgetProposal) {
            if (retBadHashes == "") {
                retBadHashes = "Unknown proposal hash! Check this proposal before voting: " + maxbudgetPayment.nProposalHash.ToString();
            } else {
                retBadHashes += "," + maxbudgetPayment.nProposalHash.ToString();
            }
        } else {
            if (pmaxbudgetProposal->GetPayee() != maxbudgetPayment.payee || pmaxbudgetProposal->GetAmount() != maxbudgetPayment.nAmount) {
                if (retBadPayeeOrAmount == "") {
                    retBadPayeeOrAmount = "Budget payee/nAmount doesn't match our proposal! " + maxbudgetPayment.nProposalHash.ToString();
                } else {
                    retBadPayeeOrAmount += "," + maxbudgetPayment.nProposalHash.ToString();
                }
            }
        }
    }

    if (retBadHashes == "" && retBadPayeeOrAmount == "") return "OK";

    return retBadHashes + retBadPayeeOrAmount;
}

bool CMAXFinalizedBudget::IsValid(std::string& strError, bool fCheckCollateral)
{
    // All(!) finalized maxbudgets have the name "main", so get some additional information about them
    std::string strProposals = GetMaxProposals();
    
    // Must be the correct block for payment to happen (once a month)
    if (nBlockStart % GetMaxBudgetPaymentCycleBlocks() != 0) {
        strError = "Invalid BlockStart";
        return false;
    }

    // The following 2 checks check the same (basically if vecBudgetPayments.size() > 100)
    if (GetMaxBlockEnd() - nBlockStart > 100) {
        strError = "Invalid BlockEnd";
        return false;
    }
    if ((int)vecBudgetPayments.size() > 100) {
        strError = "Invalid maxbudget payments count (too many)";
        return false;
    }
    if (strBudgetName == "") {
        strError = "Invalid Budget Name";
        return false;
    }
    if (nBlockStart == 0) {
        strError = "Budget " + strBudgetName + " (" + strProposals + ") Invalid BlockStart == 0";
        return false;
    }
    if (nFeeTXHash == 0) {
        strError = "Budget " + strBudgetName  + " (" + strProposals + ") Invalid FeeTx == 0";
        return false;
    }

    // Can only pay out 10% of the possible coins (min value of coins)
    if (GetMaxTotalPayout() > maxbudget.GetMaxTotalBudget(nBlockStart)) {
        strError = "Budget " + strBudgetName + " (" + strProposals + ") Invalid Payout (more than max)";
        return false;
    }

    std::string strError2 = "";
    if (fCheckCollateral) {
        int nConf = 0;
        if (!IsMaxBudgetCollateralValid(nFeeTXHash, GetHash(), strError2, nTime, nConf, true)) {
            {
                strError = "Budget " + strBudgetName + " (" + strProposals + ") Invalid Collateral : " + strError2;
                return false;
            }
        }
    }

    // Remove obsolete finalized maxbudgets after some time

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return true;

    // Get start of current maxbudget-cycle
    int nCurrentHeight = chainActive.Height();
    int nBlockStart = nCurrentHeight - nCurrentHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();

    // Remove maxbudgets where the last payment (from max. 100) ends before 2 maxbudget-cycles before the current one
    int nMaxAge = nBlockStart - (2 * GetMaxBudgetPaymentCycleBlocks());
    
    if (GetMaxBlockEnd() < nMaxAge) {
        strError = strprintf("Budget " + strBudgetName + " (" + strProposals + ") (ends at block %ld) too old and obsolete", GetMaxBlockEnd());
        return false;
    }

    return true;
}

bool CMAXFinalizedBudget::IsPaidAlready(uint256 nProposalHash, int nBlockHeight)
{
    // Remove maxbudget-payments from former/future payment cycles
    map<uint256, int>::iterator it = mapMaxPayment_History.begin();
    int nPaidBlockHeight = 0;
    uint256 nOldProposalHash;

    for(it = mapMaxPayment_History.begin(); it != mapMaxPayment_History.end(); /* No incrementation needed */ ) {
        nPaidBlockHeight = (*it).second;
        if((nPaidBlockHeight < GetMaxBlockStart()) || (nPaidBlockHeight > GetMaxBlockEnd())) {
            nOldProposalHash = (*it).first;
            LogPrint("maxbudget", "CMAXFinalizedBudget::IsPaidAlready - Budget Proposal %s, Block %d from old cycle deleted\n", 
                      nOldProposalHash.ToString().c_str(), nPaidBlockHeight);
            mapMaxPayment_History.erase(it++);
        }
        else {
            ++it;
        }
    }

    // Now that we only have payments from the current payment cycle check if this maxbudget was paid already
    if(mapMaxPayment_History.count(nProposalHash) == 0) {
        // New proposal payment, insert into map for checks with later blocks from this cycle
        mapMaxPayment_History.insert(std::pair<uint256, int>(nProposalHash, nBlockHeight));
        LogPrint("maxbudget", "CMAXFinalizedBudget::IsPaidAlready - Budget Proposal %s, Block %d added to payment history\n", 
                  nProposalHash.ToString().c_str(), nBlockHeight);
        return false;
    }
    // This maxbudget was paid already -> reject transaction so it gets paid to a maxnode instead
    return true;
}

MAXTrxValidationStatus CMAXFinalizedBudget::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    MAXTrxValidationStatus transactionStatus = MAXTrxValidationStatus::InValid;
    int nCurrentBudgetPayment = nBlockHeight - GetMaxBlockStart();
    if (nCurrentBudgetPayment < 0) {
        LogPrint("maxbudget","CMAXFinalizedBudget::IsTransactionValid - Invalid block - height: %d start: %d\n", nBlockHeight, GetMaxBlockStart());
        return MAXTrxValidationStatus::InValid;
    }

    if (nCurrentBudgetPayment > (int)vecBudgetPayments.size() - 1) {
        LogPrint("maxbudget","CMAXFinalizedBudget::IsTransactionValid - Invalid last block - current maxbudget payment: %d of %d\n", nCurrentBudgetPayment + 1, (int)vecBudgetPayments.size());
        return MAXTrxValidationStatus::InValid;
    }

    bool paid = false;

    BOOST_FOREACH (CTxOut out, txNew.vout) {
        LogPrint("maxbudget","CMAXFinalizedBudget::IsTransactionValid - nCurrentBudgetPayment=%d, payee=%s == out.scriptPubKey=%s, amount=%ld == out.nValue=%ld\n", 
                 nCurrentBudgetPayment, vecBudgetPayments[nCurrentBudgetPayment].payee.ToString().c_str(), out.scriptPubKey.ToString().c_str(),
                 vecBudgetPayments[nCurrentBudgetPayment].nAmount, out.nValue);

        if (vecBudgetPayments[nCurrentBudgetPayment].payee == out.scriptPubKey && vecBudgetPayments[nCurrentBudgetPayment].nAmount == out.nValue) {
            // Check if this proposal was paid already. If so, pay a maxnode instead
            paid = IsPaidAlready(vecBudgetPayments[nCurrentBudgetPayment].nProposalHash, nBlockHeight);
            if(paid) {
                LogPrint("maxbudget","CMAXFinalizedBudget::IsTransactionValid - Double Budget Payment of %d for proposal %d detected. Paying a maxnode instead.\n",
                          vecBudgetPayments[nCurrentBudgetPayment].nAmount, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.Get32());
                // No matter what we've found before, stop all checks here. In future releases there might be more than one maxbudget payment
                // per block, so even if the first one was not paid yet this one disables all maxbudget payments for this block.
                transactionStatus = MAXTrxValidationStatus::DoublePayment;
                break;
            }
            else {
                transactionStatus = MAXTrxValidationStatus::Valid;
                LogPrint("maxbudget","CMAXFinalizedBudget::IsTransactionValid - Found valid Budget Payment of %d for proposal %d\n",
                          vecBudgetPayments[nCurrentBudgetPayment].nAmount, vecBudgetPayments[nCurrentBudgetPayment].nProposalHash.Get32());
            }
        }
    }

    if (transactionStatus == MAXTrxValidationStatus::InValid) {
        CTxDestination address1;
        ExtractDestination(vecBudgetPayments[nCurrentBudgetPayment].payee, address1);
        CBitcoinAddress address2(address1);

        LogPrint("maxbudget","CMAXFinalizedBudget::IsTransactionValid - Missing required payment - %s: %d c: %d\n",
                  address2.ToString(), vecBudgetPayments[nCurrentBudgetPayment].nAmount, nCurrentBudgetPayment);
    }

    return transactionStatus;
}

void CMAXFinalizedBudget::SubmitVote()
{
    CPubKey pubKeyMaxnode;
    CKey keyMaxnode;
    std::string errorMessage;

    if (!obfuScationSigner.SetKey(strMaxNodePrivKey, errorMessage, keyMaxnode, pubKeyMaxnode)) {
        LogPrint("maxbudget","CMAXFinalizedBudget::SubmitVote - Error upon calling SetKey\n");
        return;
    }

    CMAXFinalizedBudgetVote vote(activeMaxnode.maxvin, GetHash());
    if (!vote.Sign(keyMaxnode, pubKeyMaxnode)) {
        LogPrint("maxbudget","CMAXFinalizedBudget::SubmitVote - Failure to sign.");
        return;
    }

    std::string strError = "";
    if (maxbudget.UpdateFinalizedBudget(vote, NULL, strError)) {
        LogPrint("maxbudget","CMAXFinalizedBudget::SubmitVote  - new finalized maxbudget vote - %s\n", vote.GetHash().ToString());

        maxbudget.mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        vote.Relay();
    } else {
        LogPrint("maxbudget","CMAXFinalizedBudget::SubmitVote : Error submitting vote - %s\n", strError);
    }
}

CMAXFinalizedBudgetBroadcast::CMAXFinalizedBudgetBroadcast()
{
    strBudgetName = "";
    nBlockStart = 0;
    vecBudgetPayments.clear();
    mapVotes.clear();
    vchSig.clear();
    nFeeTXHash = 0;
}

CMAXFinalizedBudgetBroadcast::CMAXFinalizedBudgetBroadcast(const CMAXFinalizedBudget& other)
{
    strBudgetName = other.strBudgetName;
    nBlockStart = other.nBlockStart;
    BOOST_FOREACH (CMAXTxBudgetPayment out, other.vecBudgetPayments)
        vecBudgetPayments.push_back(out);
    mapVotes = other.mapVotes;
    nFeeTXHash = other.nFeeTXHash;
}

CMAXFinalizedBudgetBroadcast::CMAXFinalizedBudgetBroadcast(std::string strBudgetNameIn, int nBlockStartIn, std::vector<CMAXTxBudgetPayment> vecBudgetPaymentsIn, uint256 nFeeTXHashIn)
{
    strBudgetName = strBudgetNameIn;
    nBlockStart = nBlockStartIn;
    BOOST_FOREACH (CMAXTxBudgetPayment out, vecBudgetPaymentsIn)
        vecBudgetPayments.push_back(out);
    mapVotes.clear();
    nFeeTXHash = nFeeTXHashIn;
}

void CMAXFinalizedBudgetBroadcast::Relay()
{
    CInv inv(MSG_MAX_BUDGET_FINALIZED, GetHash());
    RelayInv(inv);
}

CMAXFinalizedBudgetVote::CMAXFinalizedBudgetVote()
{
    vin = CTxIn();
    nBudgetHash = 0;
    nTime = 0;
    vchSig.clear();
    fValid = true;
    fSynced = false;
}

CMAXFinalizedBudgetVote::CMAXFinalizedBudgetVote(CTxIn vinIn, uint256 nBudgetHashIn)
{
    vin = vinIn;
    nBudgetHash = nBudgetHashIn;
    nTime = GetAdjustedTime();
    vchSig.clear();
    fValid = true;
    fSynced = false;
}

void CMAXFinalizedBudgetVote::Relay()
{
    CInv inv(MSG_MAX_BUDGET_FINALIZED_VOTE, GetHash());
    RelayInv(inv);
}

bool CMAXFinalizedBudgetVote::Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode)
{
    // Choose coins to use
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;

    std::string errorMessage;
    std::string strMessage = vin.prevout.ToStringShort() + nBudgetHash.ToString() + boost::lexical_cast<std::string>(nTime);

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyMaxnode)) {
        LogPrint("maxbudget","CMAXFinalizedBudgetVote::Sign - Error upon calling SignMessage");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
        LogPrint("maxbudget","CMAXFinalizedBudgetVote::Sign - Error upon calling VerifyMessage");
        return false;
    }

    return true;
}

bool CMAXFinalizedBudgetVote::SignatureValid(bool fSignatureCheck)
{
    std::string errorMessage;

    std::string strMessage = vin.prevout.ToStringShort() + nBudgetHash.ToString() + boost::lexical_cast<std::string>(nTime);

    CMaxnode* pmax = maxnodeman.Find(vin);

    if (pmax == NULL) {
        LogPrint("maxbudget","CMAXFinalizedBudgetVote::SignatureValid() - Unknown Maxnode %s\n", strMessage);
        return false;
    }

    if (!fSignatureCheck) return true;

    if (!obfuScationSigner.VerifyMessage(pmax->pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
        LogPrint("maxbudget","CMAXFinalizedBudgetVote::SignatureValid() - Verify message failed %s %s\n", strMessage, errorMessage);
        return false;
    }

    return true;
}

std::string CMAXBudgetManager::ToString() const
{
    std::ostringstream info;

    info << "Proposals: " << (int)mapProposals.size() << ", Budgets: " << (int)mapFinalizedBudgets.size() << ", Seen Budgets: " << (int)mapSeenMaxnodeBudgetProposals.size() << ", Seen Budget Votes: " << (int)mapSeenMaxnodeBudgetVotes.size() << ", Seen Final Budgets: " << (int)mapSeenFinalizedBudgets.size() << ", Seen Final Budget Votes: " << (int)mapSeenFinalizedBudgetVotes.size();

    return info.str();
}
