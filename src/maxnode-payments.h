// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2019 The Lytix developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MAXNODE_PAYMENTS_H
#define MAXNODE_PAYMENTS_H

#include "key.h"
#include "main.h"
#include "maxnode.h"
#include <boost/lexical_cast.hpp>

using namespace std;

extern CCriticalSection cs_MaxvecPayments;
extern CCriticalSection cs_mapMaxnodeBlocks;
extern CCriticalSection cs_mapMaxnodePayeeVotes;

class CMaxnodePayments;
class CMaxnodePaymentWinner;
class CMaxnodeBlockPayees;

extern CMaxnodePayments maxnodePayments;

#define MAXPAYMENTS_SIGNATURES_REQUIRED 6
#define MAXPAYMENTS_SIGNATURES_TOTAL 10

void ProcessMessageMaxnodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
bool IsMaxBlockPayeeValid(const CBlock& block, int nBlockHeight);
std::string GetMaxRequiredPaymentsString(int nBlockHeight);
bool IsMaxBlockValueValid(const CBlock& block, CAmount nExpectedValue, CAmount nMinted);
void FillMaxBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake, bool fZPIVStake);

void DumpMaxnodePayments();

/** Save Maxnode Payment Data (maxpayments.dat)
 */
class CMaxnodePaymentDB
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

    CMaxnodePaymentDB();
    bool Write(const CMaxnodePayments& objToSave);
    ReadResult Read(CMaxnodePayments& objToLoad, bool fDryRun = false);
};

class CMaxnodePayee
{
public:
    CScript scriptPubKey;
    int nVotes;

    CMaxnodePayee()
    {
        scriptPubKey = CScript();
        nVotes = 0;
    }

    CMaxnodePayee(CScript payee, int nVotesIn)
    {
        scriptPubKey = payee;
        nVotes = nVotesIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(scriptPubKey);
        READWRITE(nVotes);
    }
};

// Keep track of votes for payees from maxnodes
class CMaxnodeBlockPayees
{
public:
    int nBlockHeight;
    std::vector<CMaxnodePayee> vecPayments;

    CMaxnodeBlockPayees()
    {
        nBlockHeight = 0;
        vecPayments.clear();
    }
    CMaxnodeBlockPayees(int nBlockHeightIn)
    {
        nBlockHeight = nBlockHeightIn;
        vecPayments.clear();
    }

    void AddPayee(CScript payeeIn, int nIncrement)
    {
        LOCK(cs_MaxvecPayments);

        BOOST_FOREACH (CMaxnodePayee& payee, vecPayments) {
            if (payee.scriptPubKey == payeeIn) {
                payee.nVotes += nIncrement;
                return;
            }
        }

        CMaxnodePayee c(payeeIn, nIncrement);
        vecPayments.push_back(c);
    }

    bool GetPayee(CScript& payee)
    {
        LOCK(cs_MaxvecPayments);

        int nVotes = -1;
        BOOST_FOREACH (CMaxnodePayee& p, vecPayments) {
            if (p.nVotes > nVotes) {
                payee = p.scriptPubKey;
                nVotes = p.nVotes;
            }
        }

        return (nVotes > -1);
    }

    bool HasPayeeWithVotes(CScript payee, int nVotesReq)
    {
        LOCK(cs_MaxvecPayments);

        BOOST_FOREACH (CMaxnodePayee& p, vecPayments) {
            if (p.nVotes >= nVotesReq && p.scriptPubKey == payee) return true;
        }

        return false;
    }

    bool IsTransactionValid(const CTransaction& txNew);
    std::string GetMaxRequiredPaymentsString();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nBlockHeight);
        READWRITE(vecPayments);
    }
};

// for storing the winning payments
class CMaxnodePaymentWinner
{
public:
    CTxIn vinMaxnode;

    int nBlockHeight;
    CScript payee;
    std::vector<unsigned char> vchSig;

    CMaxnodePaymentWinner()
    {
        nBlockHeight = 0;
        vinMaxnode = CTxIn();
        payee = CScript();
    }

    CMaxnodePaymentWinner(CTxIn vinIn)
    {
        nBlockHeight = 0;
        vinMaxnode = vinIn;
        payee = CScript();
    }

    uint256 GetHash()
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << payee;
        ss << nBlockHeight;
        ss << vinMaxnode.prevout;

        return ss.GetHash();
    }

    bool Sign(CKey& keyMaxnode, CPubKey& pubKeyMaxnode);
    bool IsValid(CNode* pnode, std::string& strError);
    bool SignatureValid();
    void Relay();

    void AddPayee(CScript payeeIn)
    {
        payee = payeeIn;
    }


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vinMaxnode);
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vchSig);
    }

    std::string ToString()
    {
        std::string maxret = "";
        maxret += vinMaxnode.ToString();
        maxret += ", " + boost::lexical_cast<std::string>(nBlockHeight);
        maxret += ", " + payee.ToString();
        maxret += ", " + boost::lexical_cast<std::string>((int)vchSig.size());
        return maxret;
    }
};

//
// Maxnode Payments Class
// Keeps track of who should get paid for which blocks
//

class CMaxnodePayments
{
private:
    int nSyncedFromPeer;
    int nLastBlockHeight;

public:
    std::map<uint256, CMaxnodePaymentWinner> mapMaxnodePayeeVotes;
    std::map<int, CMaxnodeBlockPayees> mapMaxnodeBlocks;
    std::map<uint256, int> mapMaxnodesLastVote; //prevout.hash + prevout.n, nBlockHeight

    CMaxnodePayments()
    {
        nSyncedFromPeer = 0;
        nLastBlockHeight = 0;
    }

    void Clear()
    {
        LOCK2(cs_mapMaxnodeBlocks, cs_mapMaxnodePayeeVotes);
        mapMaxnodeBlocks.clear();
        mapMaxnodePayeeVotes.clear();
    }

    bool AddWinningMaxnode(CMaxnodePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight);

    void Sync(CNode* node, int nCountNeeded);
    void CleanPaymentList();
    int LastPayment(CMaxnode& max);

    bool GetBlockPayee(int nBlockHeight, CScript& payee);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    bool IsScheduled(CMaxnode& max, int nNotBlockHeight);

    bool CanVote(COutPoint outMaxnode, int nBlockHeight)
    {
        LOCK(cs_mapMaxnodePayeeVotes);

        if (mapMaxnodesLastVote.count(outMaxnode.hash + outMaxnode.n)) {
            if (mapMaxnodesLastVote[outMaxnode.hash + outMaxnode.n] == nBlockHeight) {
                return false;
            }
        }

        //record this maxnode voted
        mapMaxnodesLastVote[outMaxnode.hash + outMaxnode.n] = nBlockHeight;
        return true;
    }

    int GetMinMaxnodePaymentsProto();
    void ProcessMessageMaxnodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    std::string GetMaxRequiredPaymentsString(int nBlockHeight);
    void FillMaxBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake, bool fZPIVStake);
    std::string ToString() const;
    int GetOldestBlock();
    int GetNewestBlock();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapMaxnodePayeeVotes);
        READWRITE(mapMaxnodeBlocks);
    }
};


#endif
