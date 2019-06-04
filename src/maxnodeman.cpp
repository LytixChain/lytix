// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "maxnodeman.h"
#include "activemaxnode.h"
#include "addrman.h"
#include "maxnode.h"
#include "obfuscation.h"
#include "spork.h"
#include "util.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#define MAX_WINNER_MINIMUM_AGE 8000    // Age in seconds. This should be > MAXNODE_REMOVAL_SECONDS to avoid misconfigured new nodes in the list.

/** Maxnode manager */
CMaxnodeMan maxnodeman;

struct CompareLastPaid {
    bool operator()(const pair<int64_t, CTxIn>& t1,
        const pair<int64_t, CTxIn>& t2) const
    {
        return t1.first < t2.first;
    }
};

struct CompareScoreTxIn {
    bool operator()(const pair<int64_t, CTxIn>& t1,
        const pair<int64_t, CTxIn>& t2) const
    {
        return t1.first < t2.first;
    }
};

struct CompareScoreMAX {
    bool operator()(const pair<int64_t, CMaxnode>& t1,
        const pair<int64_t, CMaxnode>& t2) const
    {
        return t1.first < t2.first;
    }
};

//
// CMaxnodeDB
//

CMaxnodeDB::CMaxnodeDB()
{
    pathMAX = GetDataDir() / "maxcache.dat";
    strMagicMessage = "MaxnodeCache";
}

bool CMaxnodeDB::Write(const CMaxnodeMan& maxnodemanToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssMaxnodes(SER_DISK, CLIENT_VERSION);
    ssMaxnodes << strMagicMessage;                   // maxnode cache file specific magic message
    ssMaxnodes << FLATDATA(Params().MessageStart()); // network specific magic number
    ssMaxnodes << maxnodemanToSave;
    uint256 hash = Hash(ssMaxnodes.begin(), ssMaxnodes.end());
    ssMaxnodes << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fopen(pathMAX.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathMAX.string());

    // Write and commit header, data
    try {
        fileout << ssMaxnodes;
    } catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    //    FileCommit(fileout);
    fileout.fclose();

    LogPrint("maxnode","Written info to maxcache.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint("maxnode","  %s\n", maxnodemanToSave.ToString());

    return true;
}

CMaxnodeDB::ReadResult CMaxnodeDB::Read(CMaxnodeMan& maxnodemanToLoad, bool fDryRun)
{
    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fopen(pathMAX.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathMAX.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathMAX);
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

    CDataStream ssMaxnodes(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssMaxnodes.begin(), ssMaxnodes.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }

    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (maxnode cache file specific magic message) and ..

        ssMaxnodes >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid maxnode cache magic message", __func__);
            return IncorrectMagicMessage;
        }

        // de-serialize file header (network specific magic number) and ..
        ssMaxnodes >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }
        // de-serialize data into CMaxnodeMan object
        ssMaxnodes >> maxnodemanToLoad;
    } catch (std::exception& e) {
        maxnodemanToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint("maxnode","Loaded info from maxcache.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint("maxnode","  %s\n", maxnodemanToLoad.ToString());
    if (!fDryRun) {
        LogPrint("maxnode","Maxnode manager - cleaning....\n");
        maxnodemanToLoad.CheckAndRemove(true);
        LogPrint("maxnode","Maxnode manager - result:\n");
        LogPrint("maxnode","  %s\n", maxnodemanToLoad.ToString());
    }

    return Ok;
}

void DumpMaxnodes()
{
    int64_t nStart = GetTimeMillis();

    CMaxnodeDB maxdb;
    CMaxnodeMan tempMnodeman;

    LogPrint("maxnode","Verifying maxcache.dat format...\n");
    CMaxnodeDB::ReadResult readResult = maxdb.Read(tempMnodeman, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CMaxnodeDB::FileError)
        LogPrint("maxnode","Missing maxnode cache file - maxcache.dat, will try to recreate\n");
    else if (readResult != CMaxnodeDB::Ok) {
        LogPrint("maxnode","Error reading maxcache.dat: ");
        if (readResult == CMaxnodeDB::IncorrectFormat)
            LogPrint("maxnode","magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrint("maxnode","file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrint("maxnode","Writting info to maxcache.dat...\n");
    maxdb.Write(maxnodeman);

    LogPrint("maxnode","Maxnode dump finished  %dms\n", GetTimeMillis() - nStart);
}

CMaxnodeMan::CMaxnodeMan()
{
    nDsqCount = 0;
}

bool CMaxnodeMan::Add(CMaxnode& max)
{
    LOCK(cs);

    if (!max.IsEnabled())
        return false;

    CMaxnode* pmax = Find(max.maxvin);
    if (pmax == NULL) {
        LogPrint("maxnode", "CMaxnodeMan: Adding new Maxnode %s - %i now\n", max.maxvin.prevout.hash.ToString(), size() + 1);
        vMaxnodes.push_back(max);
        return true;
    }

    return false;
}

void CMaxnodeMan::AskForMAX(CNode* pnode, CTxIn& maxvin)
{
    std::map<COutPoint, int64_t>::iterator i = mWeAskedForMaxnodeListEntry.find(maxvin.prevout);
    if (i != mWeAskedForMaxnodeListEntry.end()) {
        int64_t t = (*i).second;
        if (GetTime() < t) return; // we've asked recently
    }

    // ask for the maxb info once from the node that sent maxp

    LogPrint("maxnode", "CMaxnodeMan::AskForMAX - Asking node for missing entry, maxvin: %s\n", maxvin.prevout.hash.ToString());
    pnode->PushMessage("dmaxseg", maxvin);
    int64_t askAgain = GetTime() + MAXNODE_MIN_MAXP_SECONDS;
    mWeAskedForMaxnodeListEntry[maxvin.prevout] = askAgain;
}

void CMaxnodeMan::Check()
{
    LOCK(cs);

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        max.Check();
    }
}

void CMaxnodeMan::CheckAndRemove(bool forceExpiredRemoval)
{
    Check();

    LOCK(cs);

    //remove inactive and outdated
    vector<CMaxnode>::iterator it = vMaxnodes.begin();
    while (it != vMaxnodes.end()) {
        if ((*it).activeState == CMaxnode::MAXNODE_REMOVE ||
            (*it).activeState == CMaxnode::MAXNODE_VIN_SPENT ||
            (forceExpiredRemoval && (*it).activeState == CMaxnode::MAXNODE_EXPIRED) ||
            (*it).protocolVersion < maxnodePayments.GetMinMaxnodePaymentsProto()) {
            LogPrint("maxnode", "CMaxnodeMan: Removing inactive Maxnode %s - %i now\n", (*it).maxvin.prevout.hash.ToString(), size() - 1);

            //erase all of the broadcasts we've seen from this maxvin
            // -- if we missed a few pings and the node was removed, this will allow is to get it back without them
            //    sending a brand new maxb
            map<uint256, CMaxnodeBroadcast>::iterator it3 = mapSeenMaxnodeBroadcast.begin();
            while (it3 != mapSeenMaxnodeBroadcast.end()) {
                if ((*it3).second.maxvin == (*it).maxvin) {
                    maxnodeSync.mapSeenSyncMAXB.erase((*it3).first);
                    mapSeenMaxnodeBroadcast.erase(it3++);
                } else {
                    ++it3;
                }
            }

            // allow us to ask for this maxnode again if we see another ping
            map<COutPoint, int64_t>::iterator it2 = mWeAskedForMaxnodeListEntry.begin();
            while (it2 != mWeAskedForMaxnodeListEntry.end()) {
                if ((*it2).first == (*it).maxvin.prevout) {
                    mWeAskedForMaxnodeListEntry.erase(it2++);
                } else {
                    ++it2;
                }
            }

            it = vMaxnodes.erase(it);
        } else {
            ++it;
        }
    }

    // check who's asked for the Maxnode list
    map<CNetAddr, int64_t>::iterator it1 = mAskedUsForMaxnodeList.begin();
    while (it1 != mAskedUsForMaxnodeList.end()) {
        if ((*it1).second < GetTime()) {
            mAskedUsForMaxnodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check who we asked for the Maxnode list
    it1 = mWeAskedForMaxnodeList.begin();
    while (it1 != mWeAskedForMaxnodeList.end()) {
        if ((*it1).second < GetTime()) {
            mWeAskedForMaxnodeList.erase(it1++);
        } else {
            ++it1;
        }
    }

    // check which Maxnodes we've asked for
    map<COutPoint, int64_t>::iterator it2 = mWeAskedForMaxnodeListEntry.begin();
    while (it2 != mWeAskedForMaxnodeListEntry.end()) {
        if ((*it2).second < GetTime()) {
            mWeAskedForMaxnodeListEntry.erase(it2++);
        } else {
            ++it2;
        }
    }

    // remove expired mapSeenMaxnodeBroadcast
    map<uint256, CMaxnodeBroadcast>::iterator it3 = mapSeenMaxnodeBroadcast.begin();
    while (it3 != mapSeenMaxnodeBroadcast.end()) {
        if ((*it3).second.lastPing.sigTime < GetTime() - (MAXNODE_REMOVAL_SECONDS * 2)) {
            mapSeenMaxnodeBroadcast.erase(it3++);
            maxnodeSync.mapSeenSyncMAXB.erase((*it3).second.GetHash());
        } else {
            ++it3;
        }
    }

    // remove expired mapSeenMaxnodePing
    map<uint256, CMaxnodePing>::iterator it4 = mapSeenMaxnodePing.begin();
    while (it4 != mapSeenMaxnodePing.end()) {
        if ((*it4).second.sigTime < GetTime() - (MAXNODE_REMOVAL_SECONDS * 2)) {
            mapSeenMaxnodePing.erase(it4++);
        } else {
            ++it4;
        }
    }
}

void CMaxnodeMan::Clear()
{
    LOCK(cs);
    vMaxnodes.clear();
    mAskedUsForMaxnodeList.clear();
    mWeAskedForMaxnodeList.clear();
    mWeAskedForMaxnodeListEntry.clear();
    mapSeenMaxnodeBroadcast.clear();
    mapSeenMaxnodePing.clear();
    nDsqCount = 0;
}

int CMaxnodeMan::stable_size ()
{
    int nStable_size = 0;
    int nMinProtocol = ActiveProtocol();
    int64_t nMaxnode_Min_Age = MAX_WINNER_MINIMUM_AGE;
    int64_t nMaxnode_Age = 0;

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        if (max.protocolVersion < nMinProtocol) {
            continue; // Skip obsolete versions
        }
        if (IsSporkActive (SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT)) {
            nMaxnode_Age = GetAdjustedTime() - max.sigTime;
            if ((nMaxnode_Age) < nMaxnode_Min_Age) {
                continue; // Skip maxnodes younger than (default) 8000 sec (MUST be > MAXNODE_REMOVAL_SECONDS)
            }
        }
        max.Check ();
        if (!max.IsEnabled ())
            continue; // Skip not-enabled maxnodes

        nStable_size++;
    }

    return nStable_size;
}

int CMaxnodeMan::CountEnabled(int protocolVersion)
{
    int i = 0;
    protocolVersion = protocolVersion == -1 ? maxnodePayments.GetMinMaxnodePaymentsProto() : protocolVersion;

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        max.Check();
        if (max.protocolVersion < protocolVersion || !max.IsEnabled()) continue;
        i++;
    }

    return i;
}

void CMaxnodeMan::CountNetworks(int protocolVersion, int& ipv4, int& ipv6, int& onion)
{
    protocolVersion = protocolVersion == -1 ? maxnodePayments.GetMinMaxnodePaymentsProto() : protocolVersion;

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        max.Check();
        std::string strHost;
        int port;
        SplitHostPort(max.addr.ToString(), port, strHost);
        CNetAddr node = CNetAddr(strHost, false);
        int nNetwork = node.GetNetwork();
        switch (nNetwork) {
            case 1 :
                ipv4++;
                break;
            case 2 :
                ipv6++;
                break;
            case 3 :
                onion++;
                break;
        }
    }
}

void CMaxnodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        if (!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForMaxnodeList.find(pnode->addr);
            if (it != mWeAskedForMaxnodeList.end()) {
                if (GetTime() < (*it).second) {
                    LogPrint("maxnode", "dmaxseg - we already asked peer %i for the list; skipping...\n", pnode->GetId());
                    return;
                }
            }
        }
    }

    pnode->PushMessage("dmaxseg", CTxIn());
    int64_t askAgain = GetTime() + MAXNODES_DSEG_SECONDS;
    mWeAskedForMaxnodeList[pnode->addr] = askAgain;
}

CMaxnode* CMaxnodeMan::Find(const CScript& payee)
{
    LOCK(cs);
    CScript payee2;

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        payee2 = GetScriptForDestination(max.pubKeyCollateralAddress.GetID());
        if (payee2 == payee)
            return &max;
    }
    return NULL;
}

CMaxnode* CMaxnodeMan::Find(const CTxIn& maxvin)
{
    LOCK(cs);

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        if (max.maxvin.prevout == maxvin.prevout)
            return &max;
    }
    return NULL;
}


CMaxnode* CMaxnodeMan::Find(const CPubKey& pubKeyMaxnode)
{
    LOCK(cs);

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        if (max.pubKeyMaxnode == pubKeyMaxnode)
            return &max;
    }
    return NULL;
}

//
// Deterministically select the oldest/best maxnode to pay on the network
//
CMaxnode* CMaxnodeMan::GetNextMaxnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    LOCK(cs);

    CMaxnode* pBestMaxnode = NULL;
    std::vector<pair<int64_t, CTxIn> > vecMaxnodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */

    int nMnCount = CountEnabled();
    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        max.Check();
        if (!max.IsEnabled()) continue;

        // //check protocol version
        if (max.protocolVersion < maxnodePayments.GetMinMaxnodePaymentsProto()) continue;

        //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
        if (maxnodePayments.IsScheduled(max, nBlockHeight)) continue;

        //it's too new, wait for a cycle
        if (fFilterSigTime && max.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) continue;

        //make sure it has as many confirmations as there are maxnodes
        if (max.GetMaxnodeInputAge() < nMnCount) continue;

        vecMaxnodeLastPaid.push_back(make_pair(max.SecondsSincePayment(), max.maxvin));
    }

    nCount = (int)vecMaxnodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if (fFilterSigTime && nCount < nMnCount / 3) return GetNextMaxnodeInQueueForPayment(nBlockHeight, false, nCount);

    // Sort them high to low
    sort(vecMaxnodeLastPaid.rbegin(), vecMaxnodeLastPaid.rend(), CompareLastPaid());

    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = CountEnabled() / 10;
    int nCountTenth = 0;
    uint256 nHigh = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CTxIn) & s, vecMaxnodeLastPaid) {
        CMaxnode* pmax = Find(s.second);
        if (!pmax) break;

        uint256 n = pmax->CalculateScore(1, nBlockHeight - 100);
        if (n > nHigh) {
            nHigh = n;
            pBestMaxnode = pmax;
        }
        nCountTenth++;
        if (nCountTenth >= nTenthNetwork) break;
    }
    return pBestMaxnode;
}

CMaxnode* CMaxnodeMan::FindRandomNotInVec(std::vector<CTxIn>& vecToExclude, int protocolVersion)
{
    LOCK(cs);

    protocolVersion = protocolVersion == -1 ? maxnodePayments.GetMinMaxnodePaymentsProto() : protocolVersion;

    int nCountEnabled = CountEnabled(protocolVersion);
    LogPrint("maxnode", "CMaxnodeMan::FindRandomNotInVec - nCountEnabled - vecToExclude.size() %d\n", nCountEnabled - vecToExclude.size());
    if (nCountEnabled - vecToExclude.size() < 1) return NULL;

    int rand = GetRandInt(nCountEnabled - vecToExclude.size());
    LogPrint("maxnode", "CMaxnodeMan::FindRandomNotInVec - rand %d\n", rand);
    bool found;

    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        if (max.protocolVersion < protocolVersion || !max.IsEnabled()) continue;
        found = false;
        BOOST_FOREACH (CTxIn& usedVin, vecToExclude) {
            if (max.maxvin.prevout == usedVin.prevout) {
                found = true;
                break;
            }
        }
        if (found) continue;
        if (--rand < 1) {
            return &max;
        }
    }

    return NULL;
}

CMaxnode* CMaxnodeMan::GetCurrentMaxNode(int mod, int64_t nBlockHeight, int minProtocol)
{
    int64_t score = 0;
    CMaxnode* winner = NULL;

    // scan for winner
    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        max.Check();
        if (max.protocolVersion < minProtocol || !max.IsEnabled()) continue;

        // calculate the score for each Maxnode
        uint256 n = max.CalculateScore(mod, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        // determine the winner
        if (n2 > score) {
            score = n2;
            winner = &max;
        }
    }

    return winner;
}

int CMaxnodeMan::GetMaxnodeRank(const CTxIn& maxvin, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<int64_t, CTxIn> > vecMaxnodeScores;
    int64_t nMaxnode_Min_Age = MAX_WINNER_MINIMUM_AGE;
    int64_t nMaxnode_Age = 0;

    //make sure we know about this block
    uint256 hash = 0;
    if (!GetBlockHash(hash, nBlockHeight)) return -1;

    // scan for winner
    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        if (max.protocolVersion < minProtocol) {
            LogPrint("maxnode","Skipping Maxnode with obsolete version %d\n", max.protocolVersion);
            continue;                                                       // Skip obsolete versions
        }

        if (IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT)) {
            nMaxnode_Age = GetAdjustedTime() - max.sigTime;
            if ((nMaxnode_Age) < nMaxnode_Min_Age) {
                if (fDebug) LogPrint("maxnode","Skipping just activated Maxnode. Age: %ld\n", nMaxnode_Age);
                continue;                                                   // Skip maxnodes younger than (default) 1 hour
            }
        }
        if (fOnlyActive) {
            max.Check();
            if (!max.IsEnabled()) continue;
        }
        uint256 n = max.CalculateScore(1, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        vecMaxnodeScores.push_back(make_pair(n2, max.maxvin));
    }

    sort(vecMaxnodeScores.rbegin(), vecMaxnodeScores.rend(), CompareScoreTxIn());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CTxIn) & s, vecMaxnodeScores) {
        rank++;
        if (s.second.prevout == maxvin.prevout) {
            return rank;
        }
    }

    return -1;
}

std::vector<pair<int, CMaxnode> > CMaxnodeMan::GetMaxnodeRanks(int64_t nBlockHeight, int minProtocol)
{
    std::vector<pair<int64_t, CMaxnode> > vecMaxnodeScores;
    std::vector<pair<int, CMaxnode> > vecMaxnodeRanks;

    //make sure we know about this block
    uint256 hash = 0;
    if (!GetBlockHash(hash, nBlockHeight)) return vecMaxnodeRanks;

    // scan for winner
    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        max.Check();

        if (max.protocolVersion < minProtocol) continue;

        if (!max.IsEnabled()) {
            vecMaxnodeScores.push_back(make_pair(9999, max));
            continue;
        }

        uint256 n = max.CalculateScore(1, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        vecMaxnodeScores.push_back(make_pair(n2, max));
    }

    sort(vecMaxnodeScores.rbegin(), vecMaxnodeScores.rend(), CompareScoreMAX());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CMaxnode) & s, vecMaxnodeScores) {
        rank++;
        vecMaxnodeRanks.push_back(make_pair(rank, s.second));
    }

    return vecMaxnodeRanks;
}

CMaxnode* CMaxnodeMan::GetMaxnodeByRank(int nRank, int64_t nBlockHeight, int minProtocol, bool fOnlyActive)
{
    std::vector<pair<int64_t, CTxIn> > vecMaxnodeScores;

    // scan for winner
    BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
        if (max.protocolVersion < minProtocol) continue;
        if (fOnlyActive) {
            max.Check();
            if (!max.IsEnabled()) continue;
        }

        uint256 n = max.CalculateScore(1, nBlockHeight);
        int64_t n2 = n.GetCompact(false);

        vecMaxnodeScores.push_back(make_pair(n2, max.maxvin));
    }

    sort(vecMaxnodeScores.rbegin(), vecMaxnodeScores.rend(), CompareScoreTxIn());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CTxIn) & s, vecMaxnodeScores) {
        rank++;
        if (rank == nRank) {
            return Find(s.second);
        }
    }

    return NULL;
}

void CMaxnodeMan::ProcessMaxnodeConnections()
{
    //we don't care about this for regtest
    if (Params().NetworkID() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    BOOST_FOREACH (CNode* pnode, vNodes) {
        if (pnode->fObfuScationMaster) {
            if (obfuScationPool.pSubmittedToMaxnode != NULL && pnode->addr == obfuScationPool.pSubmittedToMaxnode->addr) continue;
            LogPrint("maxnode","Closing Maxnode connection peer=%i \n", pnode->GetId());
            pnode->fObfuScationMaster = false;
            pnode->Release();
        }
    }
}

void CMaxnodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return; //disable all Obfuscation/Maxnode related functionality
    if (!maxnodeSync.IsBlockchainSynced()) return;

    LOCK(cs_process_message);

    if (strCommand == "maxb") { //Maxnode Broadcast
        CMaxnodeBroadcast maxb;
        vRecv >> maxb;

        if (mapSeenMaxnodeBroadcast.count(maxb.GetHash())) { //seen
            maxnodeSync.AddedMaxnodeList(maxb.GetHash());
            return;
        }
        mapSeenMaxnodeBroadcast.insert(make_pair(maxb.GetHash(), maxb));

        int nDoS = 0;
        if (!maxb.CheckAndUpdate(nDoS)) {
            if (nDoS > 0)
                Misbehaving(pfrom->GetId(), nDoS);

            //failed
            return;
        }

        // make sure the vout that was signed is related to the transaction that spawned the Maxnode
        //  - this is expensive, so it's only done once per Maxnode
        if (!obfuScationSigner.IsMaxVinAssociatedWithPubkey(maxb.maxvin, maxb.pubKeyCollateralAddress)) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : maxb - Got mismatched pubkey and maxvin\n");
            Misbehaving(pfrom->GetId(), 33);
            return;
        }

        // make sure it's still unspent
        //  - this is checked later by .check() in many places and by ThreadCheckObfuScationPool()
        if (maxb.CheckInputsAndAdd(nDoS)) {
            // use this as a peer
            addrman.Add(CAddress(maxb.addr), pfrom->addr, 2 * 60 * 60);
            maxnodeSync.AddedMaxnodeList(maxb.GetHash());
        } else {
            LogPrint("maxnode","maxb - Rejected Maxnode entry %s\n", maxb.maxvin.prevout.hash.ToString());

            if (nDoS > 0)
                Misbehaving(pfrom->GetId(), nDoS);
        }
    }

    else if (strCommand == "maxp") { //Maxnode Ping
        CMaxnodePing maxp;
        vRecv >> maxp;

        LogPrint("maxnode", "maxp - Maxnode ping, maxvin: %s\n", maxp.maxvin.prevout.hash.ToString());

        if (mapSeenMaxnodePing.count(maxp.GetHash())) return; //seen
        mapSeenMaxnodePing.insert(make_pair(maxp.GetHash(), maxp));

        int nDoS = 0;
        if (maxp.CheckAndUpdate(nDoS)) return;

        if (nDoS > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDoS);
        } else {
            // if nothing significant failed, search existing Maxnode list
            CMaxnode* pmax = Find(maxp.maxvin);
            // if it's known, don't ask for the maxb, just return
            if (pmax != NULL) return;
        }

        // something significant is broken or max is unknown,
        // we might have to ask for a maxnode entry once
        AskForMAX(pfrom, maxp.maxvin);

    } else if (strCommand == "dmaxseg") { //Get Maxnode list or specific entry

        CTxIn maxvin;
        vRecv >> maxvin;

        if (maxvin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if (!isLocal && Params().NetworkID() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForMaxnodeList.find(pfrom->addr);
                if (i != mAskedUsForMaxnodeList.end()) {
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxseg - peer already asked me for the list\n");
                        Misbehaving(pfrom->GetId(), 34);
                        return;
                    }
                }
                int64_t askAgain = GetTime() + MAXNODES_DSEG_SECONDS;
                mAskedUsForMaxnodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok


        int nInvCount = 0;

        BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
            if (max.addr.IsRFC1918()) continue; //local network

            if (max.IsEnabled()) {
                LogPrint("maxnode", "dmaxseg - Sending Maxnode entry - %s \n", max.maxvin.prevout.hash.ToString());
                if (maxvin == CTxIn() || maxvin == max.maxvin) {
                    CMaxnodeBroadcast maxb = CMaxnodeBroadcast(max);
                    uint256 hash = maxb.GetHash();
                    pfrom->PushInventory(CInv(MSG_MAXNODE_ANNOUNCE, hash));
                    nInvCount++;

                    if (!mapSeenMaxnodeBroadcast.count(hash)) mapSeenMaxnodeBroadcast.insert(make_pair(hash, maxb));

                    if (maxvin == max.maxvin) {
                        LogPrint("maxnode", "dmaxseg - Sent 1 Maxnode entry to peer %i\n", pfrom->GetId());
                        return;
                    }
                }
            }
        }

        if (maxvin == CTxIn()) {
            pfrom->PushMessage("smaxsc", MAXNODE_SYNC_LIST, nInvCount);
            LogPrint("maxnode", "dmaxseg - Sent %d Maxnode entries to peer %i\n", nInvCount, pfrom->GetId());
        }
    }
    /*
     * IT'S SAFE TO REMOVE THIS IN FURTHER VERSIONS
     * AFTER MIGRATION TO V12 IS DONE
     */

    // Light version for OLD MASSTERNODES - fake pings, no self-activation
    else if (strCommand == "dmaxsee") { //ObfuScation Election Entry

        if (IsSporkActive(SPORK_10_MASTERNODE_PAY_UPDATED_NODES)) return;

        CTxIn maxvin;
        CService addr;
        CPubKey pubkey;
        CPubKey pubkey2;
        vector<unsigned char> vchSig;
        int64_t sigTime;
        int count;
        int current;
        int64_t lastUpdated;
        int protocolVersion;
        CScript donationAddress;
        int donationPercentage;
        std::string strMessage;

        vRecv >> maxvin >> addr >> vchSig >> sigTime >> pubkey >> pubkey2 >> count >> current >> lastUpdated >> protocolVersion >> donationAddress >> donationPercentage;

        // make sure signature isn't in the future (past is OK)
        if (sigTime > GetAdjustedTime() + 60 * 60) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - Signature rejected, too far into the future %s\n", maxvin.prevout.hash.ToString());
            Misbehaving(pfrom->GetId(), 1);
            return;
        }

        std::string vchPubKey(pubkey.begin(), pubkey.end());
        std::string vchPubKey2(pubkey2.begin(), pubkey2.end());

        strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(protocolVersion) + donationAddress.ToString() + boost::lexical_cast<std::string>(donationPercentage);

        if (protocolVersion < maxnodePayments.GetMinMaxnodePaymentsProto()) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - ignoring outdated Maxnode %s protocol version %d < %d\n", maxvin.prevout.hash.ToString(), protocolVersion, maxnodePayments.GetMinMaxnodePaymentsProto());
            Misbehaving(pfrom->GetId(), 1);
            return;
        }

        CScript pubkeyScript;
        pubkeyScript = GetScriptForDestination(pubkey.GetID());

        if (pubkeyScript.size() != 25) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - pubkey the wrong size\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        CScript pubkeyScript2;
        pubkeyScript2 = GetScriptForDestination(pubkey2.GetID());

        if (pubkeyScript2.size() != 25) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - pubkey2 the wrong size\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        if (!maxvin.scriptSig.empty()) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - Ignore Not Empty ScriptSig %s\n", maxvin.prevout.hash.ToString());
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        std::string errorMessage = "";
        if (!obfuScationSigner.VerifyMessage(pubkey, vchSig, strMessage, errorMessage)) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - Got bad Maxnode address signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (addr.GetPort() != 27071) return;
        } else if (addr.GetPort() == 27071)
            return;

        //search existing Maxnode list, this is where we update existing Maxnodes with new dmaxsee broadcasts
        CMaxnode* pmax = this->Find(maxvin);
        if (pmax != NULL) {
            // count == -1 when it's a new entry
            //   e.g. We don't want the entry relayed/time updated when we're syncing the list
            // max.pubkey = pubkey, IsVinAssociatedWithPubkey is validated once below,
            //   after that they just need to match
            if (count == -1 && pmax->pubKeyCollateralAddress == pubkey && (GetAdjustedTime() - pmax->nLastDsee > MAXNODE_MIN_MAXB_SECONDS)) {
                if (pmax->protocolVersion > GETHEADERS_VERSION && sigTime - pmax->lastPing.sigTime < MAXNODE_MIN_MAXB_SECONDS) return;
                if (pmax->nLastDsee < sigTime) { //take the newest entry
                    LogPrint("maxnode", "dmaxsee - Got updated entry for %s\n", maxvin.prevout.hash.ToString());
                    if (pmax->protocolVersion < GETHEADERS_VERSION) {
                        pmax->pubKeyMaxnode = pubkey2;
                        pmax->sigTime = sigTime;
                        pmax->sig = vchSig;
                        pmax->protocolVersion = protocolVersion;
                        pmax->addr = addr;
                        //fake ping
                        pmax->lastPing = CMaxnodePing(maxvin);
                    }
                    pmax->nLastDsee = sigTime;
                    pmax->Check();
                    if (pmax->IsEnabled()) {
                        TRY_LOCK(cs_vNodes, lockNodes);
                        if (!lockNodes) return;
                        BOOST_FOREACH (CNode* pnode, vNodes)
                            if (pnode->nVersion >= maxnodePayments.GetMinMaxnodePaymentsProto())
                                pnode->PushMessage("dmaxsee", maxvin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
                    }
                }
            }

            return;
        }

        static std::map<COutPoint, CPubKey> mapSeenDsee;
        if (mapSeenDsee.count(maxvin.prevout) && mapSeenDsee[maxvin.prevout] == pubkey) {
            LogPrint("maxnode", "dmaxsee - already seen this maxvin %s\n", maxvin.prevout.ToString());
            return;
        }
        mapSeenDsee.insert(make_pair(maxvin.prevout, pubkey));
        // make sure the vout that was signed is related to the transaction that spawned the Maxnode
        //  - this is expensive, so it's only done once per Maxnode
        if (!obfuScationSigner.IsMaxVinAssociatedWithPubkey(maxvin, pubkey)) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - Got mismatched pubkey and maxvin\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }


        LogPrint("maxnode", "dmaxsee - Got NEW OLD Maxnode entry %s\n", maxvin.prevout.hash.ToString());

        // make sure it's still unspent
        //  - this is checked later by .check() in many places and by ThreadCheckObfuScationPool()

        CValidationState state;
        CMutableTransaction tx = CMutableTransaction();

	//CTxOut vout = CTxOut(((MAXNODE_T1_COLLATERAL_AMOUNT - 0.01) * COIN) || ((MAXNODE_T2_COLLATERAL_AMOUNT - 0.01) * COIN) || ((MAXNODE_T3_COLLATERAL_AMOUNT - 0.01) * COIN), obfuScationPool.collateralPubKey);
	CTxOut vout = CTxOut((MAXNODE_COLLATERAL_AMOUNT - 0.01) * COIN, obfuScationPool.collateralPubKey);

        tx.vin.push_back(maxvin);
        tx.vout.push_back(vout);

        bool fAcceptable = false;
        {
            TRY_LOCK(cs_main, lockMain);
            if (!lockMain) return;
            fAcceptable = AcceptableInputs(mempool, state, CTransaction(tx), false, NULL);
        }

        if (fAcceptable) {
            if (GetInputAge(maxvin) < MAXNODE_MIN_CONFIRMATIONS) {
                LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxsee - Input must have least %d confirmations\n", MAXNODE_MIN_CONFIRMATIONS);
                Misbehaving(pfrom->GetId(), 20);
                return;
            }

            // verify that sig time is legit in past
            // should be at least not earlier than block when 1000 PIVX tx got MAXNODE_MIN_CONFIRMATIONS
            uint256 hashBlock = 0;
            CTransaction tx2;
            GetTransaction(maxvin.prevout.hash, tx2, hashBlock, true);
            BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                CBlockIndex* pMAXIndex = (*mi).second;                                                        // block for 10000 PIV tx -> 1 confirmation
                CBlockIndex* pConfIndex = chainActive[pMAXIndex->nHeight + MAXNODE_MIN_CONFIRMATIONS - 1]; // block where tx got MAXNODE_MIN_CONFIRMATIONS
                if (pConfIndex->GetBlockTime() > sigTime) {
                    LogPrint("maxnode","maxb - Bad sigTime %d for Maxnode %s (%i conf block is at %d)\n",
                        sigTime, maxvin.prevout.hash.ToString(), MAXNODE_MIN_CONFIRMATIONS, pConfIndex->GetBlockTime());
                    return;
                }
            }

            // use this as a peer
            addrman.Add(CAddress(addr), pfrom->addr, 2 * 60 * 60);

            // add Maxnode
            CMaxnode max = CMaxnode();
            max.addr = addr;
            max.maxvin = maxvin;
            max.pubKeyCollateralAddress = pubkey;
            max.sig = vchSig;
            max.sigTime = sigTime;
            max.pubKeyMaxnode = pubkey2;
            max.protocolVersion = protocolVersion;
            // fake ping
            max.lastPing = CMaxnodePing(maxvin);
            max.Check(true);
            // add v11 maxnodes, v12 should be added by maxb only
            if (protocolVersion < GETHEADERS_VERSION) {
                LogPrint("maxnode", "dmaxsee - Accepted OLD Maxnode entry %i %i\n", count, current);
                Add(max);
            }
            if (max.IsEnabled()) {
                TRY_LOCK(cs_vNodes, lockNodes);
                if (!lockNodes) return;
                BOOST_FOREACH (CNode* pnode, vNodes)
                    if (pnode->nVersion >= maxnodePayments.GetMinMaxnodePaymentsProto())
                        pnode->PushMessage("dmaxsee", maxvin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion, donationAddress, donationPercentage);
            }
        } else {
            LogPrint("maxnode","dmaxsee - Rejected Maxnode entry %s\n", maxvin.prevout.hash.ToString());

            int nDoS = 0;
            if (state.IsInvalid(nDoS)) {
                LogPrint("maxnode","dmaxsee - %s from %i %s was not accepted into the memory pool\n", tx.GetHash().ToString().c_str(),
                    pfrom->GetId(), pfrom->cleanSubVer.c_str());
                if (nDoS > 0)
                    Misbehaving(pfrom->GetId(), nDoS);
            }
        }
    }

    else if (strCommand == "dmaxseep") { //ObfuScation Election Entry Ping

        if (IsSporkActive(SPORK_10_MASTERNODE_PAY_UPDATED_NODES)) return;

        CTxIn maxvin;
        vector<unsigned char> vchSig;
        int64_t sigTime;
        bool stop;
        vRecv >> maxvin >> vchSig >> sigTime >> stop;

        //LogPrint("maxnode","dmaxseep - Received: maxvin: %s sigTime: %lld stop: %s\n", maxvin.ToString().c_str(), sigTime, stop ? "true" : "false");

        if (sigTime > GetAdjustedTime() + 60 * 60) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxseep - Signature rejected, too far into the future %s\n", maxvin.prevout.hash.ToString());
            Misbehaving(pfrom->GetId(), 1);
            return;
        }

        if (sigTime <= GetAdjustedTime() - 60 * 60) {
            LogPrintf("CMaxnodeMan::ProcessMessage() : dmaxseep - Signature rejected, too far into the past %s - %d %d \n", maxvin.prevout.hash.ToString(), sigTime, GetAdjustedTime());
            Misbehaving(pfrom->GetId(), 1);
            return;
        }

        std::map<COutPoint, int64_t>::iterator i = mWeAskedForMaxnodeListEntry.find(maxvin.prevout);
        if (i != mWeAskedForMaxnodeListEntry.end()) {
            int64_t t = (*i).second;
            if (GetTime() < t) return; // we've asked recently
        }

        // see if we have this Maxnode
        CMaxnode* pmax = this->Find(maxvin);
        if (pmax != NULL && pmax->protocolVersion >= maxnodePayments.GetMinMaxnodePaymentsProto()) {
            // LogPrint("maxnode","dmaxseep - Found corresponding max for maxvin: %s\n", maxvin.ToString().c_str());
            // take this only if it's newer
            if (sigTime - pmax->nLastDseep > MAXNODE_MIN_MAXP_SECONDS) {
                std::string strMessage = pmax->addr.ToString() + boost::lexical_cast<std::string>(sigTime) + boost::lexical_cast<std::string>(stop);

                std::string errorMessage = "";
                if (!obfuScationSigner.VerifyMessage(pmax->pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
                    LogPrint("maxnode","dmaxseep - Got bad Maxnode address signature %s \n", maxvin.prevout.hash.ToString());
                    //Misbehaving(pfrom->GetId(), 100);
                    return;
                }

                // fake ping for v11 maxnodes, ignore for v12
                if (pmax->protocolVersion < GETHEADERS_VERSION) pmax->lastPing = CMaxnodePing(maxvin);
                pmax->nLastDseep = sigTime;
                pmax->Check();
                if (pmax->IsEnabled()) {
                    TRY_LOCK(cs_vNodes, lockNodes);
                    if (!lockNodes) return;
                    LogPrint("maxnode", "dmaxseep - relaying %s \n", maxvin.prevout.hash.ToString());
                    BOOST_FOREACH (CNode* pnode, vNodes)
                        if (pnode->nVersion >= maxnodePayments.GetMinMaxnodePaymentsProto())
                            pnode->PushMessage("dmaxseep", maxvin, vchSig, sigTime, stop);
                }
            }
            return;
        }

        LogPrint("maxnode", "dmaxseep - Couldn't find Maxnode entry %s peer=%i\n", maxvin.prevout.hash.ToString(), pfrom->GetId());

        AskForMAX(pfrom, maxvin);
    }

    /*
     * END OF "REMOVE"
     */
}

void CMaxnodeMan::Remove(CTxIn maxvin)
{
    LOCK(cs);

    vector<CMaxnode>::iterator it = vMaxnodes.begin();
    while (it != vMaxnodes.end()) {
        if ((*it).maxvin == maxvin) {
            LogPrint("maxnode", "CMaxnodeMan: Removing Maxnode %s - %i now\n", (*it).maxvin.prevout.hash.ToString(), size() - 1);
            vMaxnodes.erase(it);
            break;
        }
        ++it;
    }
}

void CMaxnodeMan::UpdateMaxnodeList(CMaxnodeBroadcast maxb)
{
	mapSeenMaxnodePing.insert(make_pair(maxb.lastPing.GetHash(), maxb.lastPing));
	mapSeenMaxnodeBroadcast.insert(make_pair(maxb.GetHash(), maxb));
	maxnodeSync.AddedMaxnodeList(maxb.GetHash());

    LogPrint("maxnode","CMaxnodeMan::UpdateMaxnodeList() -- maxnode=%s\n", maxb.maxvin.prevout.ToString());

    CMaxnode* pmax = Find(maxb.maxvin);
    if (pmax == NULL) {
        CMaxnode max(maxb);
        Add(max);
    } else {
    	pmax->UpdateFromNewBroadcast(maxb);
    }
}

std::string CMaxnodeMan::ToString() const
{
    std::ostringstream info;

    info << "Maxnodes: " << (int)vMaxnodes.size() << ", peers who asked us for Maxnode list: " << (int)mAskedUsForMaxnodeList.size() << ", peers we asked for Maxnode list: " << (int)mWeAskedForMaxnodeList.size() << ", entries in Maxnode list we asked for: " << (int)mWeAskedForMaxnodeListEntry.size() << ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}
