// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2018 e-Sport Betting Coin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//#ifndef BITCOIN_ALERT_H
//#define BITCOIN_ALERT_H
#ifndef ESPB_GM_H
#define ESPB_GM_H

#include "base58.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "util.h"
#include "serialize.h"
#include "sync.h"

#include "obfuscation.h"
#include "protocol.h"
#include "streams.h"

#include <map>
#include <set>
#include <stdint.h>
#include <string>

class CGM;
class CNode;
class uint256;

extern std::map<uint256, CGM> mapGMs;
extern CCriticalSection cs_mapGMs;
extern std::string GMPrivKey;
extern CGM GameMaster;
extern std::map<int, std::pair<CPubKey, int>> mapGMSigners;

void ProcessGM(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

class CUnsignedMessage
{
public:
    int nVersion;
    int nSignerID; // 0 - gm
    int nExpiration;
    int64_t nID;
    int64_t nCancel;
    std::set<int64_t> setCancel;

    std::string strData;
    std::string strStatus;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nSignerID);
        READWRITE(nExpiration);
        READWRITE(nID);
        READWRITE(nCancel);
        READWRITE(setCancel);

        READWRITE(LIMITED_STRING(strData, 65536));
        READWRITE(LIMITED_STRING(strStatus, 256));
    }

    void SetNull();

    std::string ToString() const;
};

/** An message is a combination of a serialized CUnsignedMessage and a signature. */
class CGM : public CUnsignedMessage
{
public:
    std::vector<unsigned char> vchMsg;
    std::vector<unsigned char> vchSig;

    CGM()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vchMsg);
        READWRITE(vchSig);
    }

    void SetNull();
    bool IsNull() const;
    uint256 GetHash() const;
    bool IsInEffect() const;
    bool Cancels(const CGM& message) const;
    bool AppliesTo(int nVersion, std::string strSubVerIn) const;
    bool AppliesToMe() const;
    bool RelayTo(CNode* pnode) const;
    bool CheckSignature(int& sLevel) const;
    bool ProcessMessage(bool fThread = true); // fThread means run -messagenotify in a free-running thread
    static void Notify(const std::string& strMessage, bool fThread);
    bool Sign(std::string strPrivKey);
    /*
     * Get copy of (active) message object by hash. Returns a null message if it is not found.
     */
    static CGM getMessageByHash(const uint256& hash);
};

#endif // ESPB_GM_H // BITCOIN_ALERT_H
