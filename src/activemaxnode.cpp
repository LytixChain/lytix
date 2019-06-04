// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2019 The Lytix developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemaxnode.h"
#include "addrman.h"
#include "maxnode.h"
#include "maxnodeconfig.h"
#include "maxnodeman.h"
#include "protocol.h"
#include "spork.h"

//
// Bootup the Maxnode, look for a 50000 LYTX input and register on the network
//
void CActiveMaxnode::ManageStatus()
{
    std::string errorMessage;

    //if (!fMaxNodeT1 || !fMaxNodeT2 || !fMaxNodeT3) return;
    if (!fMaxNode) return;

    if (fDebug) LogPrintf("CActiveMaxnode::ManageStatus() - Begin\n");

    //need correct blocks to send ping
    if (Params().NetworkID() != CBaseChainParams::REGTEST && !maxnodeSync.IsBlockchainSynced()) {
        status = ACTIVE_MAXNODE_SYNC_IN_PROCESS;
        LogPrintf("CActiveMaxnode::ManageStatus() - %s\n", GetStatus());
        return;
    }

    if (status == ACTIVE_MAXNODE_SYNC_IN_PROCESS) status = ACTIVE_MAXNODE_INITIAL;

    if (status == ACTIVE_MAXNODE_INITIAL) {
        CMaxnode* pmax;
        pmax = maxnodeman.Find(pubKeyMaxnode);
        if (pmax != NULL) {
            pmax->Check();
            if (pmax->IsEnabled() && pmax->protocolVersion == PROTOCOL_VERSION) EnableHotColdMaxNode(pmax->maxvin, pmax->addr);
        }
    }

    if (status != ACTIVE_MAXNODE_STARTED) {
        // Set defaults
        status = ACTIVE_MAXNODE_NOT_CAPABLE;
        notCapableReason = "";

        if (pwalletMain->IsLocked()) {
            notCapableReason = "Wallet is locked.";
            LogPrintf("CActiveMaxnode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }

        if (pwalletMain->GetBalance() == 0) {
            notCapableReason = "Hot node, waiting for remote activation.";
            LogPrintf("CActiveMaxnode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }

        if (strMaxNodeAddr.empty()) {
            if (!GetLocal(service)) {
                notCapableReason = "Can't detect external address. Please use the maxnodeaddr configuration option.";
                LogPrintf("CActiveMaxnode::ManageStatus() - not capable: %s\n", notCapableReason);
                return;
            }
        } else {
            service = CService(strMaxNodeAddr);
        }

        // The service needs the correct default port to work properly
        if(!CMaxnodeBroadcast::CheckDefaultPort(strMaxNodeAddr, errorMessage, "CActiveMaxnode::ManageStatus()"))
            return;

        LogPrintf("CActiveMaxnode::ManageStatus() - Checking inbound connection to '%s'\n", service.ToString());

        CNode* pnode = ConnectNode((CAddress)service, NULL, false);
        if (!pnode) {
            notCapableReason = "Could not connect to " + service.ToString();
            LogPrintf("CActiveMaxnode::ManageStatus() - not capable: %s\n", notCapableReason);
            return;
        }
        pnode->Release();

        // Choose coins to use
        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;

        if (GetMaxNodeVin(maxvin, pubKeyCollateralAddress, keyCollateralAddress)) {
            if (GetInputAge(maxvin) < MAXNODE_MIN_CONFIRMATIONS) {
                status = ACTIVE_MAXNODE_INPUT_TOO_NEW;
                notCapableReason = strprintf("%s - %d confirmations", GetStatus(), GetInputAge(maxvin));
                LogPrintf("CActiveMaxnode::ManageStatus() - %s\n", notCapableReason);
                return;
            }

            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(maxvin.prevout);

            // send to all nodes
            CPubKey pubKeyMaxnode;
            CKey keyMaxnode;

            if (!obfuScationSigner.SetKey(strMaxNodePrivKey, errorMessage, keyMaxnode, pubKeyMaxnode)) {
                notCapableReason = "Error upon calling SetKey: " + errorMessage;
                LogPrintf("Register::ManageStatus() - %s\n", notCapableReason);
                return;
            }

            CMaxnodeBroadcast maxb;
            if (!CreateBroadcast(maxvin, service, keyCollateralAddress, pubKeyCollateralAddress, keyMaxnode, pubKeyMaxnode, errorMessage, maxb)) {
                notCapableReason = "Error on Register: " + errorMessage;
                LogPrintf("CActiveMaxnode::ManageStatus() - %s\n", notCapableReason);
                return;
            }

            //send to all peers
            LogPrintf("CActiveMaxnode::ManageStatus() - Relay broadcast maxvin = %s\n", maxvin.ToString());
            maxb.Relay();

            LogPrintf("CActiveMaxnode::ManageStatus() - Is capable max node!\n");
            status = ACTIVE_MAXNODE_STARTED;

            return;
        } else {
            notCapableReason = "Could not find suitable coins!";
            LogPrintf("CActiveMaxnode::ManageStatus() - %s\n", notCapableReason);
            return;
        }
    }

    //send to all peers
    if (!SendMaxnodePing(errorMessage)) {
        LogPrintf("CActiveMaxnode::ManageStatus() - Error on Ping: %s\n", errorMessage);
    }
}

std::string CActiveMaxnode::GetStatus()
{
    switch (status) {
    case ACTIVE_MAXNODE_INITIAL:
        return "Node just started, not yet activated";
    case ACTIVE_MAXNODE_SYNC_IN_PROCESS:
        return "Sync in progress. Must wait until sync is complete to start Maxnode";
    case ACTIVE_MAXNODE_INPUT_TOO_NEW:
        return strprintf("Maxnode input must have at least %d confirmations", MAXNODE_MIN_CONFIRMATIONS);
    case ACTIVE_MAXNODE_NOT_CAPABLE:
        return "Not capable maxnode: " + notCapableReason;
    case ACTIVE_MAXNODE_STARTED:
        return "Maxnode successfully started";
    default:
        return "unknown";
    }
}

bool CActiveMaxnode::SendMaxnodePing(std::string& errorMessage)
{
    if (status != ACTIVE_MAXNODE_STARTED) {
        errorMessage = "Maxnode is not in a running status";
        return false;
    }

    CPubKey pubKeyMaxnode;
    CKey keyMaxnode;

    if (!obfuScationSigner.SetKey(strMaxNodePrivKey, errorMessage, keyMaxnode, pubKeyMaxnode)) {
        errorMessage = strprintf("Error upon calling SetKey: %s\n", errorMessage);
        return false;
    }

    LogPrintf("CActiveMaxnode::SendMaxnodePing() - Relay Maxnode Ping maxvin = %s\n", maxvin.ToString());

    CMaxnodePing maxp(maxvin);
    if (!maxp.Sign(keyMaxnode, pubKeyMaxnode)) {
        errorMessage = "Couldn't sign Maxnode Ping";
        return false;
    }

    // Update lastPing for our maxnode in Maxnode list
    CMaxnode* pmax = maxnodeman.Find(maxvin);
    if (pmax != NULL) {
        if (pmax->IsPingedWithin(MAXNODE_PING_SECONDS, maxp.sigTime)) {
            errorMessage = "Too early to send Maxnode Ping";
            return false;
        }

        pmax->lastPing = maxp;
        maxnodeman.mapSeenMaxnodePing.insert(make_pair(maxp.GetHash(), maxp));

        //maxnodeman.mapSeenMaxnodeBroadcast.lastPing is probably outdated, so we'll update it
        CMaxnodeBroadcast maxb(*pmax);
        uint256 hash = maxb.GetHash();
        if (maxnodeman.mapSeenMaxnodeBroadcast.count(hash)) maxnodeman.mapSeenMaxnodeBroadcast[hash].lastPing = maxp;

        maxp.Relay();

        /*
         * IT'S SAFE TO REMOVE THIS IN FURTHER VERSIONS
         * AFTER MIGRATION TO V12 IS DONE
         */

        if (IsSporkActive(SPORK_10_MASTERNODE_PAY_UPDATED_NODES)) return true;
        // for migration purposes ping our node on old maxnodes network too
        std::string retErrorMessage;
        std::vector<unsigned char> vchMaxNodeSignature;
        int64_t maxNodeSignatureTime = GetAdjustedTime();

        std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(maxNodeSignatureTime) + boost::lexical_cast<std::string>(false);

        if (!obfuScationSigner.SignMessage(strMessage, retErrorMessage, vchMaxNodeSignature, keyMaxnode)) {
            errorMessage = "dmaxseep sign message failed: " + retErrorMessage;
            return false;
        }

        if (!obfuScationSigner.VerifyMessage(pubKeyMaxnode, vchMaxNodeSignature, strMessage, retErrorMessage)) {
            errorMessage = "dmaxseep verify message failed: " + retErrorMessage;
            return false;
        }

        LogPrint("maxnode", "dmaxseep - relaying from active max, %s \n", maxvin.ToString().c_str());
        LOCK(cs_vNodes);
        BOOST_FOREACH (CNode* pnode, vNodes)
            pnode->PushMessage("dmaxseep", maxvin, vchMaxNodeSignature, maxNodeSignatureTime, false);

        /*
         * END OF "REMOVE"
         */

        return true;
    } else {
        // Seems like we are trying to send a ping while the Maxnode is not registered in the network
        errorMessage = "Obfuscation Maxnode List doesn't include our Maxnode, shutting down Maxnode pinging service! " + maxvin.ToString();
        status = ACTIVE_MAXNODE_NOT_CAPABLE;
        notCapableReason = errorMessage;
        return false;
    }
}

bool CActiveMaxnode::CreateBroadcast(std::string strService, std::string strKeyMaxnode, std::string strTxHash, std::string strOutputIndex, std::string& errorMessage, CMaxnodeBroadcast &maxb, bool fOffline)
{
    CTxIn maxvin;
    CPubKey pubKeyCollateralAddress;
    CKey keyCollateralAddress;
    CPubKey pubKeyMaxnode;
    CKey keyMaxnode;

    //need correct blocks to send ping
    if (!fOffline && !maxnodeSync.IsBlockchainSynced()) {
        errorMessage = "Sync in progress. Must wait until sync is complete to start Maxnode";
        LogPrintf("CActiveMaxnode::CreateBroadcast() - %s\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.SetKey(strKeyMaxnode, errorMessage, keyMaxnode, pubKeyMaxnode)) {
        errorMessage = strprintf("Can't find keys for maxnode %s - %s", strService, errorMessage);
        LogPrintf("CActiveMaxnode::CreateBroadcast() - %s\n", errorMessage);
        return false;
    }

    if (!GetMaxNodeVin(maxvin, pubKeyCollateralAddress, keyCollateralAddress, strTxHash, strOutputIndex)) {
        errorMessage = strprintf("Could not allocate maxvin %s:%s for maxnode %s", strTxHash, strOutputIndex, strService);
        LogPrintf("CActiveMaxnode::CreateBroadcast() - %s\n", errorMessage);
        return false;
    }

    CService service = CService(strService);

    // The service needs the correct default port to work properly
    if(!CMaxnodeBroadcast::CheckDefaultPort(strService, errorMessage, "CActiveMaxnode::CreateBroadcast()"))
        return false;

    addrman.Add(CAddress(service), CNetAddr("127.0.0.1"), 2 * 60 * 60);

    return CreateBroadcast(maxvin, CService(strService), keyCollateralAddress, pubKeyCollateralAddress, keyMaxnode, pubKeyMaxnode, errorMessage, maxb);
}

bool CActiveMaxnode::CreateBroadcast(CTxIn maxvin, CService service, CKey keyCollateralAddress, CPubKey pubKeyCollateralAddress, CKey keyMaxnode, CPubKey pubKeyMaxnode, std::string& errorMessage, CMaxnodeBroadcast &maxb)
{
	// wait for reindex and/or import to finish
	if (fImporting || fReindex) return false;

    CMaxnodePing maxp(maxvin);
    if (!maxp.Sign(keyMaxnode, pubKeyMaxnode)) {
        errorMessage = strprintf("Failed to sign ping, maxvin: %s", maxvin.ToString());
        LogPrintf("CActiveMaxnode::CreateBroadcast() -  %s\n", errorMessage);
        maxb = CMaxnodeBroadcast();
        return false;
    }

    maxb = CMaxnodeBroadcast(service, maxvin, pubKeyCollateralAddress, pubKeyMaxnode, PROTOCOL_VERSION);
    maxb.lastPing = maxp;
    if (!maxb.Sign(keyCollateralAddress)) {
        errorMessage = strprintf("Failed to sign broadcast, maxvin: %s", maxvin.ToString());
        LogPrintf("CActiveMaxnode::CreateBroadcast() - %s\n", errorMessage);
        maxb = CMaxnodeBroadcast();
        return false;
    }

    /*
     * IT'S SAFE TO REMOVE THIS IN FURTHER VERSIONS
     * AFTER MIGRATION TO V12 IS DONE
     */

    if (IsSporkActive(SPORK_10_MASTERNODE_PAY_UPDATED_NODES)) return true;
    // for migration purposes inject our node in old maxnodes' list too
    std::string retErrorMessage;
    std::vector<unsigned char> vchMaxNodeSignature;
    int64_t maxNodeSignatureTime = GetAdjustedTime();
    std::string donationAddress = "";
    int donationPercantage = 0;

    std::string vchPubKey(pubKeyCollateralAddress.begin(), pubKeyCollateralAddress.end());
    std::string vchPubKey2(pubKeyMaxnode.begin(), pubKeyMaxnode.end());

    std::string strMessage = service.ToString() + boost::lexical_cast<std::string>(maxNodeSignatureTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(PROTOCOL_VERSION) + donationAddress + boost::lexical_cast<std::string>(donationPercantage);

    if (!obfuScationSigner.SignMessage(strMessage, retErrorMessage, vchMaxNodeSignature, keyCollateralAddress)) {
        errorMessage = "dmaxsee sign message failed: " + retErrorMessage;
        LogPrintf("CActiveMaxnode::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyCollateralAddress, vchMaxNodeSignature, strMessage, retErrorMessage)) {
        errorMessage = "dmaxsee verify message failed: " + retErrorMessage;
        LogPrintf("CActiveMaxnode::Register() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    LOCK(cs_vNodes);
    BOOST_FOREACH (CNode* pnode, vNodes)
        pnode->PushMessage("dmaxsee", maxvin, service, vchMaxNodeSignature, maxNodeSignatureTime, pubKeyCollateralAddress, pubKeyMaxnode, -1, -1, maxNodeSignatureTime, PROTOCOL_VERSION, donationAddress, donationPercantage);

    /*
     * END OF "REMOVE"
     */

    return true;
}

bool CActiveMaxnode::GetMaxNodeVin(CTxIn& maxvin, CPubKey& pubkey, CKey& secretKey)
{
    return GetMaxNodeVin(maxvin, pubkey, secretKey, "", "");
}

bool CActiveMaxnode::GetMaxNodeVin(CTxIn& maxvin, CPubKey& pubkey, CKey& secretKey, std::string strTxHash, std::string strOutputIndex)
{
	// wait for reindex and/or import to finish
	if (fImporting || fReindex) return false;

    // Find possible candidates
    TRY_LOCK(pwalletMain->cs_wallet, fWallet);
    if (!fWallet) return false;

    vector<COutput> possibleCoins = SelectCoinsMaxnode();
    COutput* selectedOutput;

    // Find the maxvin
    if (!strTxHash.empty()) {
        // Let's find it
        uint256 txHash(strTxHash);
        int outputIndex;
        try {
            outputIndex = std::stoi(strOutputIndex.c_str());
        } catch (const std::exception& e) {
            LogPrintf("%s: %s on strOutputIndex\n", __func__, e.what());
            return false;
        }

        bool found = false;
        BOOST_FOREACH (COutput& out, possibleCoins) {
            if (out.tx->GetHash() == txHash && out.i == outputIndex) {
                selectedOutput = &out;
                found = true;
                break;
            }
        }
        if (!found) {
            LogPrintf("CActiveMaxnode::GetMaxNodeVin - Could not locate valid maxvin\n");
            return false;
        }
    } else {
        // No output specified,  Select the first one
        if (possibleCoins.size() > 0) {
            selectedOutput = &possibleCoins[0];
        } else {
            LogPrintf("CActiveMaxnode::GetMaxNodeVin - Could not locate specified maxvin from possible list\n");
            return false;
        }
    }

    // At this point we have a selected output, retrieve the associated info
    return GetVinFromOutput(*selectedOutput, maxvin, pubkey, secretKey);
}


// Extract Maxnode maxvin information from output
bool CActiveMaxnode::GetVinFromOutput(COutput out, CTxIn& maxvin, CPubKey& pubkey, CKey& secretKey)
{
	// wait for reindex and/or import to finish
	if (fImporting || fReindex) return false;

    CScript pubScript;

    maxvin = CTxIn(out.tx->GetHash(), out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

    CTxDestination address1;
    ExtractDestination(pubScript, address1);
    CBitcoinAddress address2(address1);

    CKeyID keyID;
    if (!address2.GetKeyID(keyID)) {
        LogPrintf("CActiveMaxnode::GetMaxNodeVin - Address does not refer to a key\n");
        return false;
    }

    if (!pwalletMain->GetKey(keyID, secretKey)) {
        LogPrintf("CActiveMaxnode::GetMaxNodeVin - Private key for address is not known\n");
        return false;
    }

    pubkey = secretKey.GetPubKey();
    return true;
}

// get all possible outputs for running Maxnode
vector<COutput> CActiveMaxnode::SelectCoinsMaxnode()
{
    vector<COutput> vCoins;
    vector<COutput> filteredCoins;
    vector<COutPoint> confLockedCoins;

    // Temporary unlock MAX coins from maxnode.conf
    if (GetBoolArg("-maxconflock", true)) {
        uint256 maxTxHash;
        BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
            maxTxHash.SetHex(maxe.getTxHash());

            int nIndex;
            if(!maxe.castOutputIndex(nIndex))
                continue;

            COutPoint outpoint = COutPoint(maxTxHash, nIndex);
            confLockedCoins.push_back(outpoint);
            pwalletMain->UnlockCoin(outpoint);
        }
    }

    // Retrieve all possible outputs
    pwalletMain->AvailableCoins(vCoins);

    // Lock MAX coins from maxnode.conf back if they where temporary unlocked
    if (!confLockedCoins.empty()) {
        BOOST_FOREACH (COutPoint outpoint, confLockedCoins)
            pwalletMain->LockCoin(outpoint);
    }

    // Filter
    BOOST_FOREACH (const COutput& out, vCoins) {
        if (out.tx->vout[out.i].nValue == MAXNODE_COLLATERAL_AMOUNT * COIN) { //exactly
            filteredCoins.push_back(out);
        }
    }

    /**BOOST_FOREACH (const COutput& out, vCoins) {
        if (out.tx->vout[out.i].nValue == MAXNODE_T2_COLLATERAL_AMOUNT * COIN) { //exactly
            filteredCoins.push_back(out);
        }
    }

    BOOST_FOREACH (const COutput& out, vCoins) {
        if (out.tx->vout[out.i].nValue == MAXNODE_T3_COLLATERAL_AMOUNT * COIN) { //exactly
            filteredCoins.push_back(out);
        }
    }**/

    return filteredCoins;
}

// when starting a Maxnode, this can enable to run as a hot wallet with no funds
bool CActiveMaxnode::EnableHotColdMaxNode(CTxIn& newVin, CService& newService)
{
    //if (!fMaxNodeT1 || !fMaxNodeT2 || !fMaxNodeT3) return false;
    if (!fMaxNode) return false;

    status = ACTIVE_MAXNODE_STARTED;

    //The values below are needed for signing maxping messages going forward
    maxvin = newVin;
    service = newService;

    LogPrintf("CActiveMaxnode::EnableHotColdMaxNode() - Enabled! You may shut down the cold daemon.\n");

    return true;
}
