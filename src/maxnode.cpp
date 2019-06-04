// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "maxnode.h"
#include "addrman.h"
#include "maxnodeman.h"
#include "obfuscation.h"
#include "sync.h"
#include "util.h"
#include <boost/lexical_cast.hpp>


// More to come - nh

// keep track of the scanning errors I've seen
map<uint256, int> mapSeenMaxnodeScanningErrors;
// cache block hashes as we calculate them
std::map<int64_t, uint256> mapMaxCacheBlockHashes;

//Get the last hash that matches the modulus given. Processed in reverse order
bool GetMaxBlockHash(uint256& hash, int nBlockHeight)
{
    if (chainActive.Tip() == NULL) return false;

    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Tip()->nHeight;

    if (mapMaxCacheBlockHashes.count(nBlockHeight)) {
        hash = mapMaxCacheBlockHashes[nBlockHeight];
        return true;
    }

    const CBlockIndex* BlockLastSolved = chainActive.Tip();
    const CBlockIndex* BlockReading = chainActive.Tip();

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || chainActive.Tip()->nHeight + 1 < nBlockHeight) return false;

    int nBlocksAgo = 0;
    if (nBlockHeight > 0) nBlocksAgo = (chainActive.Tip()->nHeight + 1) - nBlockHeight;
    assert(nBlocksAgo >= 0);

    int n = 0;
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (n >= nBlocksAgo) {
            hash = BlockReading->GetMaxBlockHash();
            mapMaxCacheBlockHashes[nBlockHeight] = hash;
            return true;
        }
        n++;

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    return false;
}

CMaxnode::CMaxnode()
{
    LOCK(cs);
    maxvin = CTxIn();
    addr = CService();
    pubKeyCollateralAddress = CPubKey();
    pubKeyMaxnode = CPubKey();
    sig = std::vector<unsigned char>();
    activeState = MAXNODE_ENABLED;
    sigTime = GetAdjustedTime();
    lastPing = CMaxnodePing();
    cacheInputAge = 0;
    cacheInputAgeBlock = 0;
    unitTest = false;
    allowFreeTx = true;
    nActiveState = MAXNODE_ENABLED,
    protocolVersion = PROTOCOL_VERSION;
    nLastDsq = 0;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
    lastTimeChecked = 0;
    nLastDsee = 0;  // temporary, do not save. Remove after migration to v12
    nLastDseep = 0; // temporary, do not save. Remove after migration to v12
}

CMaxnode::CMaxnode(const CMaxnode& other)
{
    LOCK(cs);
    maxvin = other.maxvin;
    addr = other.addr;
    pubKeyCollateralAddress = other.pubKeyCollateralAddress;
    pubKeyMaxnode = other.pubKeyMaxnode;
    sig = other.sig;
    activeState = other.activeState;
    sigTime = other.sigTime;
    lastPing = other.lastPing;
    cacheInputAge = other.cacheInputAge;
    cacheInputAgeBlock = other.cacheInputAgeBlock;
    unitTest = other.unitTest;
    allowFreeTx = other.allowFreeTx;
    nActiveState = MAXNODE_ENABLED,
    protocolVersion = other.protocolVersion;
    nLastDsq = other.nLastDsq;
    nScanningErrorCount = other.nScanningErrorCount;
    nLastScanningErrorBlockHeight = other.nLastScanningErrorBlockHeight;
    lastTimeChecked = 0;
    nLastDsee = other.nLastDsee;   // temporary, do not save. Remove after migration to v12
    nLastDseep = other.nLastDseep; // temporary, do not save. Remove after migration to v12
}

CMaxnode::CMaxnode(const CMaxnodeBroadcast& maxb)
{
    LOCK(cs);
    maxvin = maxb.maxvin;
    addr = maxb.addr;
    pubKeyCollateralAddress = maxb.pubKeyCollateralAddress;
    pubKeyMaxnode = maxb.pubKeyMaxnode;
    sig = maxb.sig;
    activeState = MAXNODE_ENABLED;
    sigTime = maxb.sigTime;
    lastPing = maxb.lastPing;
    cacheInputAge = 0;
    cacheInputAgeBlock = 0;
    unitTest = false;
    allowFreeTx = true;
    nActiveState = MAXNODE_ENABLED,
    protocolVersion = maxb.protocolVersion;
    nLastDsq = maxb.nLastDsq;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
    lastTimeChecked = 0;
    nLastDsee = 0;  // temporary, do not save. Remove after migration to v12
    nLastDseep = 0; // temporary, do not save. Remove after migration to v12
}

//
// When a new maxnode broadcast is sent, update our information
//
bool CMaxnode::UpdateFromNewBroadcast(CMaxnodeBroadcast& maxb)
{
    if (maxb.sigTime > sigTime) {
        pubKeyMaxnode = maxb.pubKeyMaxnode;
        pubKeyCollateralAddress = maxb.pubKeyCollateralAddress;
        sigTime = maxb.sigTime;
        sig = maxb.sig;
        protocolVersion = maxb.protocolVersion;
        addr = maxb.addr;
        lastTimeChecked = 0;
        int nDoS = 0;
        if (maxb.lastPing == CMaxnodePing() || (maxb.lastPing != CMaxnodePing() && maxb.lastPing.CheckAndUpdate(nDoS, false))) {
            lastPing = maxb.lastPing;
            maxnodeman.mapSeenMaxnodePing.insert(make_pair(lastPing.GetHash(), lastPing));
        }
        return true;
    }
    return false;
}

//
// Deterministically calculate a given "score" for a Maxnode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
uint256 CMaxnode::CalculateScore(int mod, int64_t nBlockHeight)
{
    if (chainActive.Tip() == NULL) return 0;

    uint256 hash = 0;
    uint256 aux = maxvin.prevout.hash + maxvin.prevout.n;

    if (!GetMaxBlockHash(hash, nBlockHeight)) {
        LogPrint("maxnode","CalculateScore ERROR - nHeight %d - Returned 0\n", nBlockHeight);
        return 0;
    }

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << hash;
    uint256 hash2 = ss.GetHash();

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << hash;
    ss2 << aux;
    uint256 hash3 = ss2.GetHash();

    uint256 r = (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);

    return r;
}

void CMaxnode::Check(bool forceCheck)
{
    if (ShutdownRequested()) return;

    if (!forceCheck && (GetTime() - lastTimeChecked < MAXNODE_CHECK_SECONDS)) return;
    lastTimeChecked = GetTime();


    //once spent, stop doing the checks
    if (activeState == MAXNODE_VIN_SPENT) return;


    if (!IsPingedWithin(MAXNODE_REMOVAL_SECONDS)) {
        activeState = MAXNODE_REMOVE;
        return;
    }

    if (!IsPingedWithin(MAXNODE_EXPIRATION_SECONDS)) {
        activeState = MAXNODE_EXPIRED;
        return;
    }

    if(lastPing.sigTime - sigTime < MAXNODE_MIN_MAXP_SECONDS){
    	activeState = MAXNODE_PRE_ENABLED;
    	return;
    }

    if (!unitTest) {
        CValidationState state;
        CMutableTransaction tx = CMutableTransaction();
        //CTxOut vout = CTxOut((((MAXNODE_T1_COLLATERAL_AMOUNT - 0.01) || (MAXNODE_T2_COLLATERAL_AMOUNT - 0.01) || (MAXNODE_T3_COLLATERAL_AMOUNT - 0.01)) * COIN), obfuScationPool.collateralPubKey);
        CTxOut vout = CTxOut((MAXNODE_COLLATERAL_AMOUNT - 0.01) * COIN, obfuScationPool.collateralPubKey);
        tx.vin.push_back(maxvin);
        tx.vout.push_back(vout);

        {
            TRY_LOCK(cs_main, lockMain);
            if (!lockMain) return;

            if (!AcceptableInputs(mempool, state, CTransaction(tx), false, NULL)) {
                activeState = MAXNODE_VIN_SPENT;
                return;
            }
        }
    }

    activeState = MAXNODE_ENABLED; // OK
}

int64_t CMaxnode::SecondsSincePayment()
{
    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    int64_t sec = (GetAdjustedTime() - GetLastPaid());
    int64_t month = 60 * 60 * 24 * 30;
    if (sec < month) return sec; //if it's less than 30 days, give seconds

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << maxvin;
    ss << sigTime;
    uint256 hash = ss.GetHash();

    // return some deterministic value for unknown/unpaid but force it to be more than 30 days old
    return month + hash.GetCompact(false);
}

int64_t CMaxnode::GetLastPaid()
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return false;

    CScript maxpayee;
    maxpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << maxvin;
    ss << sigTime;
    uint256 hash = ss.GetHash();

    // use a deterministic offset to break a tie -- 2.5 minutes
    int64_t nOffset = hash.GetCompact(false) % 150;

    if (chainActive.Tip() == NULL) return false;

    const CBlockIndex* BlockReading = chainActive.Tip();

    int nMnCount = maxnodeman.CountEnabled() * 1.25;
    int n = 0;
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (n >= nMnCount) {
            return 0;
        }
        n++;

        if (maxnodePayments.mapMaxnodeBlocks.count(BlockReading->nHeight)) {
            /*
                Search for this payee, with at least 2 votes. This will aid in consensus allowing the network
                to converge on the same payees quickly, then keep the same schedule.
            */
            if (maxnodePayments.mapMaxnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(maxpayee, 2)) {
                return BlockReading->nTime + nOffset;
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    return 0;
}

std::string CMaxnode::GetStatus()
{
    switch (nActiveState) {
    case CMaxnode::MAXNODE_PRE_ENABLED:
        return "PRE_ENABLED";
    case CMaxnode::MAXNODE_ENABLED:
        return "ENABLED";
    case CMaxnode::MAXNODE_EXPIRED:
        return "EXPIRED";
    case CMaxnode::MAXNODE_OUTPOINT_SPENT:
        return "OUTPOINT_SPENT";
    case CMaxnode::MAXNODE_REMOVE:
        return "REMOVE";
    case CMaxnode::MAXNODE_WATCHDOG_EXPIRED:
        return "WATCHDOG_EXPIRED";
    case CMaxnode::MAXNODE_POSE_BAN:
        return "POSE_BAN";
    default:
        return "UNKNOWN";
    }
}

bool CMaxnode::IsValidNetAddr()
{
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkID() == CBaseChainParams::REGTEST ||
           (IsReachable(addr) && addr.IsRoutable());
}

CMaxnodeBroadcast::CMaxnodeBroadcast()
{
    maxvin = CTxIn();
    addr = CService();
    pubKeyCollateralAddress = CPubKey();
    pubKeyMaxnode1 = CPubKey();
    sig = std::vector<unsigned char>();
    activeState = MAXNODE_ENABLED;
    sigTime = GetAdjustedTime();
    lastPing = CMaxnodePing();
    cacheInputAge = 0;
    cacheInputAgeBlock = 0;
    unitTest = false;
    allowFreeTx = true;
    protocolVersion = PROTOCOL_VERSION;
    nLastDsq = 0;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
}

CMaxnodeBroadcast::CMaxnodeBroadcast(CService newAddr, CTxIn newVin, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyMaxnodeNew, int protocolVersionIn)
{
    maxvin = newVin;
    addr = newAddr;
    pubKeyCollateralAddress = pubKeyCollateralAddressNew;
    pubKeyMaxnode = pubKeyMaxnodeNew;
    sig = std::vector<unsigned char>();
    activeState = MAXNODE_ENABLED;
    sigTime = GetAdjustedTime();
    lastPing = CMaxnodePing();
    cacheInputAge = 0;
    cacheInputAgeBlock = 0;
    unitTest = false;
    allowFreeTx = true;
    protocolVersion = protocolVersionIn;
    nLastDsq = 0;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
}

CMaxnodeBroadcast::CMaxnodeBroadcast(const CMaxnode& max)
{
    maxvin = max.maxvin;
    addr = max.addr;
    pubKeyCollateralAddress = max.pubKeyCollateralAddress;
    pubKeyMaxnode = max.pubKeyMaxnode;
    sig = max.sig;
    activeState = max.activeState;
    sigTime = max.sigTime;
    lastPing = max.lastPing;
    cacheInputAge = max.cacheInputAge;
    cacheInputAgeBlock = max.cacheInputAgeBlock;
    unitTest = max.unitTest;
    allowFreeTx = max.allowFreeTx;
    protocolVersion = max.protocolVersion;
    nLastDsq = max.nLastDsq;
    nScanningErrorCount = max.nScanningErrorCount;
    nLastScanningErrorBlockHeight = max.nLastScanningErrorBlockHeight;
}

bool CMaxnodeBroadcast::Create(std::string strService, std::string strKeyMaxnode, std::string strTxHash, std::string strOutputIndex, std::string& strErrorRet, CMaxnodeBroadcast& maxbRet, bool fOffline)
{
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyMaxnodeNew;
    CKey keyMaxnodeNew;

    //need correct blocks to send ping
    if (!fOffline && !maxnodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Maxnode";
        LogPrint("maxnode","CMaxnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!obfuScationSigner.GetKeysFromSecret(strKeyMaxnode, keyMaxnodeNew, pubKeyMaxnodeNew)) {
        strErrorRet = strprintf("Invalid maxnode key %s", strKeyMaxnode);
        LogPrint("maxnode","CMaxnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!pwalletMain->GetMaxnodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for maxnode %s", strTxHash, strOutputIndex, strService);
        LogPrint("maxnode","CMaxnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    // The service needs the correct default port to work properly
    if(!CheckDefaultPort(strService, strErrorRet, "CMaxnodeBroadcast::Create"))
        return false;

    return Create(txin, CService(strService), keyCollateralAddressNew, pubKeyCollateralAddressNew, keyMaxnodeNew, pubKeyMaxnodeNew, strErrorRet, maxbRet);
}

bool CMaxnodeBroadcast::Create(CTxIn txin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyMaxnodeNew, CPubKey pubKeyMaxnodeNew, std::string& strErrorRet, CMaxnodeBroadcast& maxbRet)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    LogPrint("maxnode", "CMaxnodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyMaxnodeNew.GetID() = %s\n",
        CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
        pubKeyMaxnodeNew.GetID().ToString());

    CMaxnodePing maxp(txin);
    if (!maxp.Sign(keyMaxnodeNew, pubKeyMaxnodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, maxnode=%s", txin.prevout.hash.ToString());
        LogPrint("maxnode","CMaxnodeBroadcast::Create -- %s\n", strErrorRet);
        maxbRet = CMaxnodeBroadcast();
        return false;
    }

    maxbRet = CMaxnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyMaxnodeNew, PROTOCOL_VERSION);

    if (!maxbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address %s, maxnode=%s", maxbRet.addr.ToStringIP (), txin.prevout.hash.ToString());
        LogPrint("maxnode","CMaxnodeBroadcast::Create -- %s\n", strErrorRet);
        maxbRet = CMaxnodeBroadcast();
        return false;
    }

    maxbRet.lastPing = maxp;
    if (!maxbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, maxnode=%s", txin.prevout.hash.ToString());
        LogPrint("maxnode","CMaxnodeBroadcast::Create -- %s\n", strErrorRet);
        maxbRet = CMaxnodeBroadcast();
        return false;
    }

    return true;
}

bool CMaxnodeBroadcast::CheckDefaultPort(std::string strService, std::string& strErrorRet, std::string strContext)
{
    CService service = CService(strService);
    int nDefaultPort = Params().GetDefaultPort();

    if (service.GetPort() != nDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for maxnode %s, only %d is supported on %s-net.",
                                        service.GetPort(), strService, nDefaultPort, Params().NetworkIDString());
        LogPrint("maxnode", "%s - %s\n", strContext, strErrorRet);
        return false;
    }

    return true;
}

bool CMaxnodeBroadcast::CheckAndUpdate(int& nDos)
{
    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrint("maxnode","maxb - Signature rejected, too far into the future %s\n", maxvin.prevout.hash.ToString());
        nDos = 1;
        return false;
    }

    // incorrect ping or its sigTime
    if(lastPing == CMaxnodePing() || !lastPing.CheckAndUpdate(nDos, false, true))
    return false;

    if (protocolVersion < maxnodePayments.GetMinMaxnodePaymentsProto()) {
        LogPrint("maxnode","maxb - ignoring outdated Maxnode %s protocol version %d\n", maxvin.prevout.hash.ToString(), protocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrint("maxnode","maxb - pubkey the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyMaxnode.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrint("maxnode","maxb - pubkey2 the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!maxvin.scriptSig.empty()) {
        LogPrint("maxnode","maxb - Ignore Not Empty ScriptSig %s\n", maxvin.prevout.hash.ToString());
        return false;
    }

    std::string errorMessage = "";
    if (!obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, sig, GetNewStrMessage(), errorMessage)
    		&& !obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, sig, GetOldStrMessage(), errorMessage))
    {
        // don't ban for old maxnodes, their sigs could be broken because of the bug
        nDos = protocolVersion < MIN_PEER_MNANNOUNCE ? 0 : 100;
        return error("CMaxnodeBroadcast::CheckAndUpdate - Got bad Maxnode address signature : %s", errorMessage);
    }

    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != 27071) return false;
    } else if (addr.GetPort() == 27071)
        return false;

    //search existing Maxnode list, this is where we update existing Maxnodes with new maxb broadcasts
    CMaxnode* pmax = maxnodeman.Find(maxvin);

    // no such maxnode, nothing to update
    if (pmax == NULL) return true;

    // this broadcast is older or equal than the one that we already have - it's bad and should never happen
	// unless someone is doing something fishy
	// (mapSeenMaxnodeBroadcast in CMaxnodeMan::ProcessMessage should filter legit duplicates)
	if(pmax->sigTime >= sigTime) {
		return error("CMaxnodeBroadcast::CheckAndUpdate - Bad sigTime %d for Maxnode %20s %105s (existing broadcast is at %d)",
					  sigTime, addr.ToString(), maxvin.ToString(), pmax->sigTime);
    }

    // maxnode is not enabled yet/already, nothing to update
    if (!pmax->IsEnabled()) return true;

    // max.pubkey = pubkey, IsVinAssociatedWithPubkey is validated once below,
    //   after that they just need to match
    if (pmax->pubKeyCollateralAddress == pubKeyCollateralAddress && !pmax->IsBroadcastedWithin(MAXNODE_MIN_MAXB_SECONDS)) {
        //take the newest entry
        LogPrint("maxnode","maxb - Got updated entry for %s\n", maxvin.prevout.hash.ToString());
        if (pmax->UpdateFromNewBroadcast((*this))) {
            pmax->Check();
            if (pmax->IsEnabled()) Relay();
        }
        maxnodeSync.AddedMaxnodeList(GetHash());
    }

    return true;
}

bool CMaxnodeBroadcast::CheckInputsAndAdd(int& nDoS)
{
    // we are a maxnode with the same maxvin (i.e. already activated) and this maxb is ours (matches our Maxnode privkey)
    // so nothing to do here for us
    //if ((fMaxNodeT1 || fMaxNodeT2 || fMaxNodeT3) && maxvin.prevout == activeMaxnode.maxvin.prevout && pubKeyMaxnode == activeMaxnode.pubKeyMaxnode)
    if ((fMaxNode) && maxvin.prevout == activeMaxnode.maxvin.prevout && pubKeyMaxnode == activeMaxnode.pubKeyMaxnode)
        return true;

    // incorrect ping or its sigTime
    if(lastPing == CMaxnodePing() || !lastPing.CheckAndUpdate(nDoS, false, true)) return false;

    // search existing Maxnode list
    CMaxnode* pmax = maxnodeman.Find(maxvin);

    if (pmax != NULL) {
        // nothing to do here if we already know about this maxnode and it's enabled
        if (pmax->IsEnabled()) return true;
        // if it's not enabled, remove old MN first and continue
        else
            maxnodeman.Remove(pmax->maxvin);
    }

    CValidationState state;
    CMutableTransaction tx = CMutableTransaction();
    //CTxOut vout = CTxOut((((MAXNODE_T1_COLLATERAL_AMOUNT - 0.01) || (MAXNODE_T2_COLLATERAL_AMOUNT - 0.01) || (MAXNODE_T3_COLLATERAL_AMOUNT - 0.01)) * COIN), obfuScationPool.collateralPubKey);
    CTxOut vout = CTxOut((MAXNODE_COLLATERAL_AMOUNT - 0.01) * COIN, obfuScationPool.collateralPubKey);
    tx.vin.push_back(maxvin);
    tx.vout.push_back(vout);

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            // not maxb fault, let it to be checked again later
            maxnodeman.mapSeenMaxnodeBroadcast.erase(GetHash());
            maxnodeSync.mapSeenSyncMAXB.erase(GetHash());
            return false;
        }

        if (!AcceptableInputs(mempool, state, CTransaction(tx), false, NULL)) {
            //set nDos
            state.IsInvalid(nDoS);
            return false;
        }
    }

    LogPrint("maxnode", "maxb - Accepted Maxnode entry\n");

    if (GetInputAge(maxvin) < MAXNODE_MIN_CONFIRMATIONS) {
        LogPrint("maxnode","maxb - Input must have at least %d confirmations\n", MAXNODE_MIN_CONFIRMATIONS);
        // maybe we miss few blocks, let this maxb to be checked again later
        maxnodeman.mapSeenMaxnodeBroadcast.erase(GetHash());
        maxnodeSync.mapSeenSyncMAXB.erase(GetHash());
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 1000 LYTX tx got MAXNODE_MIN_CONFIRMATIONS
    uint256 hashBlock = 0;
    CTransaction tx2;
    GetTransaction(maxvin.prevout.hash, tx2, hashBlock, true);
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi != mapBlockIndex.end() && (*mi).second) {
        CBlockIndex* pMNIndex = (*mi).second;                                                        // block for 1000 LYTX tx -> 1 confirmation
        CBlockIndex* pConfIndex = chainActive[pMNIndex->nHeight + MAXNODE_MIN_CONFIRMATIONS - 1]; // block where tx got MAXNODE_MIN_CONFIRMATIONS
        if (pConfIndex->GetBlockTime() > sigTime) {
            LogPrint("maxnode","maxb - Bad sigTime %d for Maxnode %s (%i conf block is at %d)\n",
                sigTime, maxvin.prevout.hash.ToString(), MAXNODE_MIN_CONFIRMATIONS, pConfIndex->GetBlockTime());
            return false;
        }
    }

    LogPrint("maxnode","maxb - Got NEW Maxnode entry - %s - %lli \n", maxvin.prevout.hash.ToString(), sigTime);
    CMaxnode max(*this);
    maxnodeman.Add(max);

    // if it matches our Maxnode privkey, then we've been remotely activated
    if (pubKeyMaxnode == activeMaxnode.pubKeyMaxnode && protocolVersion == PROTOCOL_VERSION) {
        activeMaxnode.EnableHotColdMaxNode(maxvin, addr);
    }

    bool isLocal = addr.IsRFC1918() || addr.IsLocal();
    if (Params().NetworkID() == CBaseChainParams::REGTEST) isLocal = false;

    if (!isLocal) Relay();

    return true;
}

void CMaxnodeBroadcast::Relay()
{
    CInv inv(MSG_MAXNODE_ANNOUNCE, GetHash());
    RelayInv(inv);
}

bool CMaxnodeBroadcast::Sign(CKey& keyCollateralAddress)
{
    std::string errorMessage;
    sigTime = GetAdjustedTime();

    std::string strMessage;
    if(chainActive.Height() < Params().Zerocoin_Block_V2_Start())
    	strMessage = GetOldStrMessage();
    else
    	strMessage = GetNewStrMessage();

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, sig, keyCollateralAddress))
    	return error("CMaxnodeBroadcast::Sign() - Error: %s", errorMessage);

    if (!obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, sig, strMessage, errorMessage))
    	return error("CMaxnodeBroadcast::Sign() - Error: %s", errorMessage);

    return true;
}


bool CMaxnodeBroadcast::VerifySignature()
{
    std::string errorMessage;

    if(!obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, sig, GetNewStrMessage(), errorMessage)
            && !obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, sig, GetOldStrMessage(), errorMessage))
        return error("CMaxnodeBroadcast::VerifySignature() - Error: %s", errorMessage);

    return true;
}

std::string CMaxnodeBroadcast::GetOldStrMessage()
{
    std::string strMessage;

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyMaxnode.begin(), pubKeyMaxnode.end());
    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(protocolVersion);

    return strMessage;
}

std:: string CMaxnodeBroadcast::GetNewStrMessage()
{
    std::string strMessage;

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + pubKeyCollateralAddress.GetID().ToString() + pubKeyMaxnode.GetID().ToString() + boost::lexical_cast<std::string>(protocolVersion);

    return strMessage;
}

CMaxnodePing::CMaxnodePing()
{
    maxvin = CTxIn();
    blockHash = uint256(0);
    sigTime = 0;
    vchSig = std::vector<unsigned char>();
}

CMaxnodePing::CMaxnodePing(CTxIn& newVin)
{
    maxvin = newVin;
    blockHash = chainActive[chainActive.Height() - 12]->GetMaxBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector<unsigned char>();
}


bool CMaxnodePing::Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode)
{
    std::string errorMessage;
    std::string strMaxNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = maxvin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyMaxnode)) {
        LogPrint("maxnode","CMaxnodePing::Sign() - Error: %s\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyMaxnode, vchSig, strMessage, errorMessage)) {
        LogPrint("maxnode","CMaxnodePing::Sign() - Error: %s\n", errorMessage);
        return false;
    }

    return true;
}

bool CMaxnodePing::VerifySignature(CPubKey& pubKeyMaxnode, int &nDos) {
	std::string strMessage = maxvin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
	std::string errorMessage = "";

	if(!obfuScationSigner.VerifyMessage(pubKeyMaxnode, vchSig, strMessage, errorMessage)){
		nDos = 33;
		return error("CMaxnodePing::VerifySignature - Got bad Maxnode ping signature %s Error: %s", maxvin.ToString(), errorMessage);
	}
	return true;
}

bool CMaxnodePing::CheckAndUpdate(int& nDos, bool fRequireEnabled, bool fCheckSigTimeOnly)
{
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrint("maxnode","CMaxnodePing::CheckAndUpdate - Signature rejected, too far into the future %s\n", maxvin.prevout.hash.ToString());
        nDos = 1;
        return false;
    }

    if (sigTime <= GetAdjustedTime() - 60 * 60) {
        LogPrint("maxnode","CMaxnodePing::CheckAndUpdate - Signature rejected, too far into the past %s - %d %d \n", maxvin.prevout.hash.ToString(), sigTime, GetAdjustedTime());
        nDos = 1;
        return false;
    }

    if(fCheckSigTimeOnly) {
    	CMaxnode* pmax = maxnodeman.Find(maxvin);
    	if(pmax) return VerifySignature(pmax->pubKeyMaxnode, nDos);
    	return true;
    }

    LogPrint("maxnode", "CMaxnodePing::CheckAndUpdate - New Ping - %s - %s - %lli\n", GetHash().ToString(), blockHash.ToString(), sigTime);

    // see if we have this Maxnode
    CMaxnode* pmax = maxnodeman.Find(maxvin);
    if (pmax != NULL && pmax->protocolVersion >= maxnodePayments.GetMinMaxnodePaymentsProto()) {
        if (fRequireEnabled && !pmax->IsEnabled()) return false;

        // LogPrint("maxnode","maxping - Found corresponding maxnode for maxvin: %s\n", maxvin.ToString());
        // update only if there is no known ping for this maxnode or
        // last ping was more then MAXNODE_MIN_MAXP_SECONDS-60 ago comparing to this one
        if (!pmax->IsPingedWithin(MAXNODE_MIN_MAXP_SECONDS - 60, sigTime)) {
        	if (!VerifySignature(pmax->pubKeyMaxnode, nDos))
                return false;

            BlockMap::iterator mi = mapBlockIndex.find(blockHash);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                if ((*mi).second->nHeight < chainActive.Height() - 24) {
                    LogPrint("maxnode","CMaxnodePing::CheckAndUpdate - Maxnode %s block hash %s is too old\n", maxvin.prevout.hash.ToString(), blockHash.ToString());
                    // Do nothing here (no Maxnode update, no maxping relay)
                    // Let this node to be visible but fail to accept maxping

                    return false;
                }
            } else {
                if (fDebug) LogPrint("maxnode","CMaxnodePing::CheckAndUpdate - Maxnode %s block hash %s is unknown\n", maxvin.prevout.hash.ToString(), blockHash.ToString());
                // maybe we stuck so we shouldn't ban this node, just fail to accept it
                // TODO: or should we also request this block?

                return false;
            }

            pmax->lastPing = *this;

            //maxnodeman.mapSeenMaxnodeBroadcast.lastPing is probably outdated, so we'll update it
            CMaxnodeBroadcast maxb(*pmax);
            uint256 hash = maxb.GetHash();
            if (maxnodeman.mapSeenMaxnodeBroadcast.count(hash)) {
                maxnodeman.mapSeenMaxnodeBroadcast[hash].lastPing = *this;
            }

            pmax->Check(true);
            if (!pmax->IsEnabled()) return false;

            LogPrint("maxnode", "CMaxnodePing::CheckAndUpdate - Maxnode ping accepted, maxvin: %s\n", maxvin.prevout.hash.ToString());

            Relay();
            return true;
        }
        LogPrint("maxnode", "CMaxnodePing::CheckAndUpdate - Maxnode ping arrived too early, maxvin: %s\n", maxvin.prevout.hash.ToString());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }
    LogPrint("maxnode", "CMaxnodePing::CheckAndUpdate - Couldn't find compatible Maxnode entry, maxvin: %s\n", maxvin.prevout.hash.ToString());

    return false;
}

void CMaxnodePing::Relay()
{
    CInv inv(MSG_MAXNODE_PING, GetHash());
    RelayInv(inv);
}
