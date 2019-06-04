// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2019 The Lytix developers
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

#include <boost/tokenizer.hpp>
#include <fstream>

UniValue getmaxpoolinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmaxpoolinfo\n"
            "\nReturns anonymous pool-related information\n"

            "\nResult:\n"
            "{\n"
            "  \"current\": \"addr\",    (string) Lytix address of current maxnode\n"
            "  \"state\": xxxx,        (string) unknown\n"
            "  \"entries\": xxxx,      (numeric) Number of entries\n"
            "  \"accepted\": xxxx,     (numeric) Number of entries accepted\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmaxpoolinfo", "") + HelpExampleRpc("getmaxpoolinfo", ""));

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("current_maxnode", maxnodeman.GetCurrentMaxNode()->addr.ToString()));
    obj.push_back(Pair("state", obfuScationPool.GetState()));
    obj.push_back(Pair("entries", obfuScationPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted", obfuScationPool.GetCountEntriesAccepted()));
    return obj;
}

// This command is retained for backwards compatibility, but is depreciated.
// Future removal of this command is planned to keep things clean.
UniValue maxnode(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "start-all-max" && strCommand != "start-missing" &&
            strCommand != "start-disabled" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count" && strCommand != "enforce" &&
            strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" &&
            strCommand != "outputs" && strCommand != "status" && strCommand != "calcscore"))
        throw runtime_error(
            "maxnode \"command\"...\n"
            "\nSet of commands to execute maxnode related actions\n"
            "This command is depreciated, please see individual command documentation for future reference\n\n"

            "\nArguments:\n"
            "1. \"command\"        (string or set of strings, required) The command to execute\n"

            "\nAvailable commands:\n"
            "  count        - Print count information of all known maxnodes\n"
            "  current      - Print info on current maxnode winner\n"
            "  debug        - Print maxnode status\n"
            "  genkey       - Generate new maxnodeprivkey\n"
            "  outputs      - Print maxnode compatible outputs\n"
            "  start        - Start maxnode configured in lytix.conf\n"
            "  start-alias  - Start single maxnode by assigned alias configured in maxnode.conf\n"
            "  start-<mode> - Start maxnodes configured in maxnode.conf (<mode>: 'all', 'missing', 'disabled')\n"
            "  status       - Print maxnode status information\n"
            "  list         - Print list of all known maxnodes (see maxnodelist for more info)\n"
            "  list-conf    - Print maxnode.conf in JSON format\n"
            "  winners      - Print list of maxnode winners\n");

    if (strCommand == "list") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return listmaxnodes(newParams, fHelp);
    }

    if (strCommand == "connect") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return maxnodeconnect(newParams, fHelp);
    }

    if (strCommand == "count") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmaxnodecount(newParams, fHelp);
    }

    if (strCommand == "current") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return maxnodecurrent(newParams, fHelp);
    }

    if (strCommand == "debug") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return maxnodedebug(newParams, fHelp);
    }

    if (strCommand == "start" || strCommand == "start-alias" || strCommand == "start-many" || strCommand == "start-all-max" || strCommand == "start-missing" || strCommand == "start-disabled") {
        return startmaxnode(params, fHelp);
    }

    if (strCommand == "genkey") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return createmaxnodekey(newParams, fHelp);
    }

    if (strCommand == "list-conf") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return listmaxnodeconf(newParams, fHelp);
    }

    if (strCommand == "outputs") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmaxnodeoutputs(newParams, fHelp);
    }

    if (strCommand == "status") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmaxnodestatus(newParams, fHelp);
    }

    if (strCommand == "winners") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmaxnodewinners(newParams, fHelp);
    }

    if (strCommand == "calcscore") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmaxnodescores(newParams, fHelp);
    }

    return NullUniValue;
}

UniValue listmaxnodes(const UniValue& params, bool fHelp)
{
    std::string strFilter = "";

    if (params.size() == 1) strFilter = params[0].get_str();

    if (fHelp || (params.size() > 1))
        throw runtime_error(
            "listmaxnodes ( \"filter\" )\n"
            "\nGet a ranked list of maxnodes\n"

            "\nArguments:\n"
            "1. \"filter\"    (string, optional) Filter search text. Partial match by txhash, status, or addr.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"rank\": n,           (numeric) Maxnode Rank (or 0 if not enabled)\n"
            "    \"txhash\": \"hash\",    (string) Collateral transaction hash\n"
            "    \"outidx\": n,         (numeric) Collateral transaction output index\n"
            "    \"status\": s,         (string) Status (ENABLED/EXPIRED/REMOVE/etc)\n"
            "    \"addr\": \"addr\",      (string) Maxnode Lytix address\n"
            "    \"version\": v,        (numeric) Maxnode protocol version\n"
            "    \"lastseen\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last seen\n"
            "    \"activetime\": ttt,   (numeric) The time in seconds since epoch (Jan 1 1970 GMT) maxnode has been active\n"
            "    \"lastpaid\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) maxnode was last paid\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listmaxnodes", "") + HelpExampleRpc("listmaxnodes", ""));

    UniValue ret(UniValue::VARR);
    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return 0;
        nHeight = pindex->nHeight;
    }
    std::vector<pair<int, CMaxnode> > vMaxnodeRanks = maxnodeman.GetMaxnodeRanks(nHeight);
    BOOST_FOREACH (PAIRTYPE(int, CMaxnode) & s, vMaxnodeRanks) {
        UniValue obj(UniValue::VOBJ);
        std::string strVin = s.second.maxvin.prevout.ToStringShort();
        std::string strTxHash = s.second.maxvin.prevout.hash.ToString();
        uint32_t oIdx = s.second.maxvin.prevout.n;

        CMaxnode* max = maxnodeman.Find(s.second.maxvin);

        if (max != NULL) {
            if (strFilter != "" && strTxHash.find(strFilter) == string::npos &&
                max->Status().find(strFilter) == string::npos &&
                CBitcoinAddress(max->pubKeyCollateralAddress.GetID()).ToString().find(strFilter) == string::npos) continue;

            std::string strStatus = max->Status();
            std::string strHost;
            int port;
            SplitHostPort(max->addr.ToString(), port, strHost);
            CNetAddr node = CNetAddr(strHost, false);
            std::string strNetwork = GetNetworkName(node.GetNetwork());

            obj.push_back(Pair("rank", (strStatus == "ENABLED" ? s.first : 0)));
            obj.push_back(Pair("network", strNetwork));
            obj.push_back(Pair("txhash", strTxHash));
            obj.push_back(Pair("outidx", (uint64_t)oIdx));
            obj.push_back(Pair("status", strStatus));
            obj.push_back(Pair("addr", CBitcoinAddress(max->pubKeyCollateralAddress.GetID()).ToString()));
            obj.push_back(Pair("version", max->protocolVersion));
            obj.push_back(Pair("lastseen", (int64_t)max->lastPing.sigTime));
            obj.push_back(Pair("activetime", (int64_t)(max->lastPing.sigTime - max->sigTime)));
            obj.push_back(Pair("lastpaid", (int64_t)max->GetLastPaid()));

            ret.push_back(obj);
        }
    }

    return ret;
}

UniValue maxnodeconnect(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 1))
        throw runtime_error(
            "maxnodeconnect \"address\"\n"
            "\nAttempts to connect to specified maxnode address\n"

            "\nArguments:\n"
            "1. \"address\"     (string, required) IP or net address to connect to\n"

            "\nExamples:\n" +
            HelpExampleCli("maxnodeconnect", "\"192.168.0.6:9009\"") + HelpExampleRpc("maxnodeconnect", "\"192.168.0.6:9009\""));

    std::string strAddress = params[0].get_str();

    CService addr = CService(strAddress);

    CNode* pnode = ConnectNode((CAddress)addr, NULL, false);
    if (pnode) {
        pnode->Release();
        return NullUniValue;
    } else {
        throw runtime_error("error connecting\n");
    }
}

UniValue getmaxnodecount (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 0))
        throw runtime_error(
            "getmaxnodecount\n"
            "\nGet maxnode count values\n"

            "\nResult:\n"
            "{\n"
            "  \"total\": n,        (numeric) Total maxnodes\n"
            "  \"stable\": n,       (numeric) Stable count\n"
            "  \"obfcompat\": n,    (numeric) Obfuscation Compatible\n"
            "  \"enabled\": n,      (numeric) Enabled maxnodes\n"
            "  \"inqueue\": n       (numeric) Maxnodes in queue\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmaxnodecount", "") + HelpExampleRpc("getmaxnodecount", ""));

    UniValue obj(UniValue::VOBJ);
    int nCount = 0;
    int ipv4 = 0, ipv6 = 0, onion = 0;

    if (chainActive.Tip())
        maxnodeman.GetNextMaxnodeInQueueForPayment(chainActive.Tip()->nHeight, true, nCount);

    maxnodeman.CountNetworks(ActiveProtocol(), ipv4, ipv6, onion);

    obj.push_back(Pair("total", maxnodeman.size()));
    obj.push_back(Pair("stable", maxnodeman.stable_size()));
    obj.push_back(Pair("obfcompat", maxnodeman.CountEnabled(ActiveProtocol())));
    obj.push_back(Pair("enabled", maxnodeman.CountEnabled()));
    obj.push_back(Pair("inqueue", nCount));
    obj.push_back(Pair("ipv4", ipv4));
    obj.push_back(Pair("ipv6", ipv6));
    obj.push_back(Pair("onion", onion));

    return obj;
}

UniValue maxnodecurrent (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw runtime_error(
            "maxnodecurrent\n"
            "\nGet current maxnode winner\n"

            "\nResult:\n"
            "{\n"
            "  \"protocol\": xxxx,        (numeric) Protocol version\n"
            "  \"txhash\": \"xxxx\",      (string) Collateral transaction hash\n"
            "  \"pubkey\": \"xxxx\",      (string) MAX Public key\n"
            "  \"lastseen\": xxx,       (numeric) Time since epoch of last seen\n"
            "  \"activeseconds\": xxx,  (numeric) Seconds MAX has been active\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("maxnodecurrent", "") + HelpExampleRpc("maxnodecurrent", ""));

    CMaxnode* winner = maxnodeman.GetCurrentMaxNode(1);
    if (winner) {
        UniValue obj(UniValue::VOBJ);

        obj.push_back(Pair("protocol", (int64_t)winner->protocolVersion));
        obj.push_back(Pair("txhash", winner->maxvin.prevout.hash.ToString()));
        obj.push_back(Pair("pubkey", CBitcoinAddress(winner->pubKeyCollateralAddress.GetID()).ToString()));
        obj.push_back(Pair("lastseen", (winner->lastPing == CMaxnodePing()) ? winner->sigTime : (int64_t)winner->lastPing.sigTime));
        obj.push_back(Pair("activeseconds", (winner->lastPing == CMaxnodePing()) ? 0 : (int64_t)(winner->lastPing.sigTime - winner->sigTime)));
        return obj;
    }

    throw runtime_error("unknown");
}

UniValue maxnodedebug (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw runtime_error(
            "maxnodedebug\n"
            "\nPrint maxnode status\n"

            "\nResult:\n"
            "\"status\"     (string) Maxnode status message\n"

            "\nExamples:\n" +
            HelpExampleCli("maxnodedebug", "") + HelpExampleRpc("maxnodedebug", ""));

    if (activeMaxnode.status != ACTIVE_MAXNODE_INITIAL || !maxnodeSync.IsSynced())
        return activeMaxnode.GetStatus();

    CTxIn maxvin = CTxIn();
    CPubKey pubkey = CScript();
    CKey key;
    if (!activeMaxnode.GetMaxNodeVin(maxvin, pubkey, key))
        throw runtime_error("Missing maxnode input, please look at the documentation for instructions on maxnode creation\n");
    else
        return activeMaxnode.GetStatus();
}

UniValue startmaxnode (const UniValue& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1) {
        strCommand = params[0].get_str();

        // Backwards compatibility with legacy 'maxnode' super-command forwarder
        if (strCommand == "start") strCommand = "local";
        if (strCommand == "start-alias") strCommand = "alias";
        if (strCommand == "start-all-max") strCommand = "all";
        if (strCommand == "start-many") strCommand = "many";
        if (strCommand == "start-missing") strCommand = "missing";
        if (strCommand == "start-disabled") strCommand = "disabled";
    }

    if (fHelp || params.size() < 2 || params.size() > 3 ||
        (params.size() == 2 && (strCommand != "local" && strCommand != "all" && strCommand != "many" && strCommand != "missing" && strCommand != "disabled")) ||
        (params.size() == 3 && strCommand != "alias"))
        throw runtime_error(
            "startmaxnode \"local|all|many|missing|disabled|alias\" lockwallet ( \"alias\" )\n"
            "\nAttempts to start one or more maxnode(s)\n"

            "\nArguments:\n"
            "1. set         (string, required) Specify which set of maxnode(s) to start.\n"
            "2. lockwallet  (boolean, required) Lock wallet after completion.\n"
            "3. alias       (string) Maxnode alias. Required if using 'alias' as the set.\n"

            "\nResult: (for 'local' set):\n"
            "\"status\"     (string) Maxnode status message\n"

            "\nResult: (for other sets):\n"
            "{\n"
            "  \"overall\": \"xxxx\",     (string) Overall status message\n"
            "  \"detail\": [\n"
            "    {\n"
            "      \"node\": \"xxxx\",    (string) Node name or alias\n"
            "      \"result\": \"xxxx\",  (string) 'success' or 'failed'\n"
            "      \"error\": \"xxxx\"    (string) Error message, if failed\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("startmaxnode", "\"alias\" \"0\" \"my_max\"") + HelpExampleRpc("startmaxnode", "\"alias\" \"0\" \"my_max\""));

    bool fLock = (params[1].get_str() == "true" ? true : false);

    EnsureWalletIsUnlocked();

    if (strCommand == "local") {
        //if (!fMaxNodeT1 || !fMaxNodeT2 || !fMaxNodeT3) throw runtime_error("you must set maxnode=1 in the configuration\n");
        if (!fMaxNode) throw runtime_error("you must set maxnode=1 in the configuration\n");

        if (activeMaxnode.status != ACTIVE_MAXNODE_STARTED) {
            activeMaxnode.status = ACTIVE_MAXNODE_INITIAL; // TODO: consider better way
            activeMaxnode.ManageStatus();
            if (fLock)
                pwalletMain->Lock();
        }

        return activeMaxnode.GetStatus();
    }

    if (strCommand == "all" || strCommand == "many" || strCommand == "missing" || strCommand == "disabled") {
        if ((strCommand == "missing" || strCommand == "disabled") &&
            (maxnodeSync.RequestedMaxnodeAssets <= MAXNODE_SYNC_LIST ||
                maxnodeSync.RequestedMaxnodeAssets == MAXNODE_SYNC_FAILED)) {
            throw runtime_error("You can't use this command until maxnode list is synced\n");
        }

        std::vector<CMaxnodeConfig::CMaxnodeEntry> maxEntries;
        maxEntries = maxnodeConfig.getEntries();

        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);

        BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
            std::string errorMessage;
            int nIndex;
            if(!maxe.castOutputIndex(nIndex))
                continue;
            CTxIn maxvin = CTxIn(uint256(maxe.getTxHash()), uint32_t(nIndex));
            CMaxnode* pmax = maxnodeman.Find(maxvin);
            CMaxnodeBroadcast maxb;

            if (pmax != NULL) {
                if (strCommand == "missing") continue;
                if (strCommand == "disabled" && pmax->IsEnabled()) continue;
            }

            bool result = activeMaxnode.CreateBroadcast(maxe.getIp(), maxe.getPrivKey(), maxe.getTxHash(), maxe.getOutputIndex(), errorMessage, maxb);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", maxe.getAlias()));
            statusObj.push_back(Pair("result", result ? "success" : "failed"));

            if (result) {
                successful++;
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("error", errorMessage));
            }

            resultsObj.push_back(statusObj);
        }
        if (fLock)
            pwalletMain->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d maxnodes, failed to start %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "alias") {
        std::string alias = params[2].get_str();

        bool found = false;
        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);
        UniValue statusObj(UniValue::VOBJ);
        statusObj.push_back(Pair("alias", alias));

        BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
            if (maxe.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                CMaxnodeBroadcast maxb;

                bool result = activeMaxnode.CreateBroadcast(maxe.getIp(), maxe.getPrivKey(), maxe.getTxHash(), maxe.getOutputIndex(), errorMessage, maxb);

                statusObj.push_back(Pair("result", result ? "successful" : "failed"));

                if (result) {
                    successful++;
                    maxnodeman.UpdateMaxnodeList(maxb);
                    maxb.Relay();
                } else {
                    failed++;
                    statusObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if (!found) {
            failed++;
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("error", "could not find alias in config. Verify with list-conf."));
        }

        resultsObj.push_back(statusObj);

        if (fLock)
            pwalletMain->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d maxnodes, failed to start %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }
    return NullUniValue;
}

UniValue createmaxnodekey (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw runtime_error(
            "createmaxnodekey\n"
            "\nCreate a new maxnode private key\n"

            "\nResult:\n"
            "\"key\"    (string) Maxnode private key\n"

            "\nExamples:\n" +
            HelpExampleCli("createmaxnodekey", "") + HelpExampleRpc("createmaxnodekey", ""));

    CKey secret;
    secret.MakeNewKey(false);

    return CBitcoinSecret(secret).ToString();
}

UniValue getmaxnodeoutputs (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw runtime_error(
            "getmaxnodeoutputs\n"
            "\nPrint all maxnode transaction outputs\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txhash\": \"xxxx\",    (string) output transaction hash\n"
            "    \"outputidx\": n       (numeric) output index number\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getmaxnodeoutputs", "") + HelpExampleRpc("getmaxnodeoutputs", ""));

    // Find possible candidates
    vector<COutput> possibleCoins = activeMaxnode.SelectCoinsMaxnode();

    UniValue ret(UniValue::VARR);
    BOOST_FOREACH (COutput& out, possibleCoins) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("txhash", out.tx->GetHash().ToString()));
        obj.push_back(Pair("outputidx", out.i));
        ret.push_back(obj);
    }

    return ret;
}

UniValue listmaxnodeconf (const UniValue& params, bool fHelp)
{
    std::string strFilter = "";

    if (params.size() == 1) strFilter = params[0].get_str();

    if (fHelp || (params.size() > 1))
        throw runtime_error(
            "listmaxnodeconf ( \"filter\" )\n"
            "\nPrint maxnode.conf in JSON format\n"

            "\nArguments:\n"
            "1. \"filter\"    (string, optional) Filter search text. Partial match on alias, address, txHash, or status.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"alias\": \"xxxx\",        (string) maxnode alias\n"
            "    \"address\": \"xxxx\",      (string) maxnode IP address\n"
            "    \"privateKey\": \"xxxx\",   (string) maxnode private key\n"
            "    \"txHash\": \"xxxx\",       (string) transaction hash\n"
            "    \"outputIndex\": n,       (numeric) transaction output index\n"
            "    \"status\": \"xxxx\"        (string) maxnode status\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listmaxnodeconf", "") + HelpExampleRpc("listmaxnodeconf", ""));

    std::vector<CMaxnodeConfig::CMaxnodeEntry> maxEntries;
    maxEntries = maxnodeConfig.getEntries();

    UniValue ret(UniValue::VARR);

    BOOST_FOREACH (CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
        int nIndex;
        if(!maxe.castOutputIndex(nIndex))
            continue;
        CTxIn maxvin = CTxIn(uint256(maxe.getTxHash()), uint32_t(nIndex));
        CMaxnode* pmax = maxnodeman.Find(maxvin);

        std::string strStatus = pmax ? pmax->Status() : "MISSING";

        if (strFilter != "" && maxe.getAlias().find(strFilter) == string::npos &&
            maxe.getIp().find(strFilter) == string::npos &&
            maxe.getTxHash().find(strFilter) == string::npos &&
            strStatus.find(strFilter) == string::npos) continue;

        UniValue maxObj(UniValue::VOBJ);
        maxObj.push_back(Pair("alias", maxe.getAlias()));
        maxObj.push_back(Pair("address", maxe.getIp()));
        maxObj.push_back(Pair("privateKey", maxe.getPrivKey()));
        maxObj.push_back(Pair("txHash", maxe.getTxHash()));
        maxObj.push_back(Pair("outputIndex", maxe.getOutputIndex()));
        maxObj.push_back(Pair("status", strStatus));
        ret.push_back(maxObj);
    }

    return ret;
}

UniValue getmaxnodestatus (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw runtime_error(
            "getmaxnodestatus\n"
            "\nPrint maxnode status\n"

            "\nResult:\n"
            "{\n"
            "  \"txhash\": \"xxxx\",      (string) Collateral transaction hash\n"
            "  \"outputidx\": n,        (numeric) Collateral transaction output index number\n"
            "  \"netaddr\": \"xxxx\",     (string) Maxnode network address\n"
            "  \"addr\": \"xxxx\",        (string) Lytix address for maxnode payments\n"
            "  \"status\": \"xxxx\",      (string) Maxnode status\n"
            "  \"message\": \"xxxx\"      (string) Maxnode status message\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmaxnodestatus", "") + HelpExampleRpc("getmaxnodestatus", ""));

    //if (!fMaxNodeT1 || !fMaxNodeT2 || !fMaxNodeT3) throw runtime_error("This is not a maxnode");
    if (!fMaxNode) throw runtime_error("This is not a maxnode");

    CMaxnode* pmax = maxnodeman.Find(activeMaxnode.maxvin);

    if (pmax) {
        UniValue maxObj(UniValue::VOBJ);
        maxObj.push_back(Pair("txhash", activeMaxnode.maxvin.prevout.hash.ToString()));
        maxObj.push_back(Pair("outputidx", (uint64_t)activeMaxnode.maxvin.prevout.n));
        maxObj.push_back(Pair("netaddr", activeMaxnode.service.ToString()));
        maxObj.push_back(Pair("addr", CBitcoinAddress(pmax->pubKeyCollateralAddress.GetID()).ToString()));
        maxObj.push_back(Pair("status", activeMaxnode.status));
        maxObj.push_back(Pair("message", activeMaxnode.GetStatus()));
        return maxObj;
    }
    throw runtime_error("Maxnode not found in the list of available maxnodes. Current status: "
                        + activeMaxnode.GetStatus());
}

UniValue getmaxnodewinners (const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "getmaxnodewinners ( blocks \"filter\" )\n"
            "\nPrint the maxnode winners for the last n blocks\n"

            "\nArguments:\n"
            "1. blocks      (numeric, optional) Number of previous blocks to show (default: 10)\n"
            "2. filter      (string, optional) Search filter matching MAX address\n"

            "\nResult (single winner):\n"
            "[\n"
            "  {\n"
            "    \"nHeight\": n,           (numeric) block height\n"
            "    \"winner\": {\n"
            "      \"address\": \"xxxx\",    (string) Lytix MAX Address\n"
            "      \"nVotes\": n,          (numeric) Number of votes for winner\n"
            "    }\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nResult (multiple winners):\n"
            "[\n"
            "  {\n"
            "    \"nHeight\": n,           (numeric) block height\n"
            "    \"winner\": [\n"
            "      {\n"
            "        \"address\": \"xxxx\",  (string) Lytix MAX Address\n"
            "        \"nVotes\": n,        (numeric) Number of votes for winner\n"
            "      }\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getmaxnodewinners", "") + HelpExampleRpc("getmaxnodewinners", ""));

    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return 0;
        nHeight = pindex->nHeight;
    }

    int nLast = 10;
    std::string strFilter = "";

    if (params.size() >= 1)
        nLast = atoi(params[0].get_str());

    if (params.size() == 2)
        strFilter = params[1].get_str();

    UniValue ret(UniValue::VARR);

    for (int i = nHeight - nLast; i < nHeight + 20; i++) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("nHeight", i));

        std::string strPayment = GetMaxRequiredPaymentsString(i);
        if (strFilter != "" && strPayment.find(strFilter) == std::string::npos) continue;

        if (strPayment.find(',') != std::string::npos) {
            UniValue winner(UniValue::VARR);
            boost::char_separator<char> sep(",");
            boost::tokenizer< boost::char_separator<char> > tokens(strPayment, sep);
            BOOST_FOREACH (const string& t, tokens) {
                UniValue addr(UniValue::VOBJ);
                std::size_t pos = t.find(":");
                std::string strAddress = t.substr(0,pos);
                uint64_t nVotes = atoi(t.substr(pos+1));
                addr.push_back(Pair("address", strAddress));
                addr.push_back(Pair("nVotes", nVotes));
                winner.push_back(addr);
            }
            obj.push_back(Pair("winner", winner));
        } else if (strPayment.find("Unknown") == std::string::npos) {
            UniValue winner(UniValue::VOBJ);
            std::size_t pos = strPayment.find(":");
            std::string strAddress = strPayment.substr(0,pos);
            uint64_t nVotes = atoi(strPayment.substr(pos+1));
            winner.push_back(Pair("address", strAddress));
            winner.push_back(Pair("nVotes", nVotes));
            obj.push_back(Pair("winner", winner));
        } else {
            UniValue winner(UniValue::VOBJ);
            winner.push_back(Pair("address", strPayment));
            winner.push_back(Pair("nVotes", 0));
            obj.push_back(Pair("winner", winner));
        }

            ret.push_back(obj);
    }

    return ret;
}

UniValue getmaxnodescores (const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getmaxnodescores ( blocks )\n"
            "\nPrint list of winning maxnode by score\n"

            "\nArguments:\n"
            "1. blocks      (numeric, optional) Show the last n blocks (default 10)\n"

            "\nResult:\n"
            "{\n"
            "  xxxx: \"xxxx\"   (numeric : string) Block height : Maxnode hash\n"
            "  ,...\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmaxnodescores", "") + HelpExampleRpc("getmaxnodescores", ""));

    int nLast = 10;

    if (params.size() == 1) {
        try {
            nLast = std::stoi(params[0].get_str());
        } catch (const boost::bad_lexical_cast &) {
            throw runtime_error("Exception on param 2");
        }
    }
    UniValue obj(UniValue::VOBJ);

    std::vector<CMaxnode> vMaxnodes = maxnodeman.GetFullMaxnodeVector();
    for (int nHeight = chainActive.Tip()->nHeight - nLast; nHeight < chainActive.Tip()->nHeight + 20; nHeight++) {
        uint256 nHigh = 0;
        CMaxnode* pBestMaxnode = NULL;
        BOOST_FOREACH (CMaxnode& max, vMaxnodes) {
            uint256 n = max.CalculateScore(1, nHeight - 100);
            if (n > nHigh) {
                nHigh = n;
                pBestMaxnode = &max;
            }
        }
        if (pBestMaxnode)
            obj.push_back(Pair(strprintf("%d", nHeight), pBestMaxnode->maxvin.prevout.hash.ToString().c_str()));
    }

    return obj;
}

bool DecodeHexMnb(CMaxnodeBroadcast& maxb, std::string strHexMnb) {

    if (!IsHex(strHexMnb))
        return false;

    vector<unsigned char> maxbData(ParseHex(strHexMnb));
    CDataStream ssData(maxbData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> maxb;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}
UniValue createmaxnodebroadcast(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();
    if (fHelp || (strCommand != "alias" && strCommand != "all") || (strCommand == "alias" && params.size() < 2))
        throw runtime_error(
            "createmaxnodebroadcast \"command\" ( \"alias\")\n"
            "\nCreates a maxnode broadcast message for one or all maxnodes configured in maxnode.conf\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"command\"      (string, required) \"alias\" for single maxnode, \"all\" for all maxnodes\n"
            "2. \"alias\"        (string, required if command is \"alias\") Alias of the maxnode\n"

            "\nResult (all):\n"
            "{\n"
            "  \"overall\": \"xxx\",        (string) Overall status message indicating number of successes.\n"
            "  \"detail\": [                (array) JSON array of broadcast objects.\n"
            "    {\n"
            "      \"alias\": \"xxx\",      (string) Alias of the maxnode.\n"
            "      \"success\": true|false, (boolean) Success status.\n"
            "      \"hex\": \"xxx\"         (string, if success=true) Hex encoded broadcast message.\n"
            "      \"error_message\": \"xxx\"   (string, if success=false) Error message, if any.\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nResult (alias):\n"
            "{\n"
            "  \"alias\": \"xxx\",      (string) Alias of the maxnode.\n"
            "  \"success\": true|false, (boolean) Success status.\n"
            "  \"hex\": \"xxx\"         (string, if success=true) Hex encoded broadcast message.\n"
            "  \"error_message\": \"xxx\"   (string, if success=false) Error message, if any.\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("createmaxnodebroadcast", "alias mymax1") + HelpExampleRpc("createmaxnodebroadcast", "alias mymax1"));

    EnsureWalletIsUnlocked();

    if (strCommand == "alias")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        std::string alias = params[1].get_str();
        bool found = false;

        UniValue statusObj(UniValue::VOBJ);
        statusObj.push_back(Pair("alias", alias));

        BOOST_FOREACH(CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
            if(maxe.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                CMaxnodeBroadcast maxb;

                bool success = activeMaxnode.CreateBroadcast(maxe.getIp(), maxe.getPrivKey(), maxe.getTxHash(), maxe.getOutputIndex(), errorMessage, maxb, true);

                statusObj.push_back(Pair("success", success));
                if(success) {
                    CDataStream ssMnb(SER_NETWORK, PROTOCOL_VERSION);
                    ssMnb << maxb;
                    statusObj.push_back(Pair("hex", HexStr(ssMnb.begin(), ssMnb.end())));
                } else {
                    statusObj.push_back(Pair("error_message", errorMessage));
                }
                break;
            }
        }

        if(!found) {
            statusObj.push_back(Pair("success", false));
            statusObj.push_back(Pair("error_message", "Could not find alias in config. Verify with list-conf."));
        }

        return statusObj;

    }

    if (strCommand == "all")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        std::vector<CMaxnodeConfig::CMaxnodeEntry> maxEntries;
        maxEntries = maxnodeConfig.getEntries();

        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);

        BOOST_FOREACH(CMaxnodeConfig::CMaxnodeEntry maxe, maxnodeConfig.getEntries()) {
            std::string errorMessage;

            CTxIn maxvin = CTxIn(uint256S(maxe.getTxHash()), uint32_t(atoi(maxe.getOutputIndex().c_str())));
            CMaxnodeBroadcast maxb;

            bool success = activeMaxnode.CreateBroadcast(maxe.getIp(), maxe.getPrivKey(), maxe.getTxHash(), maxe.getOutputIndex(), errorMessage, maxb, true);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", maxe.getAlias()));
            statusObj.push_back(Pair("success", success));

            if(success) {
                successful++;
                CDataStream ssMnb(SER_NETWORK, PROTOCOL_VERSION);
                ssMnb << maxb;
                statusObj.push_back(Pair("hex", HexStr(ssMnb.begin(), ssMnb.end())));
            } else {
                failed++;
                statusObj.push_back(Pair("error_message", errorMessage));
            }

            resultsObj.push_back(statusObj);
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Successfully created broadcast messages for %d maxnodes, failed to create %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }
    return NullUniValue;
}

UniValue decodemaxnodebroadcast(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "decodemaxnodebroadcast \"hexstring\"\n"
            "\nCommand to decode maxnode broadcast messages\n"

            "\nArgument:\n"
            "1. \"hexstring\"        (string) The hex encoded maxnode broadcast message\n"

            "\nResult:\n"
            "{\n"
            "  \"maxvin\": \"xxxx\"                (string) The unspent output which is holding the maxnode collateral\n"
            "  \"addr\": \"xxxx\"               (string) IP address of the maxnode\n"
            "  \"pubkeycollateral\": \"xxxx\"   (string) Collateral address's public key\n"
            "  \"pubkeymaxnode\": \"xxxx\"   (string) Maxnode's public key\n"
            "  \"vchsig\": \"xxxx\"             (string) Base64-encoded signature of this message (verifiable via pubkeycollateral)\n"
            "  \"sigtime\": \"nnn\"             (numeric) Signature timestamp\n"
            "  \"protocolversion\": \"nnn\"     (numeric) Maxnode's protocol version\n"
            "  \"nlastdsq\": \"nnn\"            (numeric) The last time the maxnode sent a DSQ message (for mixing) (DEPRECATED)\n"
            "  \"lastping\" : {                 (object) JSON object with information about the maxnode's last ping\n"
            "      \"maxvin\": \"xxxx\"            (string) The unspent output of the maxnode which is signing the message\n"
            "      \"blockhash\": \"xxxx\"      (string) Current chaintip blockhash minus 12\n"
            "      \"sigtime\": \"nnn\"         (numeric) Signature time for this ping\n"
            "      \"vchsig\": \"xxxx\"         (string) Base64-encoded signature of this ping (verifiable via pubkeymaxnode)\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("decodemaxnodebroadcast", "hexstring") + HelpExampleRpc("decodemaxnodebroadcast", "hexstring"));

    CMaxnodeBroadcast maxb;

    if (!DecodeHexMnb(maxb, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Maxnode broadcast message decode failed");

    if(!maxb.VerifySignature())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maxnode broadcast signature verification failed");

    UniValue resultObj(UniValue::VOBJ);

    resultObj.push_back(Pair("maxvin", maxb.maxvin.prevout.ToString()));
    resultObj.push_back(Pair("addr", maxb.addr.ToString()));
    resultObj.push_back(Pair("pubkeycollateral", CBitcoinAddress(maxb.pubKeyCollateralAddress.GetID()).ToString()));
    resultObj.push_back(Pair("pubkeymaxnode", CBitcoinAddress(maxb.pubKeyMaxnode.GetID()).ToString()));
    resultObj.push_back(Pair("vchsig", EncodeBase64(&maxb.sig[0], maxb.sig.size())));
    resultObj.push_back(Pair("sigtime", maxb.sigTime));
    resultObj.push_back(Pair("protocolversion", maxb.protocolVersion));
    resultObj.push_back(Pair("nlastdsq", maxb.nLastDsq));

    UniValue lastPingObj(UniValue::VOBJ);
    lastPingObj.push_back(Pair("maxvin", maxb.lastPing.maxvin.prevout.ToString()));
    lastPingObj.push_back(Pair("blockhash", maxb.lastPing.blockHash.ToString()));
    lastPingObj.push_back(Pair("sigtime", maxb.lastPing.sigTime));
    lastPingObj.push_back(Pair("vchsig", EncodeBase64(&maxb.lastPing.vchSig[0], maxb.lastPing.vchSig.size())));

    resultObj.push_back(Pair("lastping", lastPingObj));

    return resultObj;
}

UniValue relaymaxnodebroadcast(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "relaymaxnodebroadcast \"hexstring\"\n"
            "\nCommand to relay maxnode broadcast messages\n"

            "\nArguments:\n"
            "1. \"hexstring\"        (string) The hex encoded maxnode broadcast message\n"

            "\nExamples:\n" +
            HelpExampleCli("relaymaxnodebroadcast", "hexstring") + HelpExampleRpc("relaymaxnodebroadcast", "hexstring"));


    CMaxnodeBroadcast maxb;

    if (!DecodeHexMnb(maxb, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Maxnode broadcast message decode failed");

    if(!maxb.VerifySignature())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Maxnode broadcast signature verification failed");

    maxnodeman.UpdateMaxnodeList(maxb);
    maxb.Relay();

    return strprintf("Maxnode broadcast sent (service %s, maxvin %s)", maxb.addr.ToString(), maxb.maxvin.ToString());
}

