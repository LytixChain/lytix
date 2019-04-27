// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAXNODEMAN_H
#define MAXNODEMAN_H

#include "base58.h"
#include "key.h"
#include "main.h"
#include "maxnode.h"
#include "net.h"
#include "sync.h"
#include "util.h"

#define MAXNODES_DUMP_SECONDS (15 * 60)
#define MAXNODES_DSEG_SECONDS (3 * 60 * 60)

using namespace std;

class CMaxnodeMan;

extern CMaxnodeMan maxnodeman;
void DumpMaxnodes();

/** Access to the Max database (maxcache.dat)
 */
class CMaxnodeDB
{
private:
    boost::filesystem::path pathMAX;
    std::string strMagicMessage;

public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CMaxnodeDB();
    bool Write(const CMaxnodeMan& maxnodemanToSave);
    ReadResult Read(CMaxnodeMan& maxnodemanToLoad, bool fDryRun = false);
};

class CMaxnodeMan
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // critical section to protect the inner data structures specifically on messaging
    mutable CCriticalSection cs_process_message;

    // map to hold all MAXs
    std::vector<CMaxnode> vMaxnodes;
    // who's asked for the Maxnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForMaxnodeList;
    // who we asked for the Maxnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForMaxnodeList;
    // which Maxnodes we've asked for
    std::map<COutPoint, int64_t> mWeAskedForMaxnodeListEntry;

public:
    // Keep track of all broadcasts I've seen
    map<uint256, CMaxnodeBroadcast> mapSeenMaxnodeBroadcast;
    // Keep track of all pings I've seen
    map<uint256, CMaxnodePing> mapSeenMaxnodePing;

    // keep track of dsq count to prevent maxnodes from gaming obfuscation queue
    int64_t nDsqCount;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        LOCK(cs);
        READWRITE(vMaxnodes);
        READWRITE(mAskedUsForMaxnodeList);
        READWRITE(mWeAskedForMaxnodeList);
        READWRITE(mWeAskedForMaxnodeListEntry);
        READWRITE(nDsqCount);

        READWRITE(mapSeenMaxnodeBroadcast);
        READWRITE(mapSeenMaxnodePing);
    }

    CMaxnodeMan();
    CMaxnodeMan(CMaxnodeMan& other);

    /// Add an entry
    bool Add(CMaxnode& max);

    /// Ask (source) node for maxb
    void AskForMAX(CNode* pnode, CTxIn& vin);

    /// Check all Maxnodes
    void Check();

    /// Check all Maxnodes and remove inactive
    void CheckAndRemove(bool forceExpiredRemoval = false);

    /// Clear Maxnode vector
    void Clear();

    int CountEnabled(int protocolVersion = -1);

    void CountNetworks(int protocolVersion, int& ipv4, int& ipv6, int& onion);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CMaxnode* Find(const CScript& payee);
    CMaxnode* Find(const CTxIn& vin);
    CMaxnode* Find(const CPubKey& pubKeyMaxnode);

    /// Find an entry in the maxnode list that is next to be paid
    CMaxnode* GetNextMaxnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CMaxnode* FindRandomNotInVec(std::vector<CTxIn>& vecToExclude, int protocolVersion = -1);

    /// Get the current winner for this block
    CMaxnode* GetCurrentMaxNode(int mod = 1, int64_t nBlockHeight = 0, int minProtocol = 0);

    std::vector<CMaxnode> GetFullMaxnodeVector()
    {
        Check();
        return vMaxnodes;
    }

    std::vector<pair<int, CMaxnode> > GetMaxnodeRanks(int64_t nBlockHeight, int minProtocol = 0);
    int GetMaxnodeRank(const CTxIn& vin, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);
    CMaxnode* GetMaxnodeByRank(int nRank, int64_t nBlockHeight, int minProtocol = 0, bool fOnlyActive = true);

    void ProcessMaxnodeConnections();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    /// Return the number of (unique) Maxnodes
    int size() { return vMaxnodes.size(); }

    /// Return the number of Maxnodes older than (default) 8000 seconds
    int stable_size ();

    std::string ToString() const;

    void Remove(CTxIn vin);

    int GetEstimatedMaxnodes(int nBlock);

    /// Update maxnode list and maps using provided CMaxnodeBroadcast
    void UpdateMaxnodeList(CMaxnodeBroadcast maxb);
};

#endif
