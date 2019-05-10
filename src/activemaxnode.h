// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVEMAXNODE_H
#define ACTIVEMAXNODE_H

#include "init.h"
#include "key.h"
#include "maxnode.h"
#include "net.h"
#include "obfuscation.h"
#include "sync.h"
#include "wallet.h"

#define ACTIVE_MAXNODE_INITIAL 0 // initial state
#define ACTIVE_MAXNODE_SYNC_IN_PROCESS 1
#define ACTIVE_MAXNODE_INPUT_TOO_NEW 2
#define ACTIVE_MAXNODE_NOT_CAPABLE 3
#define ACTIVE_MAXNODE_STARTED 4

// Responsible for activating the Maxnode and pinging the network
class CActiveMaxnode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    /// Ping Maxnode
    bool SendMaxnodePing(std::string& errorMessage);

    /// Create Maxnode broadcast, needs to be relayed manually after that
    bool CreateBroadcast(CTxIn maxvin, CService service, CKey key, CPubKey pubKey, CKey keyMaxnode, CPubKey pubKeyMaxnode, std::string& errorMessage, CMaxnodeBroadcast &maxb);

    /// Get 50000 LYTX input that can be used for the Maxnode
    bool GetMaxNodeVin(CTxIn& maxvin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex);
    bool GetVinFromOutput(COutput out, CTxIn& maxvin, CPubKey& pubkey, CKey& secretKey);

public:
    // Initialized by init.cpp
    // Keys for the main Maxnode
    CPubKey pubKeyMaxnode;

    // Initialized while registering Maxnode
    CTxIn maxvin;
    CService service;

    int status;
    std::string notCapableReason;

    CActiveMaxnode()
    {
        status = ACTIVE_MAXNODE_INITIAL;
    }

    /// Manage status of main Maxnode
    void ManageStatus();
    std::string GetStatus();

    /// Create Maxnode broadcast, needs to be relayed manually after that
    bool CreateBroadcast(std::string strService, std::string strKey, std::string strTxHash, std::string strOutputIndex, std::string& errorMessage, CMaxnodeBroadcast &maxb, bool fOffline = false);

    /// Get 50000 LYTX input that can be used for the Maxnode
    bool GetMaxNodeVin(CTxIn& maxvin, CPubKey& pubkey, CKey& secretKey);
    vector<COutput> SelectCoinsMaxnode();

    /// Enable cold wallet mode (run a Maxnode with no funds)
    bool EnableHotColdMaxNode(CTxIn& maxvin, CService& addr);
};

#endif
