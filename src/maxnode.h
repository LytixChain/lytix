// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAXNODE_H
#define MAXNODE_H

#include "base58.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"

#define MAXNODE_MIN_CONFIRMATIONS 15
#define MAXNODE_MIN_MAXP_SECONDS (10 * 60)
#define MAXNODE_MIN_MAXB_SECONDS (5 * 60)
#define MAXNODE_PING_SECONDS (5 * 60)
#define MAXNODE_EXPIRATION_SECONDS (120 * 60)
#define MAXNODE_REMOVAL_SECONDS (130 * 60)
#define MAXNODE_CHECK_SECONDS 5

using namespace std;

class CMaxnode;
class CMaxnodeT1;
class CMaxnodeT2;
class CMaxnodeT3;
class CMaxnodeBroadcast;
class CMaxnodePing;
extern map<int64_t, uint256> mapMaxCacheBlockHashes;

bool GetMaxBlockHash(uint256& hash, int nBlockHeight);


//
// The Maxnode Ping Class : Contains a different serialize method for sending pings from maxnodes throughout the network
//

class CMaxnodePing
{
public:
    CTxIn maxvin;
    uint256 blockHash;
    int64_t sigTime; //maxb message times
    std::vector<unsigned char> vchSig;
    //removed stop

    CMaxnodePing();
    CMaxnodePing(CTxIn& newVin);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(maxvin);
        READWRITE(blockHash);
        READWRITE(sigTime);
        READWRITE(vchSig);
    }

    bool CheckAndUpdate(int& nDos, bool fRequireEnabled = true, bool fCheckSigTimeOnly = false);
    bool Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode);
    bool VerifySignature(CPubKey& pubKeyMaxnode, int &nDos);
    void Relay();

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << maxvin;
        ss << sigTime;
        return ss.GetHash();
    }

    void swap(CMaxnodePing& first, CMaxnodePing& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.maxvin, second.maxvin);
        swap(first.blockHash, second.blockHash);
        swap(first.sigTime, second.sigTime);
        swap(first.vchSig, second.vchSig);
    }

    CMaxnodePing& operator=(CMaxnodePing from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CMaxnodePing& a, const CMaxnodePing& b)
    {
        return a.maxvin == b.maxvin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CMaxnodePing& a, const CMaxnodePing& b)
    {
        return !(a == b);
    }
};

//
// The Maxnode Class. For managing the Obfuscation process. It contains the input of the 1000 LYTX, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CMaxnode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    int64_t lastTimeChecked;

public:
    enum state {
        MAXNODE_PRE_ENABLED,
        MAXNODE_ENABLED,
        MAXNODE_EXPIRED,
        MAXNODE_OUTPOINT_SPENT,
        MAXNODE_REMOVE,
        MAXNODE_WATCHDOG_EXPIRED,
        MAXNODE_POSE_BAN,
        MAXNODE_VIN_SPENT,
        MAXNODE_POS_ERROR
    };

    CTxIn maxvin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyMaxnode;
    CPubKey pubKeyCollateralAddress1;
    CPubKey pubKeyMaxnode1;
    std::vector<unsigned char> sig;
    int activeState;
    int64_t sigTime; //maxb message time
    int cacheInputAge;
    int cacheInputAgeBlock;
    bool unitTest;
    bool allowFreeTx;
    int protocolVersion;
    int nActiveState;
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int nScanningErrorCount;
    int nLastScanningErrorBlockHeight;
    CMaxnodePing lastPing;

    int64_t nLastDsee;  // temporary, do not save. Remove after migration to v12
    int64_t nLastDseep; // temporary, do not save. Remove after migration to v12

    CMaxnode();
    CMaxnode(const CMaxnode& other);
    CMaxnode(const CMaxnodeBroadcast& maxb);


    void swap(CMaxnode& first, CMaxnode& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.maxvin, second.maxvin);
        swap(first.addr, second.addr);
        swap(first.pubKeyCollateralAddress, second.pubKeyCollateralAddress);
        swap(first.pubKeyMaxnode, second.pubKeyMaxnode);
        swap(first.sig, second.sig);
        swap(first.activeState, second.activeState);
        swap(first.sigTime, second.sigTime);
        swap(first.lastPing, second.lastPing);
        swap(first.cacheInputAge, second.cacheInputAge);
        swap(first.cacheInputAgeBlock, second.cacheInputAgeBlock);
        swap(first.unitTest, second.unitTest);
        swap(first.allowFreeTx, second.allowFreeTx);
        swap(first.protocolVersion, second.protocolVersion);
        swap(first.nLastDsq, second.nLastDsq);
        swap(first.nScanningErrorCount, second.nScanningErrorCount);
        swap(first.nLastScanningErrorBlockHeight, second.nLastScanningErrorBlockHeight);
    }

    CMaxnode& operator=(CMaxnode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CMaxnode& a, const CMaxnode& b)
    {
        return a.maxvin == b.maxvin;
    }
    friend bool operator!=(const CMaxnode& a, const CMaxnode& b)
    {
        return !(a.maxvin == b.maxvin);
    }

    uint256 CalculateScore(int mod = 1, int64_t nBlockHeight = 0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        LOCK(cs);

        READWRITE(maxvin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyMaxnode);
        READWRITE(sig);
        READWRITE(sigTime);
        READWRITE(protocolVersion);
        READWRITE(activeState);
        READWRITE(lastPing);
        READWRITE(cacheInputAge);
        READWRITE(cacheInputAgeBlock);
        READWRITE(unitTest);
        READWRITE(allowFreeTx);
        READWRITE(nLastDsq);
        READWRITE(nScanningErrorCount);
        READWRITE(nLastScanningErrorBlockHeight);
    }

    int64_t SecondsSincePayment();

    bool UpdateFromNewBroadcast(CMaxnodeBroadcast& maxb);

    inline uint64_t SliceHash(uint256& hash, int slice)
    {
        uint64_t n = 0;
        memcpy(&n, &hash + slice * 64, 64);
        return n;
    }

    void Check(bool forceCheck = false);

    bool IsBroadcastedWithin(int seconds)
    {
        return (GetAdjustedTime() - sigTime) < seconds;
    }

    bool IsPingedWithin(int seconds, int64_t now = -1)
    {
        now == -1 ? now = GetAdjustedTime() : now;

        return (lastPing == CMaxnodePing()) ? false : now - lastPing.sigTime < seconds;
    }

    void Disable()
    {
        sigTime = 0;
        lastPing = CMaxnodePing();
    }

    bool IsEnabled()
    {
        return activeState == MAXNODE_ENABLED;
    }

    int GetMaxnodeInputAge()
    {
        if (chainActive.Tip() == NULL) return 0;

        if (cacheInputAge == 0) {
            cacheInputAge = GetInputAge(maxvin);
            cacheInputAgeBlock = chainActive.Tip()->nHeight;
        }

        return cacheInputAge + (chainActive.Tip()->nHeight - cacheInputAgeBlock);
    }

    std::string GetStatus();

    std::string Status()
    {
        std::string strStatus = "ACTIVE";

        if (activeState == CMaxnode::MAXNODE_ENABLED) strStatus = "ENABLED";
        if (activeState == CMaxnode::MAXNODE_EXPIRED) strStatus = "EXPIRED";
        if (activeState == CMaxnode::MAXNODE_VIN_SPENT) strStatus = "VIN_SPENT";
        if (activeState == CMaxnode::MAXNODE_REMOVE) strStatus = "REMOVE";
        if (activeState == CMaxnode::MAXNODE_POS_ERROR) strStatus = "POS_ERROR";

        return strStatus;
    }

    int64_t GetLastPaid();
    bool IsValidNetAddr();
};


//
// The Maxnode Broadcast Class : Contains a different serialize method for sending maxnodes through the network
//

class CMaxnodeBroadcast : public CMaxnode
{
public:
    CMaxnodeBroadcast();
    CMaxnodeBroadcast(CService newAddr, CTxIn newVin, CPubKey newPubkey, CPubKey newPubkey2, int protocolVersionIn);
    CMaxnodeBroadcast(const CMaxnode& max);

    bool CheckAndUpdate(int& nDoS);
    bool CheckInputsAndAdd(int& nDos);
    bool Sign(CKey& keyCollateralAddress);
    bool VerifySignature();
    void Relay();
    std::string GetOldStrMessage();
    std::string GetNewStrMessage();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(maxvin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyMaxnode);
        READWRITE(sig);
        READWRITE(sigTime);
        READWRITE(protocolVersion);
        READWRITE(lastPing);
        READWRITE(nLastDsq);
    }

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << sigTime;
        ss << pubKeyCollateralAddress;
        return ss.GetHash();
    }

    /// Create Maxnode broadcast, needs to be relayed manually after that
    static bool Create(CTxIn maxvin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyMaxnodeNew, CPubKey pubKeyMaxnodeNew, std::string& strErrorRet, CMaxnodeBroadcast& maxbRet);
    static bool Create(std::string strService, std::string strKey, std::string strTxHash, std::string strOutputIndex, std::string& strErrorRet, CMaxnodeBroadcast& maxbRet, bool fOffline = false);
    static bool CheckDefaultPort(std::string strService, std::string& strErrorRet, std::string strContext);
};

#endif
