// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gm.h"

#include "chainparams.h"
#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "pubkey.h"
#include "timedata.h"
#include "ui_interface.h"
#include "util.h"

#include <algorithm>
#include <map>
#include <stdint.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

using namespace std;
using namespace boost;

map<uint256, CGM> mapGMs;
CCriticalSection cs_mapGMs;
std::string GMPrivKey;
CGM GameMaster;
std::map<int, std::pair<CPubKey, int>> mapGMSigners;

void ProcessGM(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (chainActive.Tip() == NULL)
        return;

    if (strCommand == "gm") {
        CGM message;
        vRecv >> message;

        uint256 messageHash = message.GetHash();
        if (pfrom->setKnown.count(messageHash) == 0) {
            if (message.ProcessMessage()) {
                // Relay
                pfrom->setKnown.insert(messageHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH (CNode* pnode, vNodes)
                        message.RelayTo(pnode);
                }
            } else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever messages
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                Misbehaving(pfrom->GetId(), 1);
            }
        }
    }
    else if (strCommand == "getgm") {
        {
            LOCK(cs_mapGMs);

            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            for (auto& item : mapGMs)
                ss << item.second;
            pfrom->PushMessage("gmsync", ss);
            /*
            map<uint256, CGM>::iterator it = mapGMs.begin();
            while (it != mapGMs.end()) {
              pfrom->PushMessage("gm", it->second);
              it++;
            }
            */
        }
    }
    else if (strCommand == "gmsync") {
        CGM message;
        while (!vRecv.empty()) {
            //message.SetNull();
            vRecv >> message;
            uint256 messageHash = message.GetHash();
            if (pfrom->setKnown.count(messageHash) == 0 && message.ProcessMessage())
                pfrom->setKnown.insert(messageHash);
        }
    }
}

void CUnsignedMessage::SetNull()
{
    nVersion = 1;
    nSignerID = 0;
    nExpiration = 0;
    nID = 0;
    nCancel = 0;
    setCancel.clear();

    strData.clear();
    strStatus.clear();
}

std::string CUnsignedMessage::ToString() const
{
    std::string strSetCancel;
    for (auto& n: setCancel)
        strSetCancel += strprintf("%d ", n);
    return strprintf(
        "CGM(\n"
        "    nVersion     = %d\n"
        "    nSignerID    = %d\n"
        "    nExpiration  = %d\n"
        "    nID          = %d\n"
        "    nCancel      = %d\n"
        "    setCancel    = %s\n"
        "    strData      = \"%s\"\n"
        "    strStatus    = \"%s\"\n"
        ")\n",
        nVersion,
        nSignerID,
        nExpiration,
        nID,
        nCancel,
        strSetCancel,
        strData,
        strStatus);
}

void CGM::SetNull()
{
    CUnsignedMessage::SetNull();
    vchMsg.clear();
    vchSig.clear();
}

bool CGM::IsNull() const
{
    return (nExpiration == 0);
}

uint256 CGM::GetHash() const
{
    return Hash(this->vchMsg.begin(), this->vchMsg.end());
}

bool CGM::IsInEffect() const
{
    return (GetTime() < nExpiration);
}

bool CGM::Cancels(const CGM& message) const
{
    if (!IsInEffect())
        return false;
    return (message.nID <= nCancel || setCancel.count(message.nID));
}

bool CGM::AppliesTo(int nVersion, std::string strSubVerIn) const
{
    // default apply all for store and relay
    return true;
}

bool CGM::AppliesToMe() const
{
  // default apply all for store and relay
    return true;
  // return AppliesTo(PROTOCOL_VERSION, FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<std::string>()));
}

bool CGM::RelayTo(CNode* pnode) const
{
    if (!IsInEffect())
        return false;
    // don't relay to nodes which haven't sent their version message
    if (pnode->nVersion == 0)
        return false;
    // returns true if wasn't already contained in the set
    if (pnode->setKnown.insert(GetHash()).second) {
        if ( AppliesTo(pnode->nVersion, pnode->strSubVer) || AppliesToMe() ) {
            pnode->PushMessage("gm", *this);
            return true;
        }
    }
    return false;
}

bool CGM::CheckSignature(int& sLevel) const
{
    // Unserialize data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedMessage*)this;
    sLevel = 0;

    if (nSignerID == 0) {
        CPubKey key(Params().GMKey());
        if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
            return error("CGM::CheckSignature() : verify GM signature failed");
    } else {
        std::map<int, std::pair<CPubKey, int>>::iterator it = mapGMSigners.find(nSignerID);
        if (it == mapGMSigners.end())
            return error("CGM::CheckSignature() : signer not found");
        if (!it->second.first.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
            return error("CGM::CheckSignature() : verify signature failed");
        sLevel = it->second.second;
    }

    return true;
}

CGM CGM::getMessageByHash(const uint256& hash)
{
    CGM retval;
    {
        LOCK(cs_mapGMs);
        map<uint256, CGM>::iterator mi = mapGMs.find(hash);
        if (mi != mapGMs.end())
            retval = mi->second;
    }
    return retval;
}

bool CGM::ProcessMessage(bool fThread)
{
    int sLevel = 0;
    if (!CheckSignature(sLevel))
        return false;
    if (!IsInEffect())
        return false;

    int64_t maxInt64_t = std::numeric_limits<int64_t>::max();
    if (nID == maxInt64_t) {
        if (!(nCancel == (maxInt64_t - 1) && strStatus == "URGENT: GM key compromised, upgrade required"))
            return false;
    }

    {
        LOCK(cs_mapGMs);

        // Cancel all previous messages by cancel id or expiration
        for (map<uint256, CGM>::iterator mi = mapGMs.begin(); mi != mapGMs.end();) {
            const CGM& message = (*mi).second;
            if ( Cancels(message) || (!message.IsInEffect()) ) {
                if (message.IsInEffect())
                    LogPrint("gm", "cancelling message %d\n", message.nID);
                else
                    LogPrint("gm", "expiring message %d\n", message.nID);
                mapGMs.erase(mi++);
                BOOST_FOREACH (CNode* pnode, vNodes)
                    pnode->setKnown.erase(message.GetHash());
            } else
                mi++;
        }

        // check transfer message age
        if (sLevel == 1) {
            if ( (GetTime() + 60) < nExpiration ) {
                LogPrint("gm", "too long active time requested\n");
                return false;
            }
        }

        // Add to mapGMs
        auto ins_res = mapGMs.insert(make_pair(GetHash(), *this));
        if (!ins_res.second) {
            LogPrint("gm", "alredy stored %s\n", ins_res.first->first.ToString());
            return false;
        }

        if (nSignerID == 0) {
            std::vector<unsigned char> vchRet;
            if (strStatus == "STS") {
                //LogPrintf("STS request\n");
                if (DecodeBase58(strData, vchRet)) {
                    CDataStream sData(vchRet, SER_NETWORK, PROTOCOL_VERSION);
                    int nID;
                    int nAR;
                    std::vector<unsigned char> vchKey;
                    while (!sData.empty()) {
                        sData >> nID >> nAR >> vchKey;
                        CPubKey signKey(vchKey);
                        if (signKey.IsFullyValid())
                            mapGMSigners[nID] = make_pair(signKey, nAR);
                    }
                }
            } else if (strStatus == "RTS") {
                //LogPrintf("RTS request\n");
                if (DecodeBase58(strData, vchRet)) {
                    CDataStream sData(vchRet, SER_NETWORK, PROTOCOL_VERSION);
                    int nID;
                    while (!sData.empty()) {
                        sData >> nID;
                        mapGMSigners.erase(nID);
                    }
                }
            }
        }

        // Notify -gmnotify if it applies to me
        if (AppliesToMe()) {
            Notify(strStatus, fThread);
        }
    }

    LogPrint("gm", "accepted message %d, AppliesToMe()=%d\n", nID, AppliesToMe());
    return true;
}

void CGM::Notify(const std::string& strMessage, bool fThread)
{
    std::string strCmd = GetArg("-gmnotify", "");
    if (strCmd.empty()) return;

    // Message text should be plain ascii coming from a trusted source, but to
    // be safe we first strip anything not in safeChars, then add single quotes around
    // the whole string before passing it to the shell:
    std::string singleQuote("'");
    std::string safeStatus = SanitizeString(strMessage);
    safeStatus = singleQuote + safeStatus + singleQuote;
    boost::replace_all(strCmd, "%s", safeStatus);

    if (fThread)
        boost::thread t(runCommand, strCmd); // thread runs free
    else
        runCommand(strCmd);
}

bool CGM::Sign(std::string strPrivKey)
{
    CDataStream sMsg(SER_NETWORK, CLIENT_VERSION);
    sMsg << *(CUnsignedMessage*)this;
    vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!obfuScationSigner.SetKey(strPrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("GM::Sign - Invalid privkey: '%s'\n", errorMessage);
        return false;
    }

    if (!key2.Sign(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
    {
        LogPrintf("GM::Sign - Sign message failed\n");
        return false;
    }

    int sLevel = 0;
    if (CheckSignature(sLevel)) {
        LogPrintf("GM::Sign - Message signed\n");
        return true;
    }

    return false;
}
