// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// clang-format off
#include "main.h"
#include "activemaxnode.h"
#include "maxnode-sync.h"
#include "maxnode-payments.h"
//#include "maxnode-budget.h"
#include "maxnode.h"
#include "maxnodeman.h"
#include "spork.h"
#include "util.h"
#include "addrman.h"
// clang-format on

class CMaxnodeSync;
CMaxnodeSync maxnodeSync;

CMaxnodeSync::CMaxnodeSync()
{
    Reset();
}

bool CMaxnodeSync::IsSynced()
{
    return RequestedMaxnodeAssets == MAXNODE_SYNC_FINISHED;
}

bool CMaxnodeSync::IsBlockchainSynced()
{
    static bool fBlockchainSynced = false;
    static int64_t lastProcess = GetTime();

    // if the last call to this function was more than 60 minutes ago (client was in sleep mode) reset the sync process
    if (GetTime() - lastProcess > 60 * 60) {
        Reset();
        fBlockchainSynced = false;
    }
    lastProcess = GetTime();

    if (fBlockchainSynced) return true;

    if (fImporting || fReindex) return false;

    TRY_LOCK(cs_main, lockMain);
    if (!lockMain) return false;

    CBlockIndex* pindex = chainActive.Tip();
    if (pindex == NULL) return false;


    if (pindex->nTime + 60 * 60 < GetTime())
        return false;

    fBlockchainSynced = true;

    return true;
}

void CMaxnodeSync::Reset()
{
    lastMaxnodeList = 0;
    lastMaxnodeWinner = 0;
    mapSeenSyncMAXB.clear();
    mapSeenSyncMAXW.clear();
    mapSeenSyncBudget.clear();
    lastFailure = 0;
    nCountFailures = 0;
    sumMaxnodeList = 0;
    sumMaxnodeWinner = 0;
    countMaxnodeList = 0;
    countMaxnodeWinner = 0;
    RequestedMaxnodeAssets = MAXNODE_SYNC_INITIAL;
    RequestedMaxnodeAttempt = 0;
    nAssetSyncStarted = GetTime();
}

void CMaxnodeSync::AddedMaxnodeList(uint256 hash)
{
    if (maxnodeman.mapSeenMaxnodeBroadcast.count(hash)) {
        if (mapSeenSyncMAXB[hash] < MAXNODE_SYNC_THRESHOLD) {
            lastMaxnodeList = GetTime();
            mapSeenSyncMAXB[hash]++;
        }
    } else {
        lastMaxnodeList = GetTime();
        mapSeenSyncMAXB.insert(make_pair(hash, 1));
    }
}

void CMaxnodeSync::AddedMaxnodeWinner(uint256 hash)
{
    if (maxnodePayments.mapMaxnodePayeeVotes.count(hash)) {
        if (mapSeenSyncMAXW[hash] < MAXNODE_SYNC_THRESHOLD) {
            lastMaxnodeWinner = GetTime();
            mapSeenSyncMAXW[hash]++;
        }
    } else {
        lastMaxnodeWinner = GetTime();
        mapSeenSyncMAXW.insert(make_pair(hash, 1));
    }
}

void CMaxnodeSync::GetNextAsset()
{
    switch (RequestedMaxnodeAssets) {
    case (MAXNODE_SYNC_INITIAL):
    case (MAXNODE_SYNC_FAILED): // should never be used here actually, use Reset() instead
        ClearFulfilledRequest();
        RequestedMaxnodeAssets = MAXNODE_SYNC_SPORKS;
        break;
    case (MAXNODE_SYNC_SPORKS):
        RequestedMaxnodeAssets = MAXNODE_SYNC_LIST;
        break;
    case (MAXNODE_SYNC_LIST):
        RequestedMaxnodeAssets = MAXNODE_SYNC_MAXW;
        break;
    case (MAXNODE_SYNC_MAXW):
        RequestedMaxnodeAssets = MAXNODE_SYNC_BUDGET;
        break;
    }
    RequestedMaxnodeAttempt = 0;
    nAssetSyncStarted = GetTime();
}

std::string CMaxnodeSync::GetSyncStatus()
{
    switch (maxnodeSync.RequestedMaxnodeAssets) {
    case MAXNODE_SYNC_INITIAL:
        return _("Synchronization pending...");
    case MAXNODE_SYNC_SPORKS:
        return _("Synchronizing sporks...");
    case MAXNODE_SYNC_LIST:
        return _("Synchronizing maxnodes...");
    case MAXNODE_SYNC_MAXW:
        return _("Synchronizing maxnode winners...");
    case MAXNODE_SYNC_FAILED:
        return _("Synchronization failed");
    case MAXNODE_SYNC_FINISHED:
        return _("Synchronization finished");
    }
    return "";
}

void CMaxnodeSync::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (strCommand == "smaxsc") { //Sync status count
        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        if (RequestedMaxnodeAssets >= MAXNODE_SYNC_FINISHED) return;

        //this means we will receive no further communication
        switch (nItemID) {
        case (MAXNODE_SYNC_LIST):
            if (nItemID != RequestedMaxnodeAssets) return;
            sumMaxnodeList += nCount;
            countMaxnodeList++;
            break;
        case (MAXNODE_SYNC_MAXW):
            if (nItemID != RequestedMaxnodeAssets) return;
            sumMaxnodeWinner += nCount;
            countMaxnodeWinner++;
            break;
        }

        LogPrint("maxnode", "CMaxnodeSync:ProcessMessage - smaxsc - got inventory count %d %d\n", nItemID, nCount);
    }
}

void CMaxnodeSync::ClearFulfilledRequest()
{
    TRY_LOCK(cs_vNodes, lockRecv);
    if (!lockRecv) return;

    BOOST_FOREACH (CNode* pnode, vNodes) {
        pnode->ClearFulfilledRequest("getspork");
        pnode->ClearFulfilledRequest("maxsync");
        pnode->ClearFulfilledRequest("maxwsync");
        pnode->ClearFulfilledRequest("busync");
    }
}

void CMaxnodeSync::Process()
{
    static int tick = 0;

    if (tick++ % MAXNODE_SYNC_TIMEOUT != 0) return;

    if (IsSynced()) {
        /* 
            Resync if we lose all maxnodes from sleep/wake or failure to sync originally
        */
        if (maxnodeman.CountEnabled() == 0) {
            Reset();
        } else
            return;
    }

    //try syncing again
    if (RequestedMaxnodeAssets == MAXNODE_SYNC_FAILED && lastFailure + (1 * 60) < GetTime()) {
        Reset();
    } else if (RequestedMaxnodeAssets == MAXNODE_SYNC_FAILED) {
        return;
    }

    LogPrint("maxnode", "CMaxnodeSync::Process() - tick %d RequestedMaxnodeAssets %d\n", tick, RequestedMaxnodeAssets);

    if (RequestedMaxnodeAssets == MAXNODE_SYNC_INITIAL) GetNextAsset();

    // sporks synced but blockchain is not, wait until we're almost at a recent block to continue
    if (Params().NetworkID() != CBaseChainParams::REGTEST &&
        !IsBlockchainSynced() && RequestedMaxnodeAssets > MAXNODE_SYNC_SPORKS) return;

    TRY_LOCK(cs_vNodes, lockRecv);
    if (!lockRecv) return;

    BOOST_FOREACH (CNode* pnode, vNodes) {
        if (Params().NetworkID() == CBaseChainParams::REGTEST) {
            if (RequestedMaxnodeAttempt <= 2) {
                pnode->PushMessage("getsporks"); //get current network sporks
            } else if (RequestedMaxnodeAttempt < 4) {
                maxnodeman.DsegUpdate(pnode);
            } else if (RequestedMaxnodeAttempt < 6) {
                int nMnCount = maxnodeman.CountEnabled();
                pnode->PushMessage("maxget", nMnCount); //sync payees
                uint256 n = 0;
                pnode->PushMessage("maxvs", n); //sync maxnode votes
            } else {
                RequestedMaxnodeAssets = MAXNODE_SYNC_FINISHED;
            }
            RequestedMaxnodeAttempt++;
            return;
        }

        //set to synced
        if (RequestedMaxnodeAssets == MAXNODE_SYNC_SPORKS) {
            if (pnode->HasFulfilledRequest("getspork")) continue;
            pnode->FulfilledRequest("getspork");

            pnode->PushMessage("getsporks"); //get current network sporks
            if (RequestedMaxnodeAttempt >= 2) GetNextAsset();
            RequestedMaxnodeAttempt++;

            return;
        }

        if (pnode->nVersion >= maxnodePayments.GetMinMaxnodePaymentsProto()) {
            if (RequestedMaxnodeAssets == MAXNODE_SYNC_LIST) {
                LogPrint("maxnode", "CMaxnodeSync::Process() - lastMaxnodeList %lld (GetTime() - MAXNODE_SYNC_TIMEOUT) %lld\n", lastMaxnodeList, GetTime() - MAXNODE_SYNC_TIMEOUT);
                if (lastMaxnodeList > 0 && lastMaxnodeList < GetTime() - MAXNODE_SYNC_TIMEOUT * 2 && RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD) { //hasn't received a new item in the last five seconds, so we'll move to the
                    GetNextAsset();
                    return;
                }

                if (pnode->HasFulfilledRequest("maxsync")) continue;
                pnode->FulfilledRequest("maxsync");

                // timeout
                if (lastMaxnodeList == 0 &&
                    (RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD * 3 || GetTime() - nAssetSyncStarted > MAXNODE_SYNC_TIMEOUT * 5)) {
                    if (IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT)) {
                        LogPrintf("CMaxnodeSync::Process - ERROR - Sync has failed, will retry later\n");
                        RequestedMaxnodeAssets = MAXNODE_SYNC_FAILED;
                        RequestedMaxnodeAttempt = 0;
                        lastFailure = GetTime();
                        nCountFailures++;
                    } else {
                        GetNextAsset();
                    }
                    return;
                }

                if (RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD * 3) return;

                maxnodeman.DsegUpdate(pnode);
                RequestedMaxnodeAttempt++;
                return;
            }

            if (RequestedMaxnodeAssets == MAXNODE_SYNC_MAXW) {
                if (lastMaxnodeWinner > 0 && lastMaxnodeWinner < GetTime() - MAXNODE_SYNC_TIMEOUT * 2 && RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD) { //hasn't received a new item in the last five seconds, so we'll move to the
                    GetNextAsset();
                    return;
                }

                if (pnode->HasFulfilledRequest("maxwsync")) continue;
                pnode->FulfilledRequest("maxwsync");

                // timeout
                if (lastMaxnodeWinner == 0 &&
                    (RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD * 3 || GetTime() - nAssetSyncStarted > MAXNODE_SYNC_TIMEOUT * 5)) {
                    if (IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT)) {
                        LogPrintf("CMaxnodeSync::Process - ERROR - Sync has failed, will retry later\n");
                        RequestedMaxnodeAssets = MAXNODE_SYNC_FAILED;
                        RequestedMaxnodeAttempt = 0;
                        lastFailure = GetTime();
                        nCountFailures++;
                    } else {
                        GetNextAsset();
                    }
                    return;
                }

                if (RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD * 3) return;

                CBlockIndex* pindexPrev = chainActive.Tip();
                if (pindexPrev == NULL) return;

                int nMnCount = maxnodeman.CountEnabled();
                pnode->PushMessage("maxget", nMnCount); //sync payees
                RequestedMaxnodeAttempt++;

                return;
            }
        }

        if (pnode->nVersion >= ActiveProtocol()) {
            if (RequestedMaxnodeAssets == MAXNODE_SYNC_BUDGET) {
                
                // We'll start rejecting votes if we accidentally get set as synced too soon
                /**if (lastBudgetItem > 0 && lastBudgetItem < GetTime() - MAXNODE_SYNC_TIMEOUT * 2 && RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD) { 
                    
                    // Hasn't received a new item in the last five seconds, so we'll move to the
                    GetNextAsset();

                    // Try to activate our maxnode if possible
                    activeMaxnode.ManageStatus();

                    return;
                }

                // timeout
                if (lastBudgetItem == 0 &&
                    (RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD * 3 || GetTime() - nAssetSyncStarted > MAXNODE_SYNC_TIMEOUT * 5)) {
                    // maybe there is no budgets at all, so just finish syncing
                    GetNextAsset();
                    activeMaxnode.ManageStatus();
                    return;
                }**/

                if (pnode->HasFulfilledRequest("busync")) continue;
                pnode->FulfilledRequest("busync");

                if (RequestedMaxnodeAttempt >= MAXNODE_SYNC_THRESHOLD * 3) return;

                uint256 n = 0;
                pnode->PushMessage("maxvs", n); //sync maxnode votes
                RequestedMaxnodeAttempt++;

                return;
            }
        }
    }
}
