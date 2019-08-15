// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018 The Helium developers
// Copyright (c) 2018-2019 The Lytix developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Params.h"
#include "chainparams.h"
#include "amount.h"
#include "base58.h"
#include "random.h"
#include "util.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "spork.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */
static bool regenerate = false;

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

//   What makes a good checkpoint block?
// + Is surrounded by blocks with reasonable timestamps
//   (no blocks before with a timestamp after, none after with
//    timestamp before)
// + Contains no strange transactions
static Checkpoints::MapCheckpoints mapCheckpoints =
    boost::assign::map_list_of
    (0, uint256("0x0000028b94db22c97211ba15866bc2c9bc0daa7e9ac410a6c9660d8be19915c7"))
    (6942, uint256("0x00000010c715f94b9bfe7666f1f81b4bf20c4cb1fcd67a8c75305d6bf9372a20"))
    (30000, uint256("0x00000000c568c33cf4b881a256e779e7c73b7e326db2f603a0ce16277506baa5"))
    (60000, uint256("0x000000000222d4eb292e05aa35b4959f0705c77520d5e3c4980172988483db36"))
    (101000, uint256("0xdf8bbe946d173b8bb266ddec0195dc415baadd332dfbedd79b4668ba69893aac"))
    (170000, uint256("0xdfb4f14e31f62318ada32144417258b7ce040865343477641aabb0ff739508f1"))
    (252000, uint256("0xb9e9141619cd48dddbd75a033f171b3cd9317f748d6a1ba8ce455ad2662c28d2"))
    (312987, uint256("0x5d3dda2359f5e8c33b6eddd1a5cd35a4c8ac8036f779160176b4bfdb672da84c"))
    (320547, uint256("0xb5746d774e01cd391b685c45b67e49bd10dd982594ca9db07753672a0eecae9e"))
    (338259, uint256("0x2d8d514a8d25b12d539be6d3f19203ba775eda6089509ffaa2d2170f4db814e1"))
    (342484, uint256("0x04256ee6229e65c0d8062dae8a25ff8e6157e036f0b5cddfce2e8998d1057ff0"))
    (349570, uint256("0xd3ceb4f4434405b96317b3f79bbd635a1417e7c4af12785fb43068c326e6b69b"));
static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints,
    1565880982, // * UNIX timestamp of last checkpoint block
    653370,    // * total number of transactions between genesis and last checkpoint
                //   (the tx=... number in the SetBestChain debug.log lines)
    1440        // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of
    (0, uint256("0x000007069b3a40677ea1f2359f9edd27f952a13549e78f29cc17a758c338a4a7"));
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1544199012,
    0,
    250};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of
    (0, uint256("0x257231f8c651f8b8238253358ef7b7debbfeddf7583a578950a58cadb1bb272e"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1544199012,
    0,
    100};

libzerocoin::ZerocoinParams* CChainParams::Zerocoin_Params(bool useModulusV1) const
{
    assert(this);
    static CBigNum bnHexModulus = 0;
    if (!bnHexModulus)
        bnHexModulus.SetHex(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsHex = libzerocoin::ZerocoinParams(bnHexModulus);
    static CBigNum bnDecModulus = 0;
    if (!bnDecModulus)
        bnDecModulus.SetDec(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsDec = libzerocoin::ZerocoinParams(bnDecModulus);

    if (useModulusV1)
        return &ZCParamsHex;

    return &ZCParamsDec;
}

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 504365040 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    {
	//Dev - Governance start
        txNew.vout[0].nValue = 50000000000000;
        txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex(DecodeBase58ToHex(std::string("8p2Kso1HYUUzbKzPjk6cqsuLt65y94ydgF"))) << OP_EQUALVERIFY << OP_CHECKSIG;
	genesis.vtx.push_back(txNew);
	txNew.vout[0].nValue = 13371711343;
        txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex(DecodeBase58ToHex(std::string("8pjhs4DBNYLtsJ5ApGnkePnaFWWAX6MyaA"))) << OP_EQUALVERIFY << OP_CHECKSIG;
genesis.vtx.push_back(txNew);
        txNew.vout[0].nValue = 13371173343;
        txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex(DecodeBase58ToHex(std::string("8tjcjzkrtdaxmn43Z7WnBu47iqbmZJ87Nh"))) << OP_EQUALVERIFY << OP_CHECKSIG;
genesis.vtx.push_back(txNew);
        txNew.vout[0].nValue = 13311173343;
        txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex(DecodeBase58ToHex(std::string("8wJ51eDEtq1VgHBmg384upGJxzE2Rjco8i"))) << OP_EQUALVERIFY << OP_CHECKSIG;
genesis.vtx.push_back(txNew);
        txNew.vout[0].nValue = 1337713343;
        txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex(DecodeBase58ToHex(std::string("8xag7xZ3LPeTrTSiCvH3oCeiV6v7iy8aUa"))) << OP_EQUALVERIFY << OP_CHECKSIG;

    }
    genesis.hashPrevBlock = 0;
    genesis.hashMerkleRoot = genesis.BuildMerkleTree();
    return genesis;
};

static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward, const string addr1, const string addr2, const string addr3)
{
    const char* pszTimestamp = "Bitcoin Block 552860 - 0000000000000000002ab7cede604dedd982fe483fb0ab6c79d8574083758d7f at 2018-12-07 05:39:05";
    const CScript genesisOutputScript = CScript() << OP_2 << ParseHex(DecodeBase58ToHex(addr1)) << ParseHex(DecodeBase58ToHex(addr2)) << ParseHex(DecodeBase58ToHex(addr3)) << OP_3 << OP_CHECKMULTISIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
};

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0x5f;
        pchMessageStart[1] = 0x2a;
        pchMessageStart[2] = 0xc3;
        pchMessageStart[3] = 0xb4;
        vAlertPubKey = ParseHex("0x"); // Disabled
        nDefaultPort = 27071;
        bnProofOfWorkLimit = ~uint256(0) >> 20; // Lytix starting difficulty is 1 / 2^12
        nSubsidyHalvingInterval = 210240;
        nMaxReorganizationDepth = 1000;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 24 * 60 * 60; // Lytix: 1 day
        //nTargetSpacing = 2 * 60;  // Lytix: 2 minutes
        nTargetSpacing = 60;  // Lytix: 2 minutes
        nMaturity = 15;
        nMasternodeCountDrift = 20;
        nMaxnodeCountDrift = 20;
        nMaxMoneyOut = 100000000 * COIN;
	strDevFeeAddress = "8s8jUWMPFFwGSsGE9JX1z95sCbYhiyPUeg";

        /** Height or Time Based Activations **/
        nLastPOWBlock = 100000; //  1 per minute  - 14 days @ 1440 per day
        //if the lowest block height (vSortedByTimestamp[0]) is >= switch height, use new modifier calc
        nZerocoinStartHeight = 99999999; // (PIVX: 863787, Phore 90000)
        nZerocoinStartTime = 3999999990; //  Tuesday, November 27, 2018 3:53:30 PM
        nBlockRecalculateAccumulators = 710000; // (PIVX: 895400, Phore 90005) //Trigger a recalculation of accumulators
        nBlockLastGoodCheckpoint = 170000; // (PIVX: 891730, Phore 90005) //Last valid accumulator checkpoint
        nBlockZerocoinV2 = 999999999; // (PIVX: 1153160) //!> The block that zerocoin v2 becomes active - roughly Tuesday, May 8, 2018 4:00:00 AM GMT
        nEnforceNewSporkKey = 1544199012; // (PIVX: 1525158000) //!> Sporks signed after (GMT): Tuesday, May 1, 2018 7:00:00 AM GMT must use the new spork key
        nRejectOldSporkKey = 1544199012; // (PIVX: 1527811200) //!> Fully reject old spork key after (GMT): Friday, June 1, 2018 12:00:00 AM

        genesis = CreateGenesisBlock(
                    1544199012,                          // nTime
                    92065,                               // nNonce
                    0x1e0ffff0,                          // nBits
                    3,                                   // nVersion
                   treasuryDeposit,                      // genesisReward (treasury deposit)
                   "8eqCEt9zBB8VvLansnFWSeo1bD4dUeeJQm",  // first NEW treasury address
                   "8ye6Kprb2GuTjQWLuKFxPdxAZcUx1re3Kf",  // second NEW treasury address
                   "8miQ2oYixNeESmPy7M2AXZKnt5hrzLdsbc"  // third NEW treasury address
                    );

        hashGenesisBlock = genesis.GetHash();
        if (regenerate) {
            hashGenesisBlock = uint256S("");
            genesis.nNonce = 0;
            if (true && (genesis.GetHash() != hashGenesisBlock)) {
                uint256 hashTarget = CBigNum().SetCompact(genesis.nBits).getuint256();
                while (genesis.GetHash() > hashTarget)
                {
                    ++genesis.nNonce;
                    if (genesis.nNonce == 0)
                    {
                        ++genesis.nTime;
                    }
                }
                std::cout << "// Mainnet ---";
                std::cout << " nonce: " << genesis.nNonce;
                std::cout << " time: " << genesis.nTime;
                std::cout << " hash: 0x" << genesis.GetHash().ToString().c_str();
                std::cout << " merklehash: 0x"  << genesis.hashMerkleRoot.ToString().c_str() <<  "\n";



            }
        } else {
            LogPrintf("Mainnet ---\n");
            LogPrintf(" nonce: %u\n", genesis.nNonce);
            LogPrintf(" time: %u\n", genesis.nTime);
            LogPrintf(" hash: 0x%s\n", genesis.GetHash().ToString().c_str());
            LogPrintf(" merklehash: 0x%s\n", genesis.hashMerkleRoot.ToString().c_str());
            assert(hashGenesisBlock == uint256("0x0000028b94db22c97211ba15866bc2c9bc0daa7e9ac410a6c9660d8be19915c7"));
            assert(genesis.hashMerkleRoot == uint256("0x64e18e2d42f10266b1963f8f428faf93ffcf7c75795b1f06918726aa2e94dd79"));
        }


        vSeeds.push_back(CDNSSeedData("dns1", "dns1.lytixchain.org"));
        vSeeds.push_back(CDNSSeedData("dns2", "dns2.lytixchain.org"));
        vSeeds.push_back(CDNSSeedData("dns3", "dns3.lytixchain.org"));
        vSeeds.push_back(CDNSSeedData("dns4", "66.42.73.80"));
        vSeeds.push_back(CDNSSeedData("dnsv6", "2001:19f0:6401:dbd:5400:01ff:fe9a:0628"));


        // Lytix addresses start with '8 or 9'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,19);
        // Lytix script addresses start with '3'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,11);
        // Lytix private keys start with '7' (uncompressed) or 'V' (compressed)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,17);
        // Lytix BIP32 pubkeys start with 'xpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        // Lytix BIP32 prvkeys start with 'xprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();
        // Lytix BIP44 coin type (pending BIP44-capable wallet, use Bitcoin type)
        base58Prefixes[EXT_COIN_TYPE]  = boost::assign::list_of(0x80)(0x00)(0x00)(0xe2).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fHeadersFirstSyncingActive = false;

        nPoolMaxTransactions = 3;
	strSporkKey = "04F3226951791F52C149C34813316E03CCCDB99FA6FBBB51CB71E3184533A532A66C41FB9E17FF34FDCE8A33742F466A71F16861A7750C950573E2446356566D92";
        strSporkKeyOld = "0499A7AF4806FC6DE640D23BC5936C29B77ADF2174B4F45492727F897AE63CF8D27B2F05040606E0D14B547916379FA10716E344E745F880EDC037307186AA25B7";
        strObfuscationPoolDummyAddress = "S87q2gC9j6nNrnzCsg4aY6bHMLsT9nUhEw";
        nStartMasternodePayments = 1527634800; // 2018-05-30 00:00:00

        /** Zerocoin */
        zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";
        nMaxZerocoinSpendsPerTransaction = 7; // Assume about 20kb each
        nMinZerocoinMintFee = 1 * CENT; //high fee required for zerocoin mints
        nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        nRequiredAccumulation = 1;
        nDefaultSecurityLevel = 100; //full security level for accumulators
        nZerocoinHeaderVersion = 4; //Block headers must be this version once zerocoin is active
        nZerocoinRequiredStakeDepth = 50; //The required confirmations for a zpiv to be stakable

        nBudget_Fee_Confirmations = 6; // Number of confirmations for the finalization fee
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0xf0;
        pchMessageStart[1] = 0xa0;
        pchMessageStart[2] = 0x0c;
        pchMessageStart[3] = 0x0e;
        vAlertPubKey = ParseHex("");
        bnProofOfWorkLimit = ~uint256(0) >> 1; // 0x207fffff, Lytix testnet starting difficulty
        nSubsidyHalvingInterval = 210240;
        nDefaultPort = 27072;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nMinerThreads = 1;
        nTargetTimespan = 24 * 60 * 60; // Lytix: 1 day
        nTargetSpacing = 60;  // Lytix: 2 minute
        nLastPOWBlock = 250;
        nMaturity = 15;
        nMasternodeCountDrift = 2;
        nMaxnodeCountDrift = 2;
        // nModifierUpdateBlock = 0; //approx Mon, 17 Apr 2017 04:00:00 GMT
        nMaxMoneyOut = 100000000 * COIN;
	strDevFeeAddress = "DH7beVFvykse3ZHzZP3Q3uNiHUnd1z22v3";
        nZerocoinStartHeight = 100000;
        nZerocoinStartTime = 1630801782;
        // nBlockEnforceSerialRange = 0; //Enforce serial range starting this block
        nBlockRecalculateAccumulators = 999999999; //Trigger a recalculation of accumulators
        // nBlockFirstFraudulent = 999999999; //First block that bad serials emerged
        nBlockLastGoodCheckpoint = 999999999; //Last valid accumulator checkpoint
        // nBlockEnforceInvalidUTXO = 0; //Start enforcing the invalid UTXO's
        // nInvalidAmountFiltered = 0; //Amount of invalid coins filtered through exchanges, that should be considered valid
        nBlockZerocoinV2 = 99999999; //!> The block that zerocoin v2 becomes active
        nEnforceNewSporkKey = 1544199012; //!> Sporks signed after Wednesday, March 21, 2018 4:00:00 AM GMT must use the new spork key
        nRejectOldSporkKey = 1544199012; //!> Reject old spork key after Saturday, March 31, 2018 12:00:00 AM GMT

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis = CreateGenesisBlock(
                    1544199012,                          // nTime
                    822163,                              // nNonce
                    0x1e0ffff0,                          // nBits
                    3,                                   // nVersion
                    treasuryDeposit,                     // genesisReward (treasury deposit)
                   "msYBKKuARmcmMHmzMGxc8j8XYi8bqEeBJr", // first treasury address
                   "mfYcNVRnTNHDw6BwKPhk3Z8xXp4F8CEMtZ", // second treasury address
                   "mkGv2mrvZvKriZ1tzwQheHQzYyW5o9ir5J"  // third treasury address
                    );

        hashGenesisBlock = genesis.GetHash();
        if (regenerate) {
            hashGenesisBlock = uint256S("");
            genesis.nNonce = 0;
            if (true && (genesis.GetHash() != hashGenesisBlock)) {
                uint256 hashTarget = CBigNum().SetCompact(genesis.nBits).getuint256();
                while (genesis.GetHash() > hashTarget)
                {
                    ++genesis.nNonce;
                    if (genesis.nNonce == 0)
                    {
                        ++genesis.nTime;
                    }
                }
                std::cout << "// Testnet ---";
                std::cout << " nonce: " << genesis.nNonce;
                std::cout << " time: " << genesis.nTime;
                std::cout << " hash: 0x" << genesis.GetHash().ToString().c_str();
                std::cout << " merklehash: 0x"  << genesis.hashMerkleRoot.ToString().c_str() <<  "\n";

            }
        } else {
            LogPrintf("Testnet ---\n");
            LogPrintf(" nonce: %u\n", genesis.nNonce);
            LogPrintf(" time: %u\n", genesis.nTime);
            LogPrintf(" hash: 0x%s\n", genesis.GetHash().ToString().c_str());
            LogPrintf(" merklehash: 0x%s\n", genesis.hashMerkleRoot.ToString().c_str());
            assert(hashGenesisBlock == uint256("0x000007069b3a40677ea1f2359f9edd27f952a13549e78f29cc17a758c338a4a7"));
            assert(genesis.hashMerkleRoot == uint256("0x31bf9524b28f8051e1abfa85ac7f1f810d42494563ed893b38b95dfa9acb82fd"));
        }

        vFixedSeeds.clear();
        vSeeds.clear();

        vSeeds.push_back(CDNSSeedData("testnet", "testnet.lytixchain.org"));

        // Testnet Lytix addresses start with 'X'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,30);
        // Testnet Lytix script addresses start with 'Y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,22);
        // Testnet private keys start with 'X' (uncompressed) or 'c' (compressed)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,9);
        // Testnet Lytix BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Testnet Lytix BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
        // Testnet Lytix BIP44 coin type is '1' (All coin's testnet default)
        base58Prefixes[EXT_COIN_TYPE]  = boost::assign::list_of(0x80)(0x00)(0x00)(0x01).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;
        fSkipProofOfWorkCheck = true;

        nPoolMaxTransactions = 2;
        strSporkKey = "04270B33151E56FA36F750D4DF70FA3926EED07B2BE9BDBA36371024828A6CBC5211939514ED9F35E0B99B89BEB92987926F4067942F6BFA7E1B8D5363E5CE9E74";
        strSporkKeyOld = "";
        strObfuscationPoolDummyAddress = "m57cqfGRkekRyDRNeJiLtYVEbvhXrNbmox";
        nStartMasternodePayments = 1527634800; //30th May 2018 00:00:00
        nBudget_Fee_Confirmations = 3; // Number of confirmations for the finalization fee. We have to make this very short
                                       // here because we only have a 8 block finalization window on testnet
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xe0;
        pchMessageStart[1] = 0x2a;
        pchMessageStart[2] = 0xb4;
        pchMessageStart[3] = 0x6e;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 24 * 60 * 60; // Lytix: 1 day
        nTargetSpacing = 60;  // Lytix: 1 minute
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        nDefaultPort = 27075;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis = CreateGenesisBlock(
                    1544199012,                          // nTime
                    22,                                   // nNonce
                    0x207fffff,                          // nBits
                    3,                                   // nVersion
                    treasuryDeposit,                     // genesisReward (treasury deposit)
                   "8eqCEt9zBB8VvLansnFWSeo1bD4dUeeJQm",  // first NEW treasury address
                   "8ye6Kprb2GuTjQWLuKFxPdxAZcUx1re3Kf",  // second NEW treasury address
                   "8miQ2oYixNeESmPy7M2AXZKnt5hrzLdsbc"  // third NEW treasury address
                    );

        hashGenesisBlock = genesis.GetHash();

        if (regenerate) {
            hashGenesisBlock = uint256S("");
            if (true && (genesis.GetHash() != hashGenesisBlock)) {
                uint256 hashTarget = CBigNum().SetCompact(genesis.nBits).getuint256();
                while (genesis.GetHash() > hashTarget)
                {
                    ++genesis.nNonce;
                    if (genesis.nNonce == 0)
                    {
                        ++genesis.nTime;
                    }
                }
                std::cout << "// Regtestnet ---";
                std::cout << " nonce: " << genesis.nNonce;
                std::cout << " time: " << genesis.nTime;
                std::cout << " hash: 0x" << genesis.GetHash().ToString().c_str();
                std::cout << " merklehash: 0x"  << genesis.hashMerkleRoot.ToString().c_str() <<  "\n";

            }
        } else {
            LogPrintf("Regtestnet ---\n");
            LogPrintf(" nonce: %u\n", genesis.nNonce);
            LogPrintf(" time: %u\n", genesis.nTime);
            LogPrintf(" hash: 0x%s\n", genesis.GetHash().ToString().c_str());
            LogPrintf(" merklehash: 0x%s\n", genesis.hashMerkleRoot.ToString().c_str());
            assert(hashGenesisBlock == uint256("0x257231f8c651f8b8238253358ef7b7debbfeddf7583a578950a58cadb1bb272e"));
            assert(genesis.hashMerkleRoot == uint256("0x64e18e2d42f10266b1963f8f428faf93ffcf7c75795b1f06918726aa2e94dd79"));
        }


        if (regenerate)
            exit(0);

        vFixedSeeds.clear(); //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Testnet mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;
        // fSkipProofOfWorkCheck = true;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams
{
public:
    CUnitTestParams()
    {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 27076;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Unit test mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    virtual void setSubsidyHalvingInterval(int anSubsidyHalvingInterval) { nSubsidyHalvingInterval = anSubsidyHalvingInterval; }
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority) { nEnforceBlockUpgradeMajority = anEnforceBlockUpgradeMajority; }
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority) { nRejectBlockOutdatedMajority = anRejectBlockOutdatedMajority; }
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority) { nToCheckBlockUpgradeMajority = anToCheckBlockUpgradeMajority; }
    virtual void setDefaultConsistencyChecks(bool afDefaultConsistencyChecks) { fDefaultConsistencyChecks = afDefaultConsistencyChecks; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) { fAllowMinDifficultyBlocks = afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams* pCurrentParams = 0;

CModifiableParams* ModifiableParams()
{
    assert(pCurrentParams);
    assert(pCurrentParams == &unitTestParams);
    return (CModifiableParams*)&unitTestParams;
}

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    case CBaseChainParams::UNITTEST:
        return unitTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
