// Copyright (c) 2014-2015 The Dash Developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2019 The Lytix developer

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemaxnode.h"
#include "db.h"
#include "init.h"
#include "main.h"
#include "maxnode-budget.h"
#include "maxnode-payments.h"
#include "maxnodeconfig.h"
#include "maxnodeman.h"
#include "rpcserver.h"
#include "utilmoneystr.h"

#include <univalue.h>

#include <fstream>
using namespace std;

void mxbudgetToJSON(CMAXBudgetProposal* pmxbudgetProposal, UniValue& bObj)
{
    CTxDestination address1;
    ExtractDestination(pmxbudgetProposal->GetPayee(), address1);
    CBitcoinAddress address2(address1);

    bObj.push_back(Pair("Name", pmxbudgetProposal->GetName()));
    bObj.push_back(Pair("URL", pmxbudgetProposal->GetURL()));
    bObj.push_back(Pair("Hash", pmxbudgetProposal->GetHash().ToString()));
    bObj.push_back(Pair("FeeHash", pmxbudgetProposal->nFeeTXHash.ToString()));
    bObj.push_back(Pair("BlockStart", (int64_t)pmxbudgetProposal->GetMaxBlockStart()));
    bObj.push_back(Pair("BlockEnd", (int64_t)pmxbudgetProposal->GetMaxBlockEnd()));
    bObj.push_back(Pair("TotalPaymentCount", (int64_t)pmxbudgetProposal->GetMaxTotalPaymentCount()));
    bObj.push_back(Pair("RemainingPaymentCount", (int64_t)pmxbudgetProposal->GetMaxRemainingPaymentCount()));
    bObj.push_back(Pair("PaymentAddress", address2.ToString()));
    bObj.push_back(Pair("Ratio", pmxbudgetProposal->GetMaxRatio()));
    bObj.push_back(Pair("Yeas", (int64_t)pmxbudgetProposal->GetMaxYeas()));
    bObj.push_back(Pair("Nays", (int64_t)pmxbudgetProposal->GetMaxNays()));
    bObj.push_back(Pair("Abstains", (int64_t)pmxbudgetProposal->GetMaxAbstains()));
    bObj.push_back(Pair("TotalPayment", ValueFromAmount(pmxbudgetProposal->GetAmount() * pmxbudgetProposal->GetMaxTotalPaymentCount())));
    bObj.push_back(Pair("MonthlyPayment", ValueFromAmount(pmxbudgetProposal->GetAmount())));
    bObj.push_back(Pair("IsEstablished", pmxbudgetProposal->IsEstablished()));

    std::string strError = "";
    bObj.push_back(Pair("IsValid", pmxbudgetProposal->IsValid(strError)));
    bObj.push_back(Pair("IsValidReason", strError.c_str()));
    bObj.push_back(Pair("fValid", pmxbudgetProposal->fValid));
}

// This command is retained for backwards compatibility, but is depreciated.
// Future removal of this command is planned to keep things clean.
UniValue mxbudget(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "vote-alias" && strCommand != "vote-many" && strCommand != "prepare" && strCommand != "submit" && strCommand != "vote" && strCommand != "getvotes" && strCommand != "getinfo" && strCommand != "show" && strCommand != "projection" && strCommand != "check" && strCommand != "nextblock"))
        throw runtime_error(
            "mxbudget \"command\"... ( \"passphrase\" )\n"
            "\nVote or show current mxbudgets\n"
            "This command is depreciated, please see individual command documentation for future reference\n\n"

            "\nAvailable commands:\n"
            "  prepare            - Prepare proposal for network by signing and creating tx\n"
            "  submit             - Submit proposal for network\n"
            "  vote-many          - Vote on a Lytix initiative\n"
            "  vote-alias         - Vote on a Lytix initiative\n"
            "  vote               - Vote on a Lytix initiative/mxbudget\n"
            "  getvotes           - Show current maxnode mxbudgets\n"
            "  getinfo            - Show current maxnode mxbudgets\n"
            "  show               - Show all mxbudgets\n"
            "  projection         - Show the projection of which proposals will be paid the next cycle\n"
            "  check              - Scan proposals and remove invalid\n"
            "  nextblock          - Get next superblock for mxbudget system\n");

    if (strCommand == "nextblock") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getnextsuperblock(newParams, fHelp);
    }

    if (strCommand == "prepare") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return preparebudget(newParams, fHelp);
    }

    if (strCommand == "submit") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return submitbudget(newParams, fHelp);
    }

    if (strCommand == "vote" || strCommand == "vote-many" || strCommand == "vote-alias") {
        if (strCommand == "vote-alias")
            throw runtime_error(
                "vote-alias is not supported with this command\n"
                "Please use mxbudgetvote instead.\n"
            );
        return maxbudgetvote(params, fHelp);
    }

    if (strCommand == "projection") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getbudgetprojection(newParams, fHelp);
    }

    if (strCommand == "show" || strCommand == "getinfo") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getbudgetinfo(newParams, fHelp);
    }

    if (strCommand == "getvotes") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getbudgetvotes(newParams, fHelp);
    }

    if (strCommand == "check") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return checkbudgets(newParams, fHelp);
    }

    return NullUniValue;
}

UniValue preparebudget(const UniValue& params, bool fHelp)
{
    int nBlockMin = 0;
    CBlockIndex* pindexPrev = chainActive.Tip();

    if (fHelp || params.size() != 6)
        throw runtime_error(
            "preparebudget \"proposal-name\" \"url\" payment-count block-start \"address\" monthy-payment\n"
            "\nPrepare proposal for network by signing and creating tx\n"

            "\nArguments:\n"
            "1. \"proposal-name\":  (string, required) Desired proposal name (20 character limit)\n"
            "2. \"url\":            (string, required) URL of proposal details (64 character limit)\n"
            "3. payment-count:    (numeric, required) Total number of monthly payments\n"
            "4. block-start:      (numeric, required) Starting super block height\n"
            "5. \"address\":   (string, required) Lytix address to send payments to\n"
            "6. monthly-payment:  (numeric, required) Monthly payment amount\n"

            "\nResult:\n"
            "\"xxxx\"       (string) proposal fee hash (if successful) or error message (if failed)\n"

            "\nExamples:\n" +
            HelpExampleCli("preparebudget", "\"test-proposal\" \"https://forum.lytixchain.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500") +
            HelpExampleRpc("preparebudget", "\"test-proposal\" \"https://forum.lytixchain.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    std::string strProposalName = SanitizeString(params[0].get_str());
    if (strProposalName.size() > 20)
        throw runtime_error("Invalid proposal name, limit of 20 characters.");

    std::string strURL = SanitizeString(params[1].get_str());
    if (strURL.size() > 64)
        throw runtime_error("Invalid url, limit of 64 characters.");

    int nPaymentCount = params[2].get_int();
    if (nPaymentCount < 1)
        throw runtime_error("Invalid payment count, must be more than zero.");

    // Start must be in the next mxbudget cycle
    if (pindexPrev != NULL) nBlockMin = pindexPrev->nHeight - pindexPrev->nHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();

    int nBlockStart = params[3].get_int();
    if (nBlockStart % GetMaxBudgetPaymentCycleBlocks() != 0) {
        int nNext = pindexPrev->nHeight - pindexPrev->nHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();
        throw runtime_error(strprintf("Invalid block start - must be a mxbudget cycle block. Next valid block: %d", nNext));
    }

    int nBlockEnd = nBlockStart + GetMaxBudgetPaymentCycleBlocks() * nPaymentCount; // End must be AFTER current cycle

    if (nBlockStart < nBlockMin)
        throw runtime_error("Invalid block start, must be more than current height.");

    if (nBlockEnd < pindexPrev->nHeight)
        throw runtime_error("Invalid ending block, starting block + (payment_cycle*payments) must be more than current height.");

    CBitcoinAddress address(params[4].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Lytix address");

    // Parse Lytix address
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    CAmount nAmount = AmountFromValue(params[5]);

    //*************************************************************************

    // create transaction 15 minutes into the future, to allow for confirmation time
    CMAXBudgetProposalBroadcast mxbudgetProposalBroadcast(strProposalName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, 0);

    std::string strError = "";
    if (!mxbudgetProposalBroadcast.IsValid(strError, false))
        throw runtime_error("Proposal is not valid - " + mxbudgetProposalBroadcast.GetHash().ToString() + " - " + strError);

    bool useIX = false; //true;
    // if (params.size() > 7) {
    //     if(params[7].get_str() != "false" && params[7].get_str() != "true")
    //         return "Invalid use_ix, must be true or false";
    //     useIX = params[7].get_str() == "true" ? true : false;
    // }

    CWalletTx wtx;
    if (!pwalletMain->GetBudgetSystemCollateralTX(wtx, mxbudgetProposalBroadcast.GetHash(), useIX)) { // 50 PIV collateral for proposal
        throw runtime_error("Error making collateral transaction for proposal. Please check your wallet balance.");
    }

    // make our change address
    CReserveKey reservekey(pwalletMain);
    //send the tx to the network
    pwalletMain->CommitTransaction(wtx, reservekey, useIX ? "ix" : "tx");

    return wtx.GetHash().ToString();
}

UniValue submitbudget(const UniValue& params, bool fHelp)
{
    int nBlockMin = 0;
    CBlockIndex* pindexPrev = chainActive.Tip();

    if (fHelp || params.size() != 7)
        throw runtime_error(
            "submitbudget \"proposal-name\" \"url\" payment-count block-start \"address\" monthy-payment \"fee-tx\"\n"
            "\nSubmit proposal to the network\n"

            "\nArguments:\n"
            "1. \"proposal-name\":  (string, required) Desired proposal name (20 character limit)\n"
            "2. \"url\":            (string, required) URL of proposal details (64 character limit)\n"
            "3. payment-count:    (numeric, required) Total number of monthly payments\n"
            "4. block-start:      (numeric, required) Starting super block height\n"
            "5. \"address\":   (string, required) Lytix address to send payments to\n"
            "6. monthly-payment:  (numeric, required) Monthly payment amount\n"
            "7. \"fee-tx\":         (string, required) Transaction hash from preparebudget command\n"

            "\nResult:\n"
            "\"xxxx\"       (string) proposal hash (if successful) or error message (if failed)\n"

            "\nExamples:\n" +
            HelpExampleCli("submitbudget", "\"test-proposal\" \"https://forum.lytixchain.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500") +
            HelpExampleRpc("submitbudget", "\"test-proposal\" \"https://forum.lytixchain.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500"));

    // Check these inputs the same way we check the vote commands:
    // **********************************************************

    std::string strProposalName = SanitizeString(params[0].get_str());
    if (strProposalName.size() > 20)
        throw runtime_error("Invalid proposal name, limit of 20 characters.");

    std::string strURL = SanitizeString(params[1].get_str());
    if (strURL.size() > 64)
        throw runtime_error("Invalid url, limit of 64 characters.");

    int nPaymentCount = params[2].get_int();
    if (nPaymentCount < 1)
        throw runtime_error("Invalid payment count, must be more than zero.");

    // Start must be in the next mxbudget cycle
    if (pindexPrev != NULL) nBlockMin = pindexPrev->nHeight - pindexPrev->nHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();

    int nBlockStart = params[3].get_int();
    if (nBlockStart % GetMaxBudgetPaymentCycleBlocks() != 0) {
        int nNext = pindexPrev->nHeight - pindexPrev->nHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();
        throw runtime_error(strprintf("Invalid block start - must be a mxbudget cycle block. Next valid block: %d", nNext));
    }

    int nBlockEnd = nBlockStart + (GetMaxBudgetPaymentCycleBlocks() * nPaymentCount); // End must be AFTER current cycle

    if (nBlockStart < nBlockMin)
        throw runtime_error("Invalid block start, must be more than current height.");

    if (nBlockEnd < pindexPrev->nHeight)
        throw runtime_error("Invalid ending block, starting block + (payment_cycle*payments) must be more than current height.");

    CBitcoinAddress address(params[4].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Lytix address");

    // Parse Lytix address
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    CAmount nAmount = AmountFromValue(params[5]);
    uint256 hash = ParseHashV(params[6], "parameter 1");

    //create the proposal incase we're the first to make it
    CMAXBudgetProposalBroadcast mxbudgetProposalBroadcast(strProposalName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, hash);

    std::string strError = "";
    int nConf = 0;
    if (!IsMaxBudgetCollateralValid(hash, mxbudgetProposalBroadcast.GetHash(), strError, mxbudgetProposalBroadcast.nTime, nConf)) {
        throw runtime_error("Proposal FeeTX is not valid - " + hash.ToString() + " - " + strError);
    }

    if (!maxnodeSync.IsBlockchainSynced()) {
        throw runtime_error("Must wait for client to sync with maxnode network. Try again in a minute or so.");
    }

    // if(!mxbudgetProposalBroadcast.IsValid(strError)){
    //     return "Proposal is not valid - " + mxbudgetProposalBroadcast.GetHash().ToString() + " - " + strError;
    // }

    maxbudget.mapSeenMaxnodeBudgetProposals.insert(make_pair(mxbudgetProposalBroadcast.GetHash(), mxbudgetProposalBroadcast));
    mxbudgetProposalBroadcast.Relay();
    if(maxbudget.AddProposal(mxbudgetProposalBroadcast)) {
        return mxbudgetProposalBroadcast.GetHash().ToString();
    }
    throw runtime_error("Invalid proposal, see debug.log for details.");
}

UniValue mxbudgetvote(const UniValue& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1) {
        strCommand = params[0].get_str();

        // Backwards compatibility with legacy `mxbudget` command
        if (strCommand == "vote") strCommand = "local";
        if (strCommand == "vote-many") strCommand = "many";
        if (strCommand == "vote-alias") strCommand = "alias";
    }

    if (fHelp || (params.size() == 3 && (strCommand != "local" && strCommand != "many")) || (params.size() == 4 && strCommand != "alias") ||
        params.size() > 4 || params.size() < 3)
        throw runtime_error(
            "mxbudgetvote \"local|many|alias\" \"votehash\" \"yes|no\" ( \"alias\" )\n"
            "\nVote on a mxbudget proposal\n"

            "\nArguments:\n"
            "1. \"mode\"      (string, required) The voting mode. 'local' for voting directly from a maxnode, 'many' for voting with a MN controller and casting the same vote for each MN, 'alias' for voting with a MN controller and casting a vote for a single MN\n"
            "2. \"votehash\"  (string, required) The vote hash for the proposal\n"
            "3. \"votecast\"  (string, required) Your vote. 'yes' to vote for the proposal, 'no' to vote against\n"
            "4. \"alias\"     (string, required for 'alias' mode) The MN alias to cast a vote for.\n"

            "\nResult:\n"
            "{\n"
            "  \"overall\": \"xxxx\",      (string) The overall status message for the vote cast\n"
            "  \"detail\": [\n"
            "    {\n"
            "      \"node\": \"xxxx\",      (string) 'local' or the MN alias\n"
            "      \"result\": \"xxxx\",    (string) Either 'Success' or 'Failed'\n"
            "      \"error\": \"xxxx\",     (string) Error message, if vote failed\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("mxbudgetvote", "\"local\" \"ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041\" \"yes\"") +
            HelpExampleRpc("mxbudgetvote", "\"local\" \"ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041\" \"yes\""));

    uint256 hash = ParseHashV(params[1], "parameter 1");
    std::string strVote = params[2].get_str();

    if (strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
    int nVote = VOTE_ABSTAIN;
    if (strVote == "yes") nVote = VOTE_YES;
    if (strVote == "no") nVote = VOTE_NO;

    int success = 0;
    int failed = 0;

    UniValue resultsObj(UniValue::VARR);

    if (strCommand == "local") {
        CPubKey pubKeyMaxnode;
        CKey keyMaxnode;
        std::string errorMessage;

        UniValue statusObj(UniValue::VOBJ);

        while (true) {
            if (!obfuScationSigner.SetKey(strMaxNodePrivKey, errorMessage, keyMaxnode, pubKeyMaxnode)) {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Maxnode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(statusObj);
                break;
            }

            CMaxnode* pmax = maxnodeman.Find(activeMaxnode.maxvin);
            if (pmax == NULL) {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to find maxnode in list : " + activeMaxnode.maxvin.ToString()));
                resultsObj.push_back(statusObj);
                break;
            }

            CMAXBudgetVote vote(activeMaxnode.maxvin, hash, nVote);
            if (!vote.Sign(keyMaxnode, pubKeyMaxnode)) {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to sign."));
                resultsObj.push_back(statusObj);
                break;
            }

            std::string strError = "";
            if (maxbudget.UpdateProposal(vote, NULL, strError)) {
                success++;
                maxbudget.mapSeenMaxnodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "success"));
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Error voting : " + strError));
            }
            resultsObj.push_back(statusObj);
            break;
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "many") {
        BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchMaxNodeSignature;
            std::string strMaxNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMaxnode;
            CKey keyMaxnode;

            UniValue statusObj(UniValue::VOBJ);

            if (!obfuScationSigner.SetKey(maxe.getPrivKey(), errorMessage, keyMaxnode, pubKeyMaxnode)) {
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Maxnode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(statusObj);
                continue;
            }

            CMaxnode* pmax = maxnodeman.Find(pubKeyMaxnode);
            if (pmax == NULL) {
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Can't find maxnode by pubkey"));
                resultsObj.push_back(statusObj);
                continue;
            }

            CMAXBudgetVote vote(pmax->maxvin, hash, nVote);
            if (!vote.Sign(keyMaxnode, pubKeyMaxnode)) {
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to sign."));
                resultsObj.push_back(statusObj);
                continue;
            }

            std::string strError = "";
            if (maxbudget.UpdateProposal(vote, NULL, strError)) {
                maxbudget.mapSeenMaxnodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "success"));
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", strError.c_str()));
            }

            resultsObj.push_back(statusObj);
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "alias") {
        std::string strAlias = params[3].get_str();
        std::vector<CMaxnodeConfig::CMaxnodeEntry> maxEntries;
        maxEntries = maxnodeConfig.getEntries();

        BOOST_FOREACH(CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {

            if( strAlias != maxe.getAlias()) continue;

            std::string errorMessage;
            std::vector<unsigned char> vchMaxNodeSignature;
            std::string strMaxNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMaxnode;
            CKey keyMaxnode;

            UniValue statusObj(UniValue::VOBJ);

            if(!obfuScationSigner.SetKey(maxe.getPrivKey(), errorMessage, keyMaxnode, pubKeyMaxnode)){
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Maxnode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(statusObj);
                continue;
            }

            CMaxnode* pmax = maxnodeman.Find(pubKeyMaxnode);
            if(pmax == NULL)
            {
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Can't find maxnode by pubkey"));
                resultsObj.push_back(statusObj);
                continue;
            }

            CMAXBudgetVote vote(pmax->maxvin, hash, nVote);
            if(!vote.Sign(keyMaxnode, pubKeyMaxnode)){
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to sign."));
                resultsObj.push_back(statusObj);
                continue;
            }

            std::string strError = "";
            if(maxbudget.UpdateProposal(vote, NULL, strError)) {
                maxbudget.mapSeenMaxnodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "success"));
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("node", maxe.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", strError.c_str()));
            }

            resultsObj.push_back(statusObj);
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    return NullUniValue;
}

UniValue getbudgetvotes(const UniValue& params, bool fHelp)
{
    if (params.size() != 1)
        throw runtime_error(
            "getbudgetvotes \"proposal-name\"\n"
            "\nPrint vote information for a mxbudget proposal\n"

            "\nArguments:\n"
            "1. \"proposal-name\":      (string, required) Name of the proposal\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"maxId\": \"xxxx\",        (string) Hash of the maxnode's collateral transaction\n"
            "    \"nHash\": \"xxxx\",       (string) Hash of the vote\n"
            "    \"Vote\": \"YES|NO\",      (string) Vote cast ('YES' or 'NO')\n"
            "    \"nTime\": xxxx,         (numeric) Time in seconds since epoch the vote was cast\n"
            "    \"fValid\": true|false,  (boolean) 'true' if the vote is valid, 'false' otherwise\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getbudgetvotes", "\"test-proposal\"") + HelpExampleRpc("getbudgetvotes", "\"test-proposal\""));

    std::string strProposalName = SanitizeString(params[0].get_str());

    UniValue ret(UniValue::VARR);

    CMAXBudgetProposal* pmxbudgetProposal = maxbudget.FindMaxProposal(strProposalName);

    if (pmxbudgetProposal == NULL) throw runtime_error("Unknown proposal name");

    std::map<uint256, CMAXBudgetVote>::iterator it = pmxbudgetProposal->mapVotes.begin();
    while (it != pmxbudgetProposal->mapVotes.end()) {
        UniValue bObj(UniValue::VOBJ);
        bObj.push_back(Pair("maxId", (*it).second.vin.prevout.hash.ToString()));
        bObj.push_back(Pair("nHash", (*it).first.ToString().c_str()));
        bObj.push_back(Pair("Vote", (*it).second.GetVoteString()));
        bObj.push_back(Pair("nTime", (int64_t)(*it).second.nTime));
        bObj.push_back(Pair("fValid", (*it).second.fValid));

        ret.push_back(bObj);

        it++;
    }

    return ret;
}

UniValue getnextmaxsuperblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnextsuperblock\n"
            "\nPrint the next super block height\n"

            "\nResult:\n"
            "n      (numeric) Block height of the next super block\n"

            "\nExamples:\n" +
            HelpExampleCli("getnextsuperblock", "") + HelpExampleRpc("getnextsuperblock", ""));

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return "unknown";

    int nNext = pindexPrev->nHeight - pindexPrev->nHeight % GetMaxBudgetPaymentCycleBlocks() + GetMaxBudgetPaymentCycleBlocks();
    return nNext;
}

UniValue getbudgetprojection(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbudgetprojection\n"
            "\nShow the projection of which proposals will be paid the next cycle\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"Name\": \"xxxx\",               (string) Proposal Name\n"
            "    \"URL\": \"xxxx\",                (string) Proposal URL\n"
            "    \"Hash\": \"xxxx\",               (string) Proposal vote hash\n"
            "    \"FeeHash\": \"xxxx\",            (string) Proposal fee hash\n"
            "    \"BlockStart\": n,              (numeric) Proposal starting block\n"
            "    \"BlockEnd\": n,                (numeric) Proposal ending block\n"
            "    \"TotalPaymentCount\": n,       (numeric) Number of payments\n"
            "    \"RemainingPaymentCount\": n,   (numeric) Number of remaining payments\n"
            "    \"PaymentAddress\": \"xxxx\",     (string) Lytix address of payment\n"
            "    \"Ratio\": x.xxx,               (numeric) Ratio of yeas vs nays\n"
            "    \"Yeas\": n,                    (numeric) Number of yea votes\n"
            "    \"Nays\": n,                    (numeric) Number of nay votes\n"
            "    \"Abstains\": n,                (numeric) Number of abstains\n"
            "    \"TotalPayment\": xxx.xxx,      (numeric) Total payment amount\n"
            "    \"MonthlyPayment\": xxx.xxx,    (numeric) Monthly payment amount\n"
            "    \"IsEstablished\": true|false,  (boolean) Established (true) or (false)\n"
            "    \"IsValid\": true|false,        (boolean) Valid (true) or Invalid (false)\n"
            "    \"IsValidReason\": \"xxxx\",      (string) Error message, if any\n"
            "    \"fValid\": true|false,         (boolean) Valid (true) or Invalid (false)\n"
            "    \"Alloted\": xxx.xxx,           (numeric) Amount alloted in current period\n"
            "    \"TotalBudgetAlloted\": xxx.xxx (numeric) Total alloted\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getbudgetprojection", "") + HelpExampleRpc("getbudgetprojection", ""));

    UniValue ret(UniValue::VARR);
    UniValue resultObj(UniValue::VOBJ);
    CAmount nTotalAllotted = 0;

    std::vector<CMAXBudgetProposal*> winningProps = maxbudget.GetBudget();
    BOOST_FOREACH (CMAXBudgetProposal* pmxbudgetProposal, winningProps) {
        nTotalAllotted += pmxbudgetProposal->GetAllotted();

        CTxDestination address1;
        ExtractDestination(pmxbudgetProposal->GetPayee(), address1);
        CBitcoinAddress address2(address1);

        UniValue bObj(UniValue::VOBJ);
        mxbudgetToJSON(pmxbudgetProposal, bObj);
        bObj.push_back(Pair("Alloted", ValueFromAmount(pmxbudgetProposal->GetAllotted())));
        bObj.push_back(Pair("TotalBudgetAlloted", ValueFromAmount(nTotalAllotted)));

        ret.push_back(bObj);
    }

    return ret;
}

UniValue getbudgetinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getbudgetinfo ( \"proposal\" )\n"
            "\nShow current maxnode mxbudgets\n"

            "\nArguments:\n"
            "1. \"proposal\"    (string, optional) Proposal name\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"Name\": \"xxxx\",               (string) Proposal Name\n"
            "    \"URL\": \"xxxx\",                (string) Proposal URL\n"
            "    \"Hash\": \"xxxx\",               (string) Proposal vote hash\n"
            "    \"FeeHash\": \"xxxx\",            (string) Proposal fee hash\n"
            "    \"BlockStart\": n,              (numeric) Proposal starting block\n"
            "    \"BlockEnd\": n,                (numeric) Proposal ending block\n"
            "    \"TotalPaymentCount\": n,       (numeric) Number of payments\n"
            "    \"RemainingPaymentCount\": n,   (numeric) Number of remaining payments\n"
            "    \"PaymentAddress\": \"xxxx\",     (string) Lytix address of payment\n"
            "    \"Ratio\": x.xxx,               (numeric) Ratio of yeas vs nays\n"
            "    \"Yeas\": n,                    (numeric) Number of yea votes\n"
            "    \"Nays\": n,                    (numeric) Number of nay votes\n"
            "    \"Abstains\": n,                (numeric) Number of abstains\n"
            "    \"TotalPayment\": xxx.xxx,      (numeric) Total payment amount\n"
            "    \"MonthlyPayment\": xxx.xxx,    (numeric) Monthly payment amount\n"
            "    \"IsEstablished\": true|false,  (boolean) Established (true) or (false)\n"
            "    \"IsValid\": true|false,        (boolean) Valid (true) or Invalid (false)\n"
            "    \"IsValidReason\": \"xxxx\",      (string) Error message, if any\n"
            "    \"fValid\": true|false,         (boolean) Valid (true) or Invalid (false)\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getbudgetprojection", "") + HelpExampleRpc("getbudgetprojection", ""));

    UniValue ret(UniValue::VARR);

    std::string strShow = "valid";
    if (params.size() == 1) {
        std::string strProposalName = SanitizeString(params[0].get_str());
        CMAXBudgetProposal* pmxbudgetProposal = maxbudget.FindMaxProposal(strProposalName);
        if (pmxbudgetProposal == NULL) throw runtime_error("Unknown proposal name");
        UniValue bObj(UniValue::VOBJ);
        mxbudgetToJSON(pmxbudgetProposal, bObj);
        ret.push_back(bObj);
        return ret;
    }

    std::vector<CMAXBudgetProposal*> winningProps = maxbudget.GetAllProposals();
    BOOST_FOREACH (CMAXBudgetProposal* pmxbudgetProposal, winningProps) {
        if (strShow == "valid" && !pmxbudgetProposal->fValid) continue;

        UniValue bObj(UniValue::VOBJ);
        mxbudgetToJSON(pmxbudgetProposal, bObj);

        ret.push_back(bObj);
    }

    return ret;
}

UniValue mxbudgetrawvote(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
        throw runtime_error(
            "mxbudgetrawvote \"maxnode-tx-hash\" maxnode-tx-index \"proposal-hash\" yes|no time \"vote-sig\"\n"
            "\nCompile and relay a proposal vote with provided external signature instead of signing vote internally\n"

            "\nArguments:\n"
            "1. \"maxnode-tx-hash\"  (string, required) Transaction hash for the maxnode\n"
            "2. maxnode-tx-index   (numeric, required) Output index for the maxnode\n"
            "3. \"proposal-hash\"       (string, required) Proposal vote hash\n"
            "4. yes|no                (boolean, required) Vote to cast\n"
            "5. time                  (numeric, required) Time since epoch in seconds\n"
            "6. \"vote-sig\"            (string, required) External signature\n"

            "\nResult:\n"
            "\"status\"     (string) Vote status or error message\n"

            "\nExamples:\n" +
            HelpExampleCli("mxbudgetrawvote", "") + HelpExampleRpc("mxbudgetrawvote", ""));

    uint256 hashMnTx = ParseHashV(params[0], "max tx hash");
    int nMnTxIndex = params[1].get_int();
    CTxIn maxvin = CTxIn(hashMnTx, nMnTxIndex);

    uint256 hashProposal = ParseHashV(params[2], "Proposal hash");
    std::string strVote = params[3].get_str();

    if (strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
    int nVote = VOTE_ABSTAIN;
    if (strVote == "yes") nVote = VOTE_YES;
    if (strVote == "no") nVote = VOTE_NO;

    int64_t nTime = params[4].get_int64();
    std::string strSig = params[5].get_str();
    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSig.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CMaxnode* pmax = maxnodeman.Find(maxvin);
    if (pmax == NULL) {
        return "Failure to find maxnode in list : " + maxvin.ToString();
    }

    CMAXBudgetVote vote(maxvin, hashProposal, nVote);
    vote.nTime = nTime;
    vote.vchSig = vchSig;

    if (!vote.SignatureValid(true)) {
        return "Failure to verify signature.";
    }

    std::string strError = "";
    if (maxbudget.UpdateProposal(vote, NULL, strError)) {
        maxbudget.mapSeenMaxnodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        vote.Relay();
        return "Voted successfully";
    } else {
        return "Error voting : " + strError;
    }
}

UniValue maxfinalmxbudget(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "suggest" && strCommand != "vote-many" && strCommand != "vote" && strCommand != "show" && strCommand != "getvotes"))
        throw runtime_error(
            "maxfinalmxbudget \"command\"... ( \"passphrase\" )\n"
            "\nVote or show current mxbudgets\n"

            "\nAvailable commands:\n"
            "  vote-many   - Vote on a finalized mxbudget\n"
            "  vote        - Vote on a finalized mxbudget\n"
            "  show        - Show existing finalized mxbudgets\n"
            "  getvotes     - Get vote information for each finalized mxbudget\n");

    if (strCommand == "vote-many") {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'maxfinalmxbudget vote-many BUDGET_HASH'");

        std::string strHash = params[1].get_str();
        uint256 hash(strHash);

        int success = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VOBJ);

        BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchMaxNodeSignature;
            std::string strMaxNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMaxnode;
            CKey keyMaxnode;

            UniValue statusObj(UniValue::VOBJ);

            if (!obfuScationSigner.SetKey(maxe.getPrivKey(), errorMessage, keyMaxnode, pubKeyMaxnode)) {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Maxnode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(Pair(maxe.getAlias(), statusObj));
                continue;
            }

            CMaxnode* pmax = maxnodeman.Find(pubKeyMaxnode);
            if (pmax == NULL) {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Can't find maxnode by pubkey"));
                resultsObj.push_back(Pair(maxe.getAlias(), statusObj));
                continue;
            }


            CMAXFinalizedBudgetVote vote(pmax->maxvin, hash);
            if (!vote.Sign(keyMaxnode, pubKeyMaxnode)) {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Failure to sign."));
                resultsObj.push_back(Pair(maxe.getAlias(), statusObj));
                continue;
            }

            std::string strError = "";
            if (maxbudget.UpdateFinalizedBudget(vote, NULL, strError)) {
                maxbudget.mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("result", "success"));
            } else {
                failed++;
                statusObj.push_back(Pair("result", strError.c_str()));
            }

            resultsObj.push_back(Pair(maxe.getAlias(), statusObj));
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "vote") {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'maxfinalmxbudget vote BUDGET_HASH'");

        std::string strHash = params[1].get_str();
        uint256 hash(strHash);

        CPubKey pubKeyMaxnode;
        CKey keyMaxnode;
        std::string errorMessage;

        if (!obfuScationSigner.SetKey(strMaxNodePrivKey, errorMessage, keyMaxnode, pubKeyMaxnode))
            return "Error upon calling SetKey";

        CMaxnode* pmax = maxnodeman.Find(activeMaxnode.maxvin);
        if (pmax == NULL) {
            return "Failure to find maxnode in list : " + activeMaxnode.maxvin.ToString();
        }

        CMAXFinalizedBudgetVote vote(activeMaxnode.maxvin, hash);
        if (!vote.Sign(keyMaxnode, pubKeyMaxnode)) {
            return "Failure to sign.";
        }

        std::string strError = "";
        if (maxbudget.UpdateFinalizedBudget(vote, NULL, strError)) {
            maxbudget.mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
            vote.Relay();
            return "success";
        } else {
            return "Error voting : " + strError;
        }
    }

    if (strCommand == "show") {
        UniValue resultObj(UniValue::VOBJ);

        std::vector<CMAXFinalizedBudget*> winningFbs = maxbudget.GetFinalizedBudgets();
        BOOST_FOREACH (CMAXFinalizedBudget* finalizedBudget, winningFbs) {
            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("FeeTX", finalizedBudget->nFeeTXHash.ToString()));
            bObj.push_back(Pair("Hash", finalizedBudget->GetHash().ToString()));
            bObj.push_back(Pair("BlockStart", (int64_t)finalizedBudget->GetMaxBlockStart()));
            bObj.push_back(Pair("BlockEnd", (int64_t)finalizedBudget->GetMaxBlockEnd()));
            bObj.push_back(Pair("Proposals", finalizedBudget->GetMaxProposals()));
            bObj.push_back(Pair("VoteCount", (int64_t)finalizedBudget->GetMaxVoteCount()));
            bObj.push_back(Pair("Status", finalizedBudget->GetStatus()));

            std::string strError = "";
            bObj.push_back(Pair("IsValid", finalizedBudget->IsValid(strError)));
            bObj.push_back(Pair("IsValidReason", strError.c_str()));

            resultObj.push_back(Pair(finalizedBudget->GetName(), bObj));
        }

        return resultObj;
    }

    if (strCommand == "getvotes") {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'mxbudget getvotes mxbudget-hash'");

        std::string strHash = params[1].get_str();
        uint256 hash(strHash);

        UniValue obj(UniValue::VOBJ);

        CMAXFinalizedBudget* pfinalBudget = maxbudget.FindFinalizedBudget(hash);

        if (pfinalBudget == NULL) return "Unknown mxbudget hash";

        std::map<uint256, CMAXFinalizedBudgetVote>::iterator it = pfinalBudget->mapVotes.begin();
        while (it != pfinalBudget->mapVotes.end()) {
            UniValue bObj(UniValue::VOBJ);
            bObj.push_back(Pair("nHash", (*it).first.ToString().c_str()));
            bObj.push_back(Pair("nTime", (int64_t)(*it).second.nTime));
            bObj.push_back(Pair("fValid", (*it).second.fValid));

            obj.push_back(Pair((*it).second.vin.prevout.ToStringShort(), bObj));

            it++;
        }

        return obj;
    }

    return NullUniValue;
}

UniValue checkbudgets(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "checkbudgets\n"
            "\nInitiates a buddget check cycle manually\n"

            "\nExamples:\n" +
            HelpExampleCli("checkbudgets", "") + HelpExampleRpc("checkbudgets", ""));

    maxbudget.CheckAndRemove();

    return NullUniValue;
}
