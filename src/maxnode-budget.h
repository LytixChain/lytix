// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2019 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAXNODE_BUDGET_H
#define MAXNODE_BUDGET_H

#include "base58.h"
#include "init.h"
#include "key.h"
#include "main.h"
#include "maxnode.h"
#include "net.h"
#include "sync.h"
#include "util.h"
#include <boost/lexical_cast.hpp>

using namespace std;

extern CCriticalSection cs_budget;

class CMAXBudgetManager;
class CMAXFinalizedBudgetBroadcast;
class CMAXFinalizedBudget;
class CMAXBudgetProposal;
class CMAXBudgetProposalBroadcast;
class CMAXTxBudgetPayment;

#define VOTE_ABSTAIN 0
#define VOTE_YES 1
#define VOTE_NO 2

enum class MAXTrxValidationStatus {
    InValid,         /** Transaction verification failed */
    Valid,           /** Transaction successfully verified */
    DoublePayment,   /** Transaction successfully verified, but includes a double-budget-payment */
    VoteThreshold    /** If not enough maxnodes have voted on a finalized budget */
};

static const CAmount MAX_PROPOSAL_FEE_TX = (50 * COIN);
static const CAmount MAX_BUDGET_FEE_TX_OLD = (50 * COIN);
static const CAmount MAX_BUDGET_FEE_TX = (5 * COIN);
static const int64_t MAX_BUDGET_VOTE_UPDATE_MIN = 60 * 60;
static map<uint256, int> mapMaxPayment_History;

extern std::vector<CMAXBudgetProposalBroadcast> vecMAXImmatureBudgetProposals;
extern std::vector<CMAXFinalizedBudgetBroadcast> vecMAXImmatureFinalizedBudgets;

extern CMAXBudgetManager maxbudget;
void DumpMaxBudgets();

// Define amount of blocks in budget payment cycle
int GetMaxBudgetPaymentCycleBlocks();

//Check the collateral transaction for the budget proposal/finalized budget
bool IsMaxBudgetCollateralValid(uint256 nTxCollateralHash, uint256 nExpectedHash, std::string& strError, int64_t& nTime, int& nConf, bool fBudgetFinalization=false);

//
// CMAXBudgetVote - Allow a maxnode node to vote and broadcast throughout the network
//

class CMAXBudgetVote
{
public:
    bool fValid;  //if the vote is currently valid / counted
    bool fSynced; //if we've sent this to our peers
    CTxIn vin;
    uint256 nProposalHash;
    int nVote;
    int64_t nTime;
    std::vector<unsigned char> vchSig;

    CMAXBudgetVote();
    CMAXBudgetVote(CTxIn vin, uint256 nProposalHash, int nVoteIn);

    bool Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode);
    bool SignatureValid(bool fSignatureCheck);
    void Relay();

    std::string GetVoteString()
    {
        std::string ret = "ABSTAIN";
        if (nVote == VOTE_YES) ret = "YES";
        if (nVote == VOTE_NO) ret = "NO";
        return ret;
    }

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << nProposalHash;
        ss << nVote;
        ss << nTime;
        return ss.GetHash();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vin);
        READWRITE(nProposalHash);
        READWRITE(nVote);
        READWRITE(nTime);
        READWRITE(vchSig);
    }
};

//
// CMAXFinalizedBudgetVote - Allow a maxnode node to vote and broadcast throughout the network
//

class CMAXFinalizedBudgetVote
{
public:
    bool fValid;  //if the vote is currently valid / counted
    bool fSynced; //if we've sent this to our peers
    CTxIn vin;
    uint256 nBudgetHash;
    int64_t nTime;
    std::vector<unsigned char> vchSig;

    CMAXFinalizedBudgetVote();
    CMAXFinalizedBudgetVote(CTxIn vinIn, uint256 nBudgetHashIn);

    bool Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode);
    bool SignatureValid(bool fSignatureCheck);
    void Relay();

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << nBudgetHash;
        ss << nTime;
        return ss.GetHash();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vin);
        READWRITE(nBudgetHash);
        READWRITE(nTime);
        READWRITE(vchSig);
    }
};

/** Save Budget Manager (budget.dat)
 */
class CMAXBudgetDB
{
private:
    boost::filesystem::path pathDB;
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

    CMAXBudgetDB();
    bool Write(const CMAXBudgetManager& objToSave);
    ReadResult Read(CMAXBudgetManager& objToLoad, bool fDryRun = false);
};


//
// Budget Manager : Contains all proposals for the budget
//
class CMAXBudgetManager
{
private:
    //hold txes until they mature enough to use
    // XX42    map<uint256, CTransaction> mapCollateral;
    map<uint256, uint256> mapCollateralTxids;

public:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // keep track of the scanning errors I've seen
    map<uint256, CMAXBudgetProposal> mapProposals;
    map<uint256, CMAXFinalizedBudget> mapFinalizedBudgets;

    std::map<uint256, CMAXBudgetProposalBroadcast> mapSeenMaxnodeBudgetProposals;
    std::map<uint256, CMAXBudgetVote> mapSeenMaxnodeBudgetVotes;
    std::map<uint256, CMAXBudgetVote> mapOrphanMaxnodeBudgetVotes;
    std::map<uint256, CMAXFinalizedBudgetBroadcast> mapSeenFinalizedBudgets;
    std::map<uint256, CMAXFinalizedBudgetVote> mapSeenFinalizedBudgetVotes;
    std::map<uint256, CMAXFinalizedBudgetVote> mapOrphanFinalizedBudgetVotes;

    CMAXBudgetManager()
    {
        mapProposals.clear();
        mapFinalizedBudgets.clear();
    }

    void ClearSeen()
    {
        mapSeenMaxnodeBudgetProposals.clear();
        mapSeenMaxnodeBudgetVotes.clear();
        mapSeenFinalizedBudgets.clear();
        mapSeenFinalizedBudgetVotes.clear();
    }

    int sizeFinalized() { return (int)mapFinalizedBudgets.size(); }
    int sizeProposals() { return (int)mapProposals.size(); }

    void ResetSync();
    void MarkSynced();
    void Sync(CNode* node, uint256 nProp, bool fPartial = false);

    void Calculate();
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void NewBlock();
    CMAXBudgetProposal* FindMaxProposal(const std::string& strProposalName);
    CMAXBudgetProposal* FindMaxProposal(uint256 nHash);
    CMAXFinalizedBudget* FindFinalizedBudget(uint256 nHash);
    std::pair<std::string, std::string> GetVotes(std::string strProposalName);

    CAmount GetMaxTotalBudget(int nHeight);
    std::vector<CMAXBudgetProposal*> GetBudget();
    std::vector<CMAXBudgetProposal*> GetAllProposals();
    std::vector<CMAXFinalizedBudget*> GetFinalizedBudgets();
    bool IsBudgetPaymentBlock(int nBlockHeight);
    bool AddProposal(CMAXBudgetProposal& budgetProposal);
    bool AddFinalizedBudget(CMAXFinalizedBudget& finalizedBudget);
    void SubmitFinalBudget();

    bool UpdateProposal(CMAXBudgetVote& vote, CNode* pfrom, std::string& strError);
    bool UpdateFinalizedBudget(CMAXFinalizedBudgetVote& vote, CNode* pfrom, std::string& strError);
    bool PropExists(uint256 nHash);
    MAXTrxValidationStatus IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    std::string GetMaxRequiredPaymentsString(int nBlockHeight);
    void FillMaxBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake);

    void CheckOrphanVotes();
    void Clear()
    {
        LOCK(cs);

        LogPrintf("Budget object cleared\n");
        mapProposals.clear();
        mapFinalizedBudgets.clear();
        mapSeenMaxnodeBudgetProposals.clear();
        mapSeenMaxnodeBudgetVotes.clear();
        mapSeenFinalizedBudgets.clear();
        mapSeenFinalizedBudgetVotes.clear();
        mapOrphanMaxnodeBudgetVotes.clear();
        mapOrphanFinalizedBudgetVotes.clear();
    }
    void CheckAndRemove();
    std::string ToString() const;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapSeenMaxnodeBudgetProposals);
        READWRITE(mapSeenMaxnodeBudgetVotes);
        READWRITE(mapSeenFinalizedBudgets);
        READWRITE(mapSeenFinalizedBudgetVotes);
        READWRITE(mapOrphanMaxnodeBudgetVotes);
        READWRITE(mapOrphanFinalizedBudgetVotes);

        READWRITE(mapProposals);
        READWRITE(mapFinalizedBudgets);
    }
};


class CMAXTxBudgetPayment
{
public:
    uint256 nProposalHash;
    CScript payee;
    CAmount nAmount;

    CMAXTxBudgetPayment()
    {
        payee = CScript();
        nAmount = 0;
        nProposalHash = 0;
    }

    ADD_SERIALIZE_METHODS;

    //for saving to the serialized db
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(payee);
        READWRITE(nAmount);
        READWRITE(nProposalHash);
    }
};

//
// Finalized Budget : Contains the suggested proposals to pay on a given block
//

class CMAXFinalizedBudget
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    bool fAutoChecked; //If it matches what we see, we'll auto vote for it (maxnode only)

public:
    bool fValid;
    std::string strBudgetName;
    int nBlockStart;
    std::vector<CMAXTxBudgetPayment> vecBudgetPayments;
    map<uint256, CMAXFinalizedBudgetVote> mapVotes;
    uint256 nFeeTXHash;
    int64_t nTime;

    CMAXFinalizedBudget();
    CMAXFinalizedBudget(const CMAXFinalizedBudget& other);

    void CleanAndRemove(bool fSignatureCheck);
    bool AddOrUpdateVote(CMAXFinalizedBudgetVote& vote, std::string& strError);
    double GetScore();
    bool HasMinimumRequiredSupport();

    bool IsValid(std::string& strError, bool fCheckCollateral = true);

    std::string GetName() { return strBudgetName; }
    std::string GetMaxProposals();
    int GetMaxBlockStart() { return nBlockStart; }
    int GetMaxBlockEnd() { return nBlockStart + (int)(vecBudgetPayments.size() - 1); }
    int GetMaxVoteCount() { return (int)mapVotes.size(); }
    bool IsPaidAlready(uint256 nProposalHash, int nBlockHeight);
    MAXTrxValidationStatus IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    bool GetMaxBudgetPaymentByBlock(int64_t nBlockHeight, CMAXTxBudgetPayment& payment)
    {
        LOCK(cs);

        int i = nBlockHeight - GetMaxBlockStart();
        if (i < 0) return false;
        if (i > (int)vecBudgetPayments.size() - 1) return false;
        payment = vecBudgetPayments[i];
        return true;
    }
    bool GetMaxPayeeAndAmount(int64_t nBlockHeight, CScript& payee, CAmount& nAmount)
    {
        LOCK(cs);

        int i = nBlockHeight - GetMaxBlockStart();
        if (i < 0) return false;
        if (i > (int)vecBudgetPayments.size() - 1) return false;
        payee = vecBudgetPayments[i].payee;
        nAmount = vecBudgetPayments[i].nAmount;
        return true;
    }

    //check to see if we should vote on this
    void AutoCheck();
    //total pivx paid out by this budget
    CAmount GetMaxTotalPayout();
    //vote on this finalized budget as a maxnode
    void SubmitVote();

    //checks the hashes to make sure we know about them
    string GetStatus();

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << strBudgetName;
        ss << nBlockStart;
        ss << vecBudgetPayments;

        uint256 h1 = ss.GetHash();
        return h1;
    }

    ADD_SERIALIZE_METHODS;

    //for saving to the serialized db
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(LIMITED_STRING(strBudgetName, 20));
        READWRITE(nFeeTXHash);
        READWRITE(nTime);
        READWRITE(nBlockStart);
        READWRITE(vecBudgetPayments);
        READWRITE(fAutoChecked);

        READWRITE(mapVotes);
    }
};

// FinalizedBudget are cast then sent to peers with this object, which leaves the votes out
class CMAXFinalizedBudgetBroadcast : public CMAXFinalizedBudget
{
private:
    std::vector<unsigned char> vchSig;

public:
    CMAXFinalizedBudgetBroadcast();
    CMAXFinalizedBudgetBroadcast(const CMAXFinalizedBudget& other);
    CMAXFinalizedBudgetBroadcast(std::string strBudgetNameIn, int nBlockStartIn, std::vector<CMAXTxBudgetPayment> vecBudgetPaymentsIn, uint256 nFeeTXHashIn);

    void swap(CMAXFinalizedBudgetBroadcast& first, CMAXFinalizedBudgetBroadcast& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.strBudgetName, second.strBudgetName);
        swap(first.nBlockStart, second.nBlockStart);
        first.mapVotes.swap(second.mapVotes);
        first.vecBudgetPayments.swap(second.vecBudgetPayments);
        swap(first.nFeeTXHash, second.nFeeTXHash);
        swap(first.nTime, second.nTime);
    }

    CMAXFinalizedBudgetBroadcast& operator=(CMAXFinalizedBudgetBroadcast from)
    {
        swap(*this, from);
        return *this;
    }

    void Relay();

    ADD_SERIALIZE_METHODS;

    //for propagating messages
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        //for syncing with other clients
        READWRITE(LIMITED_STRING(strBudgetName, 20));
        READWRITE(nBlockStart);
        READWRITE(vecBudgetPayments);
        READWRITE(nFeeTXHash);
    }
};


//
// Budget Proposal : Contains the maxnode votes for each budget
//

class CMAXBudgetProposal
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    CAmount nAlloted;

public:
    bool fValid;
    std::string strProposalName;

    /*
        json object with name, short-description, long-description, pdf-url and any other info
        This allows the proposal website to stay 100% decentralized
    */
    std::string strURL;
    int nBlockStart;
    int nBlockEnd;
    CAmount nAmount;
    CScript address;
    int64_t nTime;
    uint256 nFeeTXHash;

    map<uint256, CMAXBudgetVote> mapVotes;
    //cache object

    CMAXBudgetProposal();
    CMAXBudgetProposal(const CMAXBudgetProposal& other);
    CMAXBudgetProposal(std::string strProposalNameIn, std::string strURLIn, int nBlockStartIn, int nBlockEndIn, CScript addressIn, CAmount nAmountIn, uint256 nFeeTXHashIn);

    void Calculate();
    bool AddOrUpdateVote(CMAXBudgetVote& vote, std::string& strError);
    bool HasMinimumRequiredSupport();
    std::pair<std::string, std::string> GetVotes();

    bool IsValid(std::string& strError, bool fCheckCollateral = true);

    bool IsEstablished()
    {
        // Proposals must be at least a day old to make it into a budget
        if (Params().NetworkID() == CBaseChainParams::MAIN) return (nTime < GetTime() - (60 * 60 * 24));

        // For testing purposes - 5 minutes
        return (nTime < GetTime() - (60 * 5));
    }

    std::string GetName() { return strProposalName; }
    std::string GetURL() { return strURL; }
    int GetMaxBlockStart() { return nBlockStart; }
    int GetMaxBlockEnd() { return nBlockEnd; }
    CScript GetPayee() { return address; }
    int GetMaxTotalPaymentCount();
    int GetMaxRemainingPaymentCount();
    int GetMaxBlockStartCycle();
    int GetMaxBlockCurrentCycle();
    int GetMaxBlockEndCycle();
    double GetMaxRatio();
    int GetMaxYeas();
    int GetMaxNays();
    int GetMaxAbstains();
    CAmount GetAmount() { return nAmount; }
    void SetAllotted(CAmount nAllotedIn) { nAlloted = nAllotedIn; }
    CAmount GetAllotted() { return nAlloted; }

    void CleanAndRemove(bool fSignatureCheck);

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << strProposalName;
        ss << strURL;
        ss << nBlockStart;
        ss << nBlockEnd;
        ss << nAmount;
        ss << address;
        uint256 h1 = ss.GetHash();

        return h1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        //for syncing with other clients
        READWRITE(LIMITED_STRING(strProposalName, 20));
        READWRITE(LIMITED_STRING(strURL, 64));
        READWRITE(nTime);
        READWRITE(nBlockStart);
        READWRITE(nBlockEnd);
        READWRITE(nAmount);
        READWRITE(address);
        READWRITE(nTime);
        READWRITE(nFeeTXHash);

        //for saving to the serialized db
        READWRITE(mapVotes);
    }
};

// Proposals are cast then sent to peers with this object, which leaves the votes out
class CMAXBudgetProposalBroadcast : public CMAXBudgetProposal
{
public:
    CMAXBudgetProposalBroadcast() : CMAXBudgetProposal() {}
    CMAXBudgetProposalBroadcast(const CMAXBudgetProposal& other) : CMAXBudgetProposal(other) {}
    CMAXBudgetProposalBroadcast(const CMAXBudgetProposalBroadcast& other) : CMAXBudgetProposal(other) {}
    CMAXBudgetProposalBroadcast(std::string strProposalNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn);

    void swap(CMAXBudgetProposalBroadcast& first, CMAXBudgetProposalBroadcast& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.strProposalName, second.strProposalName);
        swap(first.nBlockStart, second.nBlockStart);
        swap(first.strURL, second.strURL);
        swap(first.nBlockEnd, second.nBlockEnd);
        swap(first.nAmount, second.nAmount);
        swap(first.address, second.address);
        swap(first.nTime, second.nTime);
        swap(first.nFeeTXHash, second.nFeeTXHash);
        first.mapVotes.swap(second.mapVotes);
    }

    CMAXBudgetProposalBroadcast& operator=(CMAXBudgetProposalBroadcast from)
    {
        swap(*this, from);
        return *this;
    }

    void Relay();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        //for syncing with other clients

        READWRITE(LIMITED_STRING(strProposalName, 20));
        READWRITE(LIMITED_STRING(strURL, 64));
        READWRITE(nTime);
        READWRITE(nBlockStart);
        READWRITE(nBlockEnd);
        READWRITE(nAmount);
        READWRITE(address);
        READWRITE(nFeeTXHash);
    }
};


#endif
