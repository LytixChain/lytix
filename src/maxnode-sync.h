// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAXNODE_SYNC_H
#define MAXNODE_SYNC_H

#define MAXNODE_SYNC_INITIAL 0
#define MAXNODE_SYNC_SPORKS 1
#define MAXNODE_SYNC_LIST 2
#define MAXNODE_SYNC_MAXW 3
#define MAXNODE_SYNC_BUDGET 4
#define MAXNODE_SYNC_BUDGET_PROP 10
#define MAXNODE_SYNC_BUDGET_FIN 11
#define MAXNODE_SYNC_FAILED 998
#define MAXNODE_SYNC_FINISHED 999

#define MAXNODE_SYNC_TIMEOUT 5
#define MAXNODE_SYNC_THRESHOLD 2

class CMaxnodeSync;
extern CMaxnodeSync maxnodeSync;

//
// CMaxnodeSync : Sync maxnode assets in stages
//

class CMaxnodeSync
{
public:
    std::map<uint256, int> mapSeenSyncMAXB;
    std::map<uint256, int> mapSeenSyncMAXW;
    std::map<uint256, int> mapSeenSyncBudget;

    int64_t lastMaxnodeList;
    int64_t lastMaxnodeWinner;
    int64_t lastFailure;
    int nCountFailures;

    // sum of all counts
    int sumMaxnodeList;
    int sumMaxnodeWinner;
    // peers that reported counts
    int countMaxnodeList;
    int countMaxnodeWinner;

    // Count peers we've requested the list from
    int RequestedMaxnodeAssets;
    int RequestedMaxnodeAttempt;

    // Time when current maxnode asset sync started
    int64_t nAssetSyncStarted;

    CMaxnodeSync();

    void AddedMaxnodeList(uint256 hash);
    void AddedMaxnodeWinner(uint256 hash);
    void GetNextAsset();
    std::string GetSyncStatus();
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void Reset();
    void Process();
    bool IsSynced();
    bool IsBlockchainSynced();
    bool IsMaxnodeListSynced() { return RequestedMaxnodeAssets > MAXNODE_SYNC_LIST; }
    void ClearFulfilledRequest();
};

#endif
