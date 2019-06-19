// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "maxnode-payments.h"
#include "masternode-payments.h"
#include "addrman.h"
#include "maxnode-budget.h"
#include "maxnode-sync.h"
#include "maxnodeman.h"
#include "masternodeman.h"
#include "obfuscation.h"
#include "spork.h"
#include "sync.h"
#include "util.h"
#include "utilmoneystr.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CMaxnodePayments maxnodePayments;

CCriticalSection cs_MaxvecPayments;
CCriticalSection cs_mapMaxnodeBlocks;
CCriticalSection cs_mapMaxnodePayeeVotes;

//
// CMaxnodePaymentDB
//

CMaxnodePaymentDB::CMaxnodePaymentDB()
{
    pathDB = GetDataDir() / "maxpayments.dat";
    strMagicMessage = "MaxnodePayments";
}

bool CMaxnodePaymentDB::Write(const CMaxnodePayments& objToSave)
{
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

    LogPrint("maxnode","Written info to maxpayments.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CMaxnodePaymentDB::ReadResult CMaxnodePaymentDB::Read(CMaxnodePayments& objToLoad, bool fDryRun)
{
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
            error("%s : Invalid maxnode payement cache magic message", __func__);
            return IncorrectMagicMessage;
        }


        // de-serialize file header (network specific magic number) and ..
        ssObj >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CMaxnodePayments object
        ssObj >> objToLoad;
    } catch (std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint("maxnode","Loaded info from maxpayments.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint("maxnode","  %s\n", objToLoad.ToString());
    if (!fDryRun) {
        LogPrint("maxnode","Maxnode payments manager - cleaning....\n");
        objToLoad.CleanPaymentList();
        LogPrint("maxnode","Maxnode payments manager - result:\n");
        LogPrint("maxnode","  %s\n", objToLoad.ToString());
    }

    return Ok;
}

void DumpMaxnodePayments()
{
    int64_t nStart = GetTimeMillis();

    CMaxnodePaymentDB paymentdb;
    CMaxnodePayments tempPayments;

    LogPrint("maxnode","Verifying maxpayments.dat format...\n");
    CMaxnodePaymentDB::ReadResult readResult = paymentdb.Read(tempPayments, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CMaxnodePaymentDB::FileError)
        LogPrint("maxnode","Missing budgets file - maxpayments.dat, will try to recreate\n");
    else if (readResult != CMaxnodePaymentDB::Ok) {
        LogPrint("maxnode","Error reading maxpayments.dat: ");
        if (readResult == CMaxnodePaymentDB::IncorrectFormat)
            LogPrint("maxnode","magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrint("maxnode","file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrint("maxnode","Writting info to maxpayments.dat...\n");
    paymentdb.Write(maxnodePayments);

    LogPrint("maxnode","Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool IsMaxBlockValueValid(const CBlock& block, CAmount nExpectedValue, CAmount nMinted)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return true;

    int nHeight = 0;
    if (pindexPrev->GetMaxBlockHash() == block.hashPrevBlock) {
        nHeight = pindexPrev->nHeight + 1;
    } else { //out of order
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
            nHeight = (*mi).second->nHeight + 1;
    }

    if (nHeight == 0) {
        LogPrint("maxnode","IsMaxBlockValueValid() : WARNING: Couldn't find previous block\n");
    }

    //LogPrintf("XX69----------> IsMaxBlockValueValid(): nMinted: %d, nExpectedValue: %d\n", FormatMoney(nMinted), FormatMoney(nExpectedValue));

    if (!maxnodeSync.IsSynced()) { //there is no budget data to use to check anything
        //super blocks will always be on these blocks, max 100 per budgeting
        if (nHeight % GetMaxBudgetPaymentCycleBlocks() < 100) {
            return true;
        } else {
            if (nMinted > nExpectedValue) {
                return false;
            }
        }
    } else { // we're synced and have data so check the budget schedule

        //are these blocks even enabled
        if (!IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
            return nMinted <= nExpectedValue;
        }

        if (maxbudget.IsBudgetPaymentBlock(nHeight)) {
            //the value of the block is evaluated in CheckBlock
            return true;
        } else {
            if (nMinted > nExpectedValue) {
                return false;
            }
        }
    }

    return true;
}

bool IsMaxBlockPayeeValid(const CBlock& block, int nBlockHeight)
{
    MAXTrxValidationStatus transactionStatus = MAXTrxValidationStatus::InValid;

    if (!maxnodeSync.IsSynced()) { //there is no budget data to use to check anything -- find the longest chain
        LogPrint("maxpayments", "Client not synced, skipping block payee checks\n");
        return true;
    }

    const CTransaction& txNew = (nBlockHeight > Params().LAST_POW_BLOCK() ? block.vtx[1] : block.vtx[0]);

    //check if it's a budget block
    if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
        if (maxbudget.IsBudgetPaymentBlock(nBlockHeight)) {
            transactionStatus = maxbudget.IsTransactionValid(txNew, nBlockHeight);
            if (transactionStatus == MAXTrxValidationStatus::Valid) {
                return true;
            }

            if (transactionStatus == MAXTrxValidationStatus::InValid) {
                LogPrint("maxnode","Invalid budget payment detected %s\n", txNew.ToString().c_str());
                if (IsSporkActive(SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT))
                    return false;

                LogPrint("maxnode","Budget enforcement is disabled, accepting block\n");
            }
        }
    }

    // If we end here the transaction was either TrxValidationStatus::InValid and Budget enforcement is disabled, or
    // a double budget payment (status = TrxValidationStatus::DoublePayment) was detected, or no/not enough maxnode
    // votes (status = TrxValidationStatus::VoteThreshold) for a finalized budget were found
    // In all cases a maxnode will get the payment for this block

    //check for maxnode payee
    if (maxnodePayments.IsTransactionValid(txNew, nBlockHeight))
        return true;
    LogPrint("maxnode","Invalid max payment detected %s\n", txNew.ToString().c_str());

    if (IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT))
        return false;
    LogPrint("maxnode","Maxnode payment enforcement is disabled, accepting block\n");

    return true;
}


void FillMaxBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake, bool fZPIVStake)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    /**if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && budget.IsBudgetPaymentBlock(pindexPrev->nHeight + 1)) {
        budget.FillMaxBlockPayee(txNew, nFees, fProofOfStake);
    } else {**/
        maxnodePayments.FillMaxBlockPayee(txNew, nFees, fProofOfStake, fZPIVStake);
    //}
}

std::string GetMaxRequiredPaymentsString(int nBlockHeight)
{
    /**if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && budget.IsBudgetPaymentBlock(nBlockHeight)) {
        return budget.GetMaxRequiredPaymentsString(nBlockHeight);
    } else {**/
        return maxnodePayments.GetMaxRequiredPaymentsString(nBlockHeight);
    //}
}


///JVD Method, Jerome approved, R.I.P., bro
//
void CMaxnodePayments::FillMaxBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake, bool fZPIVStake)
{
    //int lastPoW = Params().LAST_POW_BLOCK();
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    bool hasPayment = true;
    CScript payee;
    CScript payee2;
    CScript payee3;

    //spork
    if (!maxnodePayments.GetBlockPayee(pindexPrev->nHeight + 1, payee)) {
        //no maxnode detected
        CMaxnode* winningNode = maxnodeman.GetCurrentMaxNode(1);
        if (winningNode) {
            payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
        } else {
            LogPrint("maxnode","CreateNewBlock: Failed to detect maxnode to pay\n");
            hasPayment = false;
        }
    }

    if (!masternodePayments.GetBlockPayee(pindexPrev->nHeight + 1, payee2)) {
        //no masternode detected
        CMasternode* winningNode = mnodeman.GetCurrentMasterNode(1);
        if (winningNode) {
            payee2 = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
        } else {
            LogPrint("masternode","CreateNewBlock: Failed to detect masternode to pay\n");
            hasPayment = false;
        }
    }

    //Dev payment - if spork enabled in main then payment is 5% oitherwise it is zero
    CBitcoinAddress devFeeAddress(Params().DevFeeAddress());
    payee3 = GetScriptForDestination(devFeeAddress.Get()); 


    CAmount blockValue = GetBlockValue(pindexPrev->nHeight);
    CAmount maxnodePayment = GetMaxnodePayment(pindexPrev->nHeight, blockValue, 0, fZPIVStake);
    CAmount masternodePayment = GetMasternodePayment(pindexPrev->nHeight, blockValue, 0, fZPIVStake);
    CAmount devPayment = GetDevFeePayment(pindexPrev->nHeight, blockValue);

    if (hasPayment) {
        if (fProofOfStake) {
            /**For Proof Of Stake vout[0] must be null
             * Stake reward can be split into many different outputs, so we must
             * use vout.size() to align with several different cases.
             * An additional output is appended as the maxnode payment
             */
            unsigned int i = txNew.vout.size();
            txNew.vout.resize(i + 3);
	    txNew.vout[i + 2].scriptPubKey = payee2;
            txNew.vout[i + 2].nValue = masternodePayment;
            txNew.vout[i + 1].scriptPubKey = payee3;
            txNew.vout[i + 1].nValue = devPayment;
            txNew.vout[i].scriptPubKey = payee;
            txNew.vout[i].nValue = maxnodePayment;


            //subtract max payment from the stake reward
            if (!txNew.vout[1].IsZerocoinMint())
                txNew.vout[i - 1].nValue -= (masternodePayment + maxnodePayment + devPayment);
                //txNew.vout[i - 1].nValue -= maxnodePayment;
        } else {
            txNew.vout.resize(4);
            //txNew.vout.resize(3);
	    txNew.vout[3].scriptPubKey = payee3;
            txNew.vout[3].nValue = devPayment;
	    txNew.vout[2].scriptPubKey = payee2;
	    txNew.vout[2].nValue = masternodePayment;
            txNew.vout[1].scriptPubKey = payee;
            txNew.vout[1].nValue = maxnodePayment;
            //txNew.vout[0].nValue = blockValue - maxnodePayment - masternodePayment;
            txNew.vout[0].nValue = blockValue - maxnodePayment - masternodePayment - devPayment;
            //txNew.vout[0].nValue = blockValue;
        }

	//Maxnode Payment notification
        CTxDestination address1;
        ExtractDestination(payee, address1);
        CBitcoinAddress address2(address1);

        LogPrint("maxnode","Maxnode payment of %s to %s\n", FormatMoney(maxnodePayment).c_str(), address2.ToString().c_str());

	//Masternode Payment notification
	CTxDestination address3;
        ExtractDestination(payee2, address3);
        CBitcoinAddress address4(address3);

        LogPrint("masternode","Masternode payment of %s to %s\n", FormatMoney(masternodePayment).c_str(), address4.ToString().c_str());

	//Dev Fee Payment notification
	CTxDestination address5;
        ExtractDestination(payee3, address5);
        CBitcoinAddress address6(address5);

        LogPrint("devfee","Dev Fee payment of %s to %s\n", FormatMoney(devPayment).c_str(), address6.ToString().c_str());

    }
}

int CMaxnodePayments::GetMinMaxnodePaymentsProto()
{
    if (IsSporkActive(SPORK_10_MASTERNODE_PAY_UPDATED_NODES))
        return ActiveProtocol();                          // Allow only updated peers
    else
        return MIN_PEER_PROTO_VERSION_BEFORE_ENFORCEMENT; // Also allow old peers as long as they are allowed to run
}

void CMaxnodePayments::ProcessMessageMaxnodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (!maxnodeSync.IsBlockchainSynced()) return;

    if (fLiteMode) return; //disable all Obfuscation/Maxnode related functionality


    if (strCommand == "maxget") { //Maxnode Payments Request Sync
        if (fLiteMode) return;   //disable all Obfuscation/Maxnode related functionality

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (pfrom->HasFulfilledRequest("maxget")) {
                LogPrintf("CMaxnodePayments::ProcessMessageMaxnodePayments() : maxget - peer already asked me for the list\n");
                Misbehaving(pfrom->GetId(), 20);
                return;
            }
        }

        pfrom->FulfilledRequest("maxget");
        maxnodePayments.Sync(pfrom, nCountNeeded);
        LogPrint("maxpayments", "maxget - Sent Maxnode winners to peer %i\n", pfrom->GetId());
    } else if (strCommand == "maxw") { //Maxnode Payments Declare Winner
        //this is required in litemodef
        CMaxnodePaymentWinner winner;
        vRecv >> winner;

        if (pfrom->nVersion < ActiveProtocol()) return;

        int nHeight;
        {
            TRY_LOCK(cs_main, locked);
            if (!locked || chainActive.Tip() == NULL) return;
            nHeight = chainActive.Tip()->nHeight;
        }

        if (maxnodePayments.mapMaxnodePayeeVotes.count(winner.GetHash())) {
            LogPrint("maxpayments", "maxw - Already seen - %s bestHeight %d\n", winner.GetHash().ToString().c_str(), nHeight);
            maxnodeSync.AddedMaxnodeWinner(winner.GetHash());
            return;
        }

        int nFirstBlock = nHeight - (maxnodeman.CountEnabled() * 1.25);
        if (winner.nBlockHeight < nFirstBlock || winner.nBlockHeight > nHeight + 20) {
            LogPrint("maxpayments", "maxw - winner out of range - FirstBlock %d Height %d bestHeight %d\n", nFirstBlock, winner.nBlockHeight, nHeight);
            return;
        }

        std::string strError = "";
        if (!winner.IsValid(pfrom, strError)) {
            // if(strError != "") LogPrint("maxnode","maxw - invalid message - %s\n", strError);
            return;
        }

        if (!maxnodePayments.CanVote(winner.vinMaxnode.prevout, winner.nBlockHeight)) {
            //  LogPrint("maxnode","maxw - maxnode already voted - %s\n", winner.vinMaxnode.prevout.ToStringShort());
            return;
        }

        if (!winner.SignatureValid()) {
            if (maxnodeSync.IsSynced()) {
                LogPrintf("CMaxnodePayments::ProcessMessageMaxnodePayments() : maxw - invalid signature\n");
                Misbehaving(pfrom->GetId(), 20);
            }
            // it could just be a non-synced maxnode
            maxnodeman.AskForMAX(pfrom, winner.vinMaxnode);
            return;
        }

        CTxDestination address1;
        ExtractDestination(winner.payee, address1);
        CBitcoinAddress address2(address1);

        //   LogPrint("maxpayments", "maxw - winning vote - Addr %s Height %d bestHeight %d - %s\n", address2.ToString().c_str(), winner.nBlockHeight, nHeight, winner.vinMaxnode.prevout.ToStringShort());

        if (maxnodePayments.AddWinningMaxnode(winner)) {
            winner.Relay();
            maxnodeSync.AddedMaxnodeWinner(winner.GetHash());
        }
    }
}

bool CMaxnodePaymentWinner::Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode)
{
    std::string errorMessage;
    std::string strMaxNodeSignMessage;

    std::string strMessage = vinMaxnode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             payee.ToString();

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyMaxnode)) {
        LogPrint("maxnode","CMaxnodePing::Sign() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
        LogPrint("maxnode","CMaxnodePing::Sign() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    return true;
}

bool CMaxnodePayments::GetBlockPayee(int nBlockHeight, CScript& payee)
{
    if (mapMaxnodeBlocks.count(nBlockHeight)) {
        return mapMaxnodeBlocks[nBlockHeight].GetPayee(payee);
    }

    return false;
}

// Is this maxnode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 winners
bool CMaxnodePayments::IsScheduled(CMaxnode& max, int nNotBlockHeight)
{
    LOCK(cs_mapMaxnodeBlocks);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || chainActive.Tip() == NULL) return false;
        nHeight = chainActive.Tip()->nHeight;
    }

    CScript maxpayee;
    maxpayee = GetScriptForDestination(max.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = nHeight; h <= nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapMaxnodeBlocks.count(h)) {
            if (mapMaxnodeBlocks[h].GetPayee(payee)) {
                if (maxpayee == payee) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool CMaxnodePayments::AddWinningMaxnode(CMaxnodePaymentWinner& winnerIn)
{
    uint256 blockHash = 0;
    if (!GetMaxBlockHash(blockHash, winnerIn.nBlockHeight - 100)) {
        return false;
    }

    {
        LOCK2(cs_mapMaxnodePayeeVotes, cs_mapMaxnodeBlocks);

        if (mapMaxnodePayeeVotes.count(winnerIn.GetHash())) {
            return false;
        }

        mapMaxnodePayeeVotes[winnerIn.GetHash()] = winnerIn;

        if (!mapMaxnodeBlocks.count(winnerIn.nBlockHeight)) {
            CMaxnodeBlockPayees blockPayees(winnerIn.nBlockHeight);
            mapMaxnodeBlocks[winnerIn.nBlockHeight] = blockPayees;
        }
    }

    mapMaxnodeBlocks[winnerIn.nBlockHeight].AddPayee(winnerIn.payee, 1);

    return true;
}

bool CMaxnodeBlockPayees::IsTransactionValid(const CTransaction& txNew)
{
    LOCK(cs_MaxvecPayments);

    int nMaxSignatures = 0;
    int nMaxnode_Drift_Count = 0;

    std::string strPayeesPossible = "";

    CAmount nReward = GetBlockValue(nBlockHeight);

    if (IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT)) {
        // Get a stable number of maxnodes by ignoring newly activated (< 8000 sec old) maxnodes
        nMaxnode_Drift_Count = maxnodeman.stable_size() + Params().MaxnodeCountDrift();
    }
    else {
        //account for the fact that all peers do not see the same maxnode count. A allowance of being off our maxnode count is given
        //we only need to look at an increased maxnode count because as count increases, the reward decreases. This code only checks
        //for maxPayment >= required, so it only makes sense to check the max node count allowed.
        nMaxnode_Drift_Count = maxnodeman.size() + Params().MaxnodeCountDrift();
    }

    CAmount requiredMaxnodePayment = GetMaxnodePayment(nBlockHeight, nReward, nMaxnode_Drift_Count, txNew.IsZerocoinSpend());

    //require at least 6 signatures
    BOOST_FOREACH (CMaxnodePayee& payee, vecPayments)
        if (payee.nVotes >= nMaxSignatures && payee.nVotes >= MAXPAYMENTS_SIGNATURES_REQUIRED)
            nMaxSignatures = payee.nVotes;

    // if we don't have at least 6 signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MAXPAYMENTS_SIGNATURES_REQUIRED) return true;

    BOOST_FOREACH (CMaxnodePayee& payee, vecPayments) {
        bool found = false;
        BOOST_FOREACH (CTxOut out, txNew.vout) {
            if (payee.scriptPubKey == out.scriptPubKey) {
                if(out.nValue >= requiredMaxnodePayment)
                    found = true;
                else
                    LogPrint("maxnode","Maxnode payment is out of drift range. Paid=%s Min=%s\n", FormatMoney(out.nValue).c_str(), FormatMoney(requiredMaxnodePayment).c_str());
            }
        }

        if (payee.nVotes >= MAXPAYMENTS_SIGNATURES_REQUIRED) {
            if (found) return true;

            CTxDestination address1;
            ExtractDestination(payee.scriptPubKey, address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible += address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    LogPrint("maxnode","CMaxnodePayments::IsTransactionValid - Missing required payment of %s to %s\n", FormatMoney(requiredMaxnodePayment).c_str(), strPayeesPossible.c_str());
    return false;
}

std::string CMaxnodeBlockPayees::GetMaxRequiredPaymentsString()
{
    LOCK(cs_MaxvecPayments);

    std::string maxret = "Unknown";

    BOOST_FOREACH (CMaxnodePayee& payee, vecPayments) {
        CTxDestination address1;
        ExtractDestination(payee.scriptPubKey, address1);
        CBitcoinAddress address2(address1);

        if (maxret != "Unknown") {
            maxret += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.nVotes);
        } else {
            maxret = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.nVotes);
        }
    }

    return maxret;
}

std::string CMaxnodePayments::GetMaxRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs_mapMaxnodeBlocks);

    if (mapMaxnodeBlocks.count(nBlockHeight)) {
        return mapMaxnodeBlocks[nBlockHeight].GetMaxRequiredPaymentsString();
    }

    return "Unknown";
}

bool CMaxnodePayments::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs_mapMaxnodeBlocks);

    if (mapMaxnodeBlocks.count(nBlockHeight)) {
        return mapMaxnodeBlocks[nBlockHeight].IsTransactionValid(txNew);
    }

    return true;
}

void CMaxnodePayments::CleanPaymentList()
{
    LOCK2(cs_mapMaxnodePayeeVotes, cs_mapMaxnodeBlocks);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || chainActive.Tip() == NULL) return;
        nHeight = chainActive.Tip()->nHeight;
    }

    //keep up to five cycles for historical sake
    int nLimit = std::max(int(maxnodeman.size() * 1.25), 1000);

    std::map<uint256, CMaxnodePaymentWinner>::iterator it = mapMaxnodePayeeVotes.begin();
    while (it != mapMaxnodePayeeVotes.end()) {
        CMaxnodePaymentWinner winner = (*it).second;

        if (nHeight - winner.nBlockHeight > nLimit) {
            LogPrint("maxpayments", "CMaxnodePayments::CleanPaymentList - Removing old Maxnode payment - block %d\n", winner.nBlockHeight);
            maxnodeSync.mapSeenSyncMAXW.erase((*it).first);
            mapMaxnodePayeeVotes.erase(it++);
            mapMaxnodeBlocks.erase(winner.nBlockHeight);
        } else {
            ++it;
        }
    }
}

bool CMaxnodePaymentWinner::IsValid(CNode* pnode, std::string& strError)
{
    CMaxnode* pmax = maxnodeman.Find(vinMaxnode);

    if (!pmax) {
        strError = strprintf("Unknown Maxnode %s", vinMaxnode.prevout.hash.ToString());
        LogPrint("maxnode","CMaxnodePaymentWinner::IsValid - %s\n", strError);
        maxnodeman.AskForMAX(pnode, vinMaxnode);
        return false;
    }

    if (pmax->protocolVersion < ActiveProtocol()) {
        strError = strprintf("Maxnode protocol too old %d - req %d", pmax->protocolVersion, ActiveProtocol());
        LogPrint("maxnode","CMaxnodePaymentWinner::IsValid - %s\n", strError);
        return false;
    }

    int n = maxnodeman.GetMaxnodeRank(vinMaxnode, nBlockHeight - 100, ActiveProtocol());

    if (n > MAXPAYMENTS_SIGNATURES_TOTAL) {
        //It's common to have maxnodes mistakenly think they are in the top 10
        // We don't want to print all of these messages, or punish them unless they're way off
        if (n > MAXPAYMENTS_SIGNATURES_TOTAL * 2) {
            strError = strprintf("Maxnode not in the top %d (%d)", MAXPAYMENTS_SIGNATURES_TOTAL * 2, n);
            LogPrint("maxnode","CMaxnodePaymentWinner::IsValid - %s\n", strError);
            //if (maxnodeSync.IsSynced()) Misbehaving(pnode->GetId(), 20);
        }
        return false;
    }

    return true;
}

bool CMaxnodePayments::ProcessBlock(int nBlockHeight)
{
    //if (!fMaxNodeT1 || !fMaxNodeT2 || !fMaxNodeT3) return false;
    if (!fMaxNode) return false;

    //reference node - hybrid mode

    int n = maxnodeman.GetMaxnodeRank(activeMaxnode.maxvin, nBlockHeight - 100, ActiveProtocol());

    if (n == -1) {
        LogPrint("maxpayments", "CMaxnodePayments::ProcessBlock - Unknown Maxnode\n");
        return false;
    }

    if (n > MAXPAYMENTS_SIGNATURES_TOTAL) {
        LogPrint("maxpayments", "CMaxnodePayments::ProcessBlock - Maxnode not in the top %d (%d)\n", MAXPAYMENTS_SIGNATURES_TOTAL, n);
        return false;
    }

    if (nBlockHeight <= nLastBlockHeight) return false;

    CMaxnodePaymentWinner newWinner(activeMaxnode.maxvin);

    if (maxbudget.IsBudgetPaymentBlock(nBlockHeight)) {
        //is budget payment block -- handled by the budgeting software
    } else {
        LogPrint("maxnode","CMaxnodePayments::ProcessBlock() Start nHeight %d - maxvin %s. \n", nBlockHeight, activeMaxnode.maxvin.prevout.hash.ToString());

        // pay to the oldest MAX that still had no payment but its input is old enough and it was active long enough
        int nCount = 0;
        CMaxnode* pmax = maxnodeman.GetNextMaxnodeInQueueForPayment(nBlockHeight, true, nCount);

        if (pmax != NULL) {
            LogPrint("maxnode","CMaxnodePayments::ProcessBlock() Found by FindOldestNotInVec \n");

            newWinner.nBlockHeight = nBlockHeight;

            CScript payee = GetScriptForDestination(pmax->pubKeyCollateralAddress.GetID());
            newWinner.AddPayee(payee);

            CTxDestination address1;
            ExtractDestination(payee, address1);
            CBitcoinAddress address2(address1);

            LogPrint("maxnode","CMaxnodePayments::ProcessBlock() Winner payee %s nHeight %d. \n", address2.ToString().c_str(), newWinner.nBlockHeight);
        } else {
            LogPrint("maxnode","CMaxnodePayments::ProcessBlock() Failed to find maxnode to pay\n");
        }
    }

    std::string errorMessage;
    CPubKey pubKeyMaxnode;
    CKey keyMaxnode;

    if (!obfuScationSigner.SetKey(strMaxNodePrivKey, errorMessage, keyMaxnode, pubKeyMaxnode)) {
        LogPrint("maxnode","CMaxnodePayments::ProcessBlock() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    LogPrint("maxnode","CMaxnodePayments::ProcessBlock() - Signing Winner\n");
    if (newWinner.Sign(keyMaxnode, pubKeyMaxnode)) {
        LogPrint("maxnode","CMaxnodePayments::ProcessBlock() - AddWinningMaxnode\n");

        if (AddWinningMaxnode(newWinner)) {
            newWinner.Relay();
            nLastBlockHeight = nBlockHeight;
            return true;
        }
    }

    return false;
}

void CMaxnodePaymentWinner::Relay()
{
    CInv inv(MSG_MAXNODE_WINNER, GetHash());
    RelayInv(inv);
}

bool CMaxnodePaymentWinner::SignatureValid()
{
    CMaxnode* pmax = maxnodeman.Find(vinMaxnode);

    if (pmax != NULL) {
        std::string strMessage = vinMaxnode.prevout.ToStringShort() +
                                 boost::lexical_cast<std::string>(nBlockHeight) +
                                 payee.ToString();

        std::string errorMessage = "";
        if (!obfuScationSigner.VerifyMessage(pmax->pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
            return error("CMaxnodePaymentWinner::SignatureValid() - Got bad Maxnode address signature %s\n", vinMaxnode.prevout.hash.ToString());
        }

        return true;
    }

    return false;
}

void CMaxnodePayments::Sync(CNode* node, int nCountNeeded)
{
    LOCK(cs_mapMaxnodePayeeVotes);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || chainActive.Tip() == NULL) return;
        nHeight = chainActive.Tip()->nHeight;
    }

    int nCount = (maxnodeman.CountEnabled() * 1.25);
    if (nCountNeeded > nCount) nCountNeeded = nCount;

    int nInvCount = 0;
    std::map<uint256, CMaxnodePaymentWinner>::iterator it = mapMaxnodePayeeVotes.begin();
    while (it != mapMaxnodePayeeVotes.end()) {
        CMaxnodePaymentWinner winner = (*it).second;
        if (winner.nBlockHeight >= nHeight - nCountNeeded && winner.nBlockHeight <= nHeight + 20) {
            node->PushInventory(CInv(MSG_MAXNODE_WINNER, winner.GetHash()));
            nInvCount++;
        }
        ++it;
    }
    node->PushMessage("smaxsc", MAXNODE_SYNC_MAXW, nInvCount);
}

std::string CMaxnodePayments::ToString() const
{
    std::ostringstream info;

    info << "Votes: " << (int)mapMaxnodePayeeVotes.size() << ", Blocks: " << (int)mapMaxnodeBlocks.size();

    return info.str();
}


int CMaxnodePayments::GetOldestBlock()
{
    LOCK(cs_mapMaxnodeBlocks);

    int nOldestBlock = std::numeric_limits<int>::max();

    std::map<int, CMaxnodeBlockPayees>::iterator it = mapMaxnodeBlocks.begin();
    while (it != mapMaxnodeBlocks.end()) {
        if ((*it).first < nOldestBlock) {
            nOldestBlock = (*it).first;
        }
        it++;
    }

    return nOldestBlock;
}


int CMaxnodePayments::GetNewestBlock()
{
    LOCK(cs_mapMaxnodeBlocks);

    int nNewestBlock = 0;

    std::map<int, CMaxnodeBlockPayees>::iterator it = mapMaxnodeBlocks.begin();
    while (it != mapMaxnodeBlocks.end()) {
        if ((*it).first > nNewestBlock) {
            nNewestBlock = (*it).first;
        }
        it++;
    }

    return nNewestBlock;
}
