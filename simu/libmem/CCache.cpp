// copyright and includes {{{1
// Contributed by Luis Ceze
//                Jose Renau
//                Karin Strauss
//                Milos Prvulovic
//
// The ESESC/BSD License
//
// Copyright (c) 2005-2013, Regents of the University of California and
// the ESESC Project.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   - Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
//   - Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
//   - Neither the name of the University of California, Santa Cruz nor the
//   names of its contributors may be used to endorse or promote products
//   derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <iostream>

#include "nanassert.h"

#include "SescConf.h"
#include "MemorySystem.h"
#include "CCache.h"
#include "MSHR.h"
#include "GProcessor.h"
#include "TaskHandler.h"
#include "MemRequest.h"
#include "PortManager.h"
#include "../libsampler/BootLoader.h"
/* }}} */

#ifdef DEBUG
void CCache::trackAddress(MemRequest *mreq) {
  //I((mreq->getAddr() & 0xFFFF0) != 0x74f00);
  //I(mreq->getID() != 17);
  //I((mreq->getAddr() & 0xFFFF0) != 0x15e10);
}
#endif

//#define MTRACE(a...)   do{ fprintf(stderr,"@%lld %s %d 0x%x:",(long long int)globalClock,getName(), (int)mreq->getID(), (unsigned int)mreq->getAddr()); fprintf(stderr,##a); fprintf(stderr,"\n"); }while(0)
#define MTRACE(a...)

CCache::CCache(MemorySystem *gms, const char *section, const char *name)
  // Constructor {{{1
  :MemObj(section, name)
	 ,displacedSend   ("%s:displacedSend",       name)
	 ,displacedRecv   ("%s:displacedRecv",       name)
	 ,invAll          ("%s:invAll",          name)
	 ,invOne          ("%s:invOne",          name)
	 ,invNone         ("%s:invNone",         name)
	 ,writeBack       ("%s:writeBack",       name)
	 ,lineFill        ("%s:lineFill",        name)
	 ,avgMissLat      ("%s_avgMissLat",      name)
	 ,avgMemLat       ("%s_avgMemLat",       name)
	 ,avgSnoopLat     ("%s_avgSnoopLat",     name)
	 ,capInvalidateHit   ("%s_capInvalidateHit",   name)
	 ,capInvalidateMiss  ("%s_capInvalidateMiss",  name)
	 ,invalidateHit   ("%s_invalidateHit",   name)
	 ,invalidateMiss  ("%s_invalidateMiss",  name)
	 ,writeExclusive  ("%s:writeExclusive",  name)
{
  s_reqHit[ma_setInvalid]   = new GStatsCntr("%s:setInvalidHit",name);
  s_reqHit[ma_setValid]     = new GStatsCntr("%s:readHit",name);
  s_reqHit[ma_setDirty]     = new GStatsCntr("%s:writeHit",name);
  s_reqHit[ma_setShared]    = new GStatsCntr("%s:setSharedHit",name);
  s_reqHit[ma_setExclusive] = new GStatsCntr("%s:setExclusiveHit",name);
  s_reqHit[ma_MMU]          = new GStatsCntr("%s:MMUHit",name);
  s_reqHit[ma_VPCWU]        = new GStatsCntr("%s:VPCMUHit",name);

  s_reqMissLine[ma_setInvalid]   = new GStatsCntr("%s:setInvalidMiss",name);
  s_reqMissLine[ma_setValid]     = new GStatsCntr("%s:readMiss",name);
  s_reqMissLine[ma_setDirty]     = new GStatsCntr("%s:writeMiss",name);
  s_reqMissLine[ma_setShared]    = new GStatsCntr("%s:setSharedMiss",name);
  s_reqMissLine[ma_setExclusive] = new GStatsCntr("%s:setExclusiveMiss",name);
  s_reqMissLine[ma_MMU]          = new GStatsCntr("%s:MMUMiss",name);
  s_reqMissLine[ma_VPCWU]        = new GStatsCntr("%s:VPCMUMiss",name);

  s_reqMissState[ma_setInvalid]   = new GStatsCntr("%s:setInvalidMissState",name);
  s_reqMissState[ma_setValid]     = new GStatsCntr("%s:readMissState",name);
  s_reqMissState[ma_setDirty]     = new GStatsCntr("%s:writeMissState",name);
  s_reqMissState[ma_setShared]    = new GStatsCntr("%s:setSharedMissState",name);
  s_reqMissState[ma_setExclusive] = new GStatsCntr("%s:setExclusiveMissState",name);
  s_reqMissState[ma_MMU]          = new GStatsCntr("%s:MMUMissState",name);
  s_reqMissState[ma_VPCWU]        = new GStatsCntr("%s:VPCMUMissState",name);

  s_reqHalfMiss[ma_setInvalid]   = new GStatsCntr("%s:setInvalidHalfMiss",name);
  s_reqHalfMiss[ma_setValid]     = new GStatsCntr("%s:readHalfMiss",name);
  s_reqHalfMiss[ma_setDirty]     = new GStatsCntr("%s:writeHalfMiss",name);
  s_reqHalfMiss[ma_setShared]    = new GStatsCntr("%s:setSharedHalfMiss",name);
  s_reqHalfMiss[ma_setExclusive] = new GStatsCntr("%s:setExclusiveHalfMiss",name);
  s_reqHalfMiss[ma_MMU]          = new GStatsCntr("%s:MMUHalfMiss",name);
  s_reqHalfMiss[ma_VPCWU]        = new GStatsCntr("%s:VPCMUHalfMiss",name);

  s_reqAck[ma_setInvalid]   = new GStatsCntr("%s:setInvalidAck",name);
  s_reqAck[ma_setValid]     = new GStatsCntr("%s:setValidAck",name);
  s_reqAck[ma_setDirty]     = new GStatsCntr("%s:setDirtyAck",name);
  s_reqAck[ma_setShared]    = new GStatsCntr("%s:setSharedAck",name);
  s_reqAck[ma_setExclusive] = new GStatsCntr("%s:setExclusiveAck",name);
  s_reqAck[ma_MMU]          = new GStatsCntr("%s:MMUAck",name);
  s_reqAck[ma_VPCWU]        = new GStatsCntr("%s:VPCMUAck",name);

  s_reqSetState[ma_setInvalid]   = new GStatsCntr("%s:setInvalidSetState",name);
  s_reqSetState[ma_setValid]     = new GStatsCntr("%s:setValidSetState",name);
  s_reqSetState[ma_setDirty]     = new GStatsCntr("%s:setDirtySetState",name);
  s_reqSetState[ma_setShared]    = new GStatsCntr("%s:setSharedSetState",name);
  s_reqSetState[ma_setExclusive] = new GStatsCntr("%s:setExclusiveSetState",name);
  s_reqSetState[ma_MMU]          = new GStatsCntr("%s:MMUSetState",name);
  s_reqSetState[ma_VPCWU]        = new GStatsCntr("%s:VPCMUSetState",name);

  // TODO: add support for coreCoupledFreq as part of mreq
  //if(SescConf->checkBool(section,"coreCoupledFreq")) {
    //MSG("WARNING: coreCoupledFreq does not work yet");
  //}

#if 1
  if(SescConf->checkBool(section,"incoherent")) {
    incoherent = SescConf->checkBool(section,"incoherent");
    //MSG("WARNING: coreCoupledFreq does not work yet");
  } else {
    incoherent = false;
  }
#else

  if(SescConf->checkBool(section,"incoherent")) {
    if(SescConf->getBool(section,"incoherent")) {
      clearNeedsCoherence();
    }
  }

#endif


  coreCoupledFreq = false;
  //if (coreCoupledFreq) {
  //  BootLoader::getPowerModelPtr()->addTurboCoupledMemory(this);
  //}

  char tmpName[512];

  sprintf(tmpName, "%s", name);

  cacheBank = CacheType::create(section, "", tmpName);
  lineSize  = cacheBank->getLineSize();
  lineSizeBits = log2i(lineSize);
  mshr      = new MSHR(tmpName, 512, lineSize, 512);

  I(getLineSize() < 4096); // To avoid bank selection conflict (insane CCache line)
  I(gms);

  SescConf->checkBool(section, "inclusive");
  SescConf->checkBool(section, "directory");

  inclusive = SescConf->getBool(section, "inclusive");
  directory = SescConf->getBool(section, "directory");
	if (directory && !inclusive) {
	I(0);
		MSG("ERROR: %s CCache can not have a 'directory' without being 'inclusive'",section);
		SescConf->notCorrect();
	}

  MemObj *lower_level = gms->declareMemoryObj(section, "lowerLevel");
  if(lower_level)
    addLowerLevel(lower_level);

  port = PortManager::create(section, this);

	lastUpMsg = 0;
}
// }}}

CCache::~CCache()
/* destructor {{{1 */
{
  cacheBank->destroy();
}
// }}}

void CCache::displaceLine(AddrType naddr, MemRequest *mreq, Line *l)
/* displace a line from the CCache. writeback if necessary {{{1 */
{
  I(naddr != mreq->getAddr()); // naddr is the displace address, mreq is the trigger
	I(l->isValid());

  bool doStats = mreq->getStatsFlag();

  if (inclusive) {
		if (directory) {
			if (l->getSharingCount() == 0) {
				invNone.inc(doStats);

				// DONE! Nice directory tracking detected no higher level sharing
			}else if (l->getSharingCount() == 1) {
				invOne.inc(doStats);

				MemRequest *inv_req = MemRequest::createSetState(this, this, ma_setInvalid, naddr, mreq->getPC(), doStats);
        trackAddress(inv_req);
				int32_t i = router->sendSetStateOthersPos(l->getFirstSharingPos(), inv_req, ma_setInvalid, inOrderUpMessage(1));
        if (i==0)
          inv_req->ack();
			}else{
        // FIXME: optimize directory for 2 or more
				invAll.inc(doStats);

				MemRequest *inv_req = MemRequest::createSetState(this, this, ma_setInvalid, naddr, mreq->getPC(), doStats);
        trackAddress(inv_req);
				int32_t i = router->sendSetStateAll(inv_req, ma_setInvalid, inOrderUpMessage(1));
        if (i==0)
          inv_req->ack();
			}
		}else{
			invAll.inc(doStats);

			MemRequest *inv_req = MemRequest::createSetState(this, this, ma_setInvalid, naddr, mreq->getPC(), doStats);
      int32_t i = router->sendSetStateAll(inv_req, ma_setInvalid, inOrderUpMessage(1));

      if (i==0)
        inv_req->ack();
		}
  }

  displacedSend.inc(doStats);

  if (l->needsDisp()) {
    router->sendDirtyDisp(naddr, mreq->getStatsFlag(),1);
		writeBack.inc();
  }else{
    router->sendCleanDisp(naddr, mreq->getStatsFlag(),1);
  }

  l->clearSharing();
}
// }}}

CCache::Line *CCache::allocateLine(AddrType addr, MemRequest *mreq)
/* find a new CCacheline for addr (it can return 0) {{{1 */
{
  AddrType rpl_addr=0;

  I(cacheBank->findLineDebug(addr) == 0);
  Line *l = cacheBank->fillLine(addr, rpl_addr);
  lineFill.inc(mreq->getStatsFlag());

  I(l); // Ignore lock guarantees to find line

  if(l->isValid()) {
		// TODO: add a port for evictions. Schedule the displaceLine accordingly
    displaceLine(rpl_addr, mreq, l);
  }
  I(l->getSharingCount()==0);

  return l;
}
// }}}

void CCache::mustForwardReqDown(MemRequest *mreq, bool miss)
  // pass a req down the hierarchy Check with MSR {{{1
{
  s_reqMissLine[mreq->getAction()]->inc(miss && mreq->getStatsFlag());
  s_reqMissState[mreq->getAction()]->inc(!miss && mreq->getStatsFlag());

  if (mreq->getAction() == ma_setDirty) {
    mreq->adjustReqAction(ma_setExclusive);
  }

  I(!mreq->isRetrying());

  router->scheduleReq(mreq, 1);
}
// }}}

bool CCache::CState::shouldNotifyLowerLevels(MsgAction ma, bool incoherent) const
// {{{1
{

  if (incoherent){
    if (state == I) {
      return true;
    }
    return false;
  }

  switch(ma) {
    case ma_setValid:     return state == I;
    case ma_setInvalid:   return true;
    case ma_MMU: // ARM does not cache MMU translations???
    case ma_setDirty:     return state != M && state != E;
    case ma_setShared:    return state != S && state != E;
    case ma_setExclusive: return state != M && state != E;
    default:          I(0);
  }
  I(0);
  return false;
}
// }}}

bool CCache::CState::shouldNotifyHigherLevels(MemRequest *mreq, int16_t portid) const
// {{{1
{
#if 0
  if uncoherent, return false
#endif
  if (nSharers == 0)
    return false;

  if (nSharers == 1 && share[0] == portid)
    return false; // Nobody but requester

#if 0
  if (incoherent){
    return false;
  }
#endif

  switch(mreq->getAction()) {
    case ma_setValid:
    case ma_setShared:    return shareState != S;
    case ma_setDirty:
    case ma_setExclusive:
    case ma_setInvalid:   return true;
    default:          I(0);
  }
  I(0);
  return true;
}
// }}}

CCache::CState::StateType CCache::CState::calcAdjustState(MemRequest *mreq) const
// {{{1
{
  StateType nstate = state;
  switch(mreq->getAction()) {
    case ma_setValid:
      if (state == I)
        nstate = E; // Simpler not E state
      // else keep the same
      break;
    case ma_setInvalid:
      nstate = I;
      break;
    case ma_setDirty:
			nstate = M;
      break;
    case ma_setShared:
			nstate = S;
      break;
    case ma_setExclusive:
      //I(state == I || state == E);
      nstate = E;
      break;
    default: I(0);
  }

  return nstate;
}
// }}}

void CCache::CState::adjustState(MemRequest *mreq, int16_t portid)
  // {{{1
{
  StateType ostate = state;
  state = calcAdjustState(mreq);

  //I(ostate != state); // only if we have full MSHR
  GI(mreq->isReq()        , mreq->getAction() == ma_setExclusive || mreq->getAction() == ma_setDirty   || mreq->getAction() == ma_setValid);
  GI(mreq->isReqAck()     , mreq->getAction() == ma_setExclusive || mreq->getAction() == ma_setDirty   || mreq->getAction() == ma_setShared);
  GI(mreq->isDisp()       , mreq->getAction() == ma_setDirty     || mreq->getAction() == ma_setValid   );
  GI(mreq->isSetStateAck(), mreq->getAction() == ma_setShared    || mreq->getAction() == ma_setInvalid );
  GI(mreq->isSetState()   , mreq->getAction() == ma_setShared    || mreq->getAction() == ma_setInvalid );

  if (mreq->isDisp()) {
    I(state != I);
    if (mreq->getAction() == ma_setDirty) {
      state = M;
    }else{
      I(mreq->getAction() == ma_setValid);
    }
    removeSharing(portid);
  }else if (mreq->isSetStateAck()) {
    if (mreq->getAction() == ma_setInvalid) {
      removeSharing(portid);
    }else{
      I(mreq->getAction() == ma_setShared);
      if (shareState != I)
        shareState = S;
    }
    if (mreq->isHomeNode()) {
      state = ostate;
			if (mreq->isSetStateAckDisp()) {
				// Only if it is the one that triggered the setState
				state = M;
			}
		}
  }else if (mreq->isSetState()) {
//    if (nSharers)
//      shareState = state;
    //I(portid<0);
  }else{
    I(state != I);
    I(mreq->isReq() || mreq->isReqAck());
    if (ostate != I && portid>=0) {
      //I(!mreq->isHomeNode());
      state = ostate;
    }
    if (mreq->isReqAck() && !mreq->isHomeNode()) {
      switch(mreq->getAction()) {
        case ma_setDirty:
          if (ostate == I)
            state = E;
          else
            state = ostate;
          break;
        case ma_setShared:
          state = S;
          break;
        case ma_setExclusive:
          state = E;
          break;
        default: I(0);
      }
    }
    addSharing(portid);
    if (nSharers == 0)
      shareState = I;
    else if (nSharers >1)
      shareState = S;
    else{
      if (state == S)
        shareState = S;
      else{
        I(state!=I);
        shareState = E;
      }
    }
  }

  GI(nSharers>1,shareState!=E && shareState!=M);
  GI(shareState==E || shareState==M,nSharers==1);
  GI(nSharers==0,shareState==I);
  GI(shareState==I, nSharers==0);

  if (state == I) {
    invalidate();
    return;
  }

#ifdef DEBUG_RABBIT
  const char *name = mreq->getCurrMem()->getName();
  if (strncasecmp(name,"L2",2)==0) {
    I(state != S);
  }
  if (strncasecmp(name,"L3",2)==0) {
    I(state != S);
  }
#endif

}
// }}}

bool CCache::notifyLowerLevels(Line *l, MemRequest *mreq)
  // {{{1
{
  if (!needsCoherence)
    return false;

  if (mreq->isReqAck())
    return false;

  I(mreq->isReq());

  if (l->shouldNotifyLowerLevels(mreq->getAction(),incoherent))
    return true;

  return false;
}
// }}}

bool CCache::notifyHigherLevels(Line *l, MemRequest *mreq)
// {{{1
{
  // Must do the notifyLowerLevels first
  I(l);
  I(!notifyLowerLevels(l,mreq));

  if (mreq->isHomeNode())
    return false;
  I(!router->isTopLevel());

  int16_t portid = router->getCreatorPort(mreq);
  if (l->shouldNotifyHigherLevels(mreq, portid)) {
    MsgAction ma = l->othersNeed(mreq->getAction());

    trackAddress(mreq);
    // TODO: check that it is the correct DL1 IL1 request source

    if (!directory || l->isBroadcastNeeded()) {
      router->sendSetStateOthers(mreq,ma,inOrderUpMessage(1));
      //I(num); // Otherwise, the need coherent would be set
    }else{
      for(int16_t i=0;i<l->getSharingCount();i++) {
        int16_t pos = l->getSharingPos(i);
        if (pos != portid) {
#ifdef DEBUG
          int32_t j =
#endif
            router->sendSetStateOthersPos(l->getSharingPos(i),mreq,ma,inOrderUpMessage(1));
          I(j);
        }
      }
    }

    // If mreq has pending stateack, it should not complete the read now
    return(mreq->hasPendingSetStateAck());
  }

  return false;
}
// }}}

void CCache::CState::addSharing(int16_t id)
{/*{{{*/
  if (nSharers>=8) {
    I(shareState == S);
    return;
  }
  if (id<0) {
    if (nSharers == 0) {
      shareState = I;
    }
    return;
  }

  I(id>=0); // portid<0 means no portid found
  if (nSharers==0) {
    share[0] = id;
    nSharers++;
    GI(nSharers>1,shareState == S);
    return;
  }

  for(int i=0;i<nSharers;i++) {
    if(share[i] == id) {
      return;
    }
  }

  share[nSharers] = id;
  nSharers++;
}/*}}}*/

void CCache::CState::removeSharing(int16_t id)
// {{{1
{
  if (nSharers>=8)
    return; // not possible to remove if in broadcast mode

  for(int16_t i=0;i<nSharers;i++) {
    if (share[i] == id) {
      for(int16_t j = i;j<(nSharers-1);j++) {
        share[j] = share[j+1];
      }
      nSharers--;
      if (nSharers == 0)
        shareState = I;
      return;
    }
  }
}
// }}}

void CCache::doReq(MemRequest *mreq)
/* processor/upper level issued read/write/busread {{{1 */
{
  MTRACE("doReq start");

  trackAddress(mreq);

  AddrType addr = mreq->getAddr();
  bool retrying = mreq->isRetrying();

  //MSG("%s 0x%x %s",getName(),  mreq->getAddr(), retrying?"R":"");

  if (retrying) { // reissued operation
		mreq->clearRetrying();
    I(!mshr->canIssue(addr)); // the req is already queued if retrying
  }else{
    if(!mshr->canIssue(addr)) {
      s_reqHalfMiss[mreq->getAction()]->inc(mreq->getStatsFlag());

      mreq->setRetrying();
      mshr->addEntry(addr, &mreq->startReqCB,mreq);
      return;
    }
    mshr->blockEntry(addr,mreq);
  }

  Line *l = cacheBank->readLine(addr);
  if (l == 0) {
    MTRACE("doReq cache miss");
		mustForwardReqDown(mreq, true);
    return;
  }

  if (notifyLowerLevels(l,mreq)) {
    MTRACE("doReq change state down");
    mustForwardReqDown(mreq, false);
    return; // Done (no retrying), and wait for the ReqAck
  }

  I(!mreq->hasPendingSetStateAck());
  bool waitupper = notifyHigherLevels(l,mreq);
  if (waitupper) {
    MTRACE("doReq change state up others");
    I(mreq->hasPendingSetStateAck());
    mreq->setRetrying();
    return;
  }

	I(l);
	I(l->isValid());

  int16_t portid = router->getCreatorPort(mreq);
  GI(portid<0,mreq->isHomeNode());
	l->adjustState(mreq, portid);

  Time_t when = port->reqDone(mreq);
  if (when == 0) {
    //I(0);
    MTRACE("doReq restartReq");
    // Must restart request
    mreq->setRetrying();
    mreq->restartReq();
    return;
  }

  if (retrying)
    s_reqHalfMiss[mreq->getAction()]->inc(mreq->getStatsFlag());
  else {
    s_reqHit[mreq->getAction()]->inc(mreq->getStatsFlag());
  }

  avgMemLat.sample(mreq->getTimeDelay()+(when-globalClock), mreq->getStatsFlag());
	I(when>=globalClock);
  if(mreq->isHomeNode()) {
		mreq->ackAbs(when);
	}else{
#ifdef DEBUG_RABBIT
    const char *name = mreq->getCurrMem()->getName();
    if (strncasecmp(name,"L3",2)==0) {
      I(l->reqAckNeeds() != ma_setShared);
    }
#endif
		mreq->convert2ReqAck(l->reqAckNeeds());
    if (!mreq->isWarmup())
      when = inOrderUpMessageAbs(when);
		router->scheduleReqAckAbs(mreq,when);
	}

  MTRACE("doReq done");

  port->reqRetire(mreq);
  mshr->retire(addr,mreq);
}
// }}}

void CCache::doDisp(MemRequest *mreq)
/* CCache displaced from higher levels {{{1 */
{
  trackAddress(mreq);

  AddrType addr = mreq->getAddr();

  Line *l=cacheBank->readLine(addr);
  if (l) {
    int16_t portid = router->getCreatorPort(mreq);
    l->adjustState(mreq,portid);
  }

  mreq->ack();
}
// }}}

void CCache::blockFill(MemRequest *mreq)
{
  port->blockFill(mreq);
}

void CCache::doReqAck(MemRequest *mreq)
/* CCache reqAck {{{1 */
{
  MTRACE("doReqAck start");
  trackAddress(mreq);

  mreq->recoverReqAction();

  AddrType addr = mreq->getAddr();
  Line *l = cacheBank->readLine(addr);
  // It could be l!=0 if we requested a check in the lower levels to change state.
  if (l == 0) {
    MTRACE("doReqAck allocatingline");
    l = allocateLine(addr, mreq);
  }else{
    if (notifyHigherLevels(l,mreq)) {
      // FIXME I(0);
      MTRACE("doReqAck Notifying Higher Levels");
      I(mreq->hasPendingSetStateAck());
      return;
    }
  }

  s_reqSetState[mreq->getAction()]->inc(mreq->getStatsFlag());

  int16_t portid = router->getCreatorPort(mreq);
  GI(portid<0,mreq->isHomeNode());
	l->adjustState(mreq, portid);

  Time_t when = port->reqDone(mreq);
  if (when==0) {
    MTRACE("doReqAck restartReqAck");
    // Must restart request
    mreq->setRetrying();
    mreq->restartReqAckAbs(globalClock+1);
    return;
  }

  port->reqRetire(mreq);
  mshr->retire(addr,mreq);

  avgMissLat.sample(mreq->getTimeDelay(when), mreq->getStatsFlag());
  avgMemLat.sample(mreq->getTimeDelay(when), mreq->getStatsFlag());

  if(mreq->isHomeNode()) {
    MTRACE("doReqAck isHomeNode, calling ackAbs");
		mreq->ackAbs(when);
  }else {
    MTRACE("doReqAck is Not HomeNode, calling ackAbsCB %u", when);
    if (!mreq->isWarmup())
      when = inOrderUpMessageAbs(when);
    router->scheduleReqAckAbs(mreq,when);
  }
}
// }}}

void CCache::doSetState(MemRequest *mreq)
/* CCache invalidate request {{{1 */
{
  trackAddress(mreq);
  I(!mreq->isHomeNode());
  if (!inclusive || !needsCoherence) {
		// If not inclusive, do whatever we want
    I(mreq->getCurrMem() == this);
		mreq->convert2SetStateAck(ma_setInvalid, false);
    router->scheduleSetStateAck(mreq, 1); // invalidate ack with nothing else if not inclusive

		MTRACE("scheduleSetStateAck without disp (incoherent cache)");
    return;
  }

  Line *l = cacheBank->readLine(mreq->getAddr());
  if (l==0) {
		// Done!
		mreq->convert2SetStateAck(ma_setInvalid, false);
    router->scheduleSetStateAck(mreq, 1);

		MTRACE("scheduleSetStateAck without disp (local miss)");
    return;
  }

  int16_t portid = router->getCreatorPort(mreq);
	if (l->getSharingCount()) {
    if (portid>=0)
      l->removeSharing(portid);
    if (directory) {
			if (l->getSharingCount() == 1) {
        invOne.inc(mreq->getStatsFlag());
				int32_t i = router->sendSetStateOthersPos(l->getFirstSharingPos(), mreq, mreq->getAction(), inOrderUpMessage(1));
        I(i);
      }else{
        invAll.inc(mreq->getStatsFlag());
        // FIXME: optimize directory for 2 or more
        int32_t nmsg = router->sendSetStateAll(mreq, mreq->getAction(), inOrderUpMessage(1));
        I(nmsg);
      }
    }else{
      invAll.inc(mreq->getStatsFlag());
      int32_t nmsg = router->sendSetStateAll(mreq, mreq->getAction(), inOrderUpMessage(1));
      I(nmsg);
    }
	}else{
    invNone.inc(mreq->getStatsFlag());
    // We are done
		bool needsDisp = l->needsDisp();
		l->adjustState(mreq,portid);
		GI(mreq->getAction() == ma_setInvalid, l->isInvalid());
		GI(mreq->getAction() == ma_setShared , l->isShared());

		mreq->convert2SetStateAck(mreq->getAction(), needsDisp); // Keep shared or invalid
    router->scheduleSetStateAck(mreq, 1);

		MTRACE("scheduleSetStateAck %s disp", needsDisp?"with":"without");
  }
}
// }}}

void CCache::doSetStateAck(MemRequest *mreq)
/* CCache invalidate request Ack {{{1 */
{
  trackAddress(mreq);

  Line *l = cacheBank->readLine(mreq->getAddr());
  if (l) {
    bool needsDisp = l->needsDisp();
    int16_t portid = router->getCreatorPort(mreq);

    l->adjustState(mreq,portid);
    if (needsDisp)
      mreq->setNeedsDisp();
  }

  if(mreq->isHomeNode()) {
		MTRACE("scheduleSetStateAck %s disp (home) line is %d" , mreq->isDisp()?"with":"without", l?l->getState():-1);

    avgSnoopLat.sample(mreq->getTimeDelay()+1, mreq->getStatsFlag());
		mreq->ack(1); // to give time for the same cycle disp
  }else{
		router->scheduleSetStateAck(mreq, 1);
		MTRACE("scheduleSetStateAck %s disp (forward)", mreq->isDisp()?"with":"without");
	}
}
// }}}

void CCache::req(MemRequest *mreq)
/* main processor read entry point {{{1 */
{
  // predicated ARM instructions can be with zero address
  if(mreq->getAddr() == 0) {
    mreq->ack(1);
    return;
  }
	//I(!mreq->isRetrying());
  port->req(mreq);
}
// }}}

void CCache::reqAck(MemRequest *mreq)
/* request Ack {{{1 */
{
	//I(!mreq->isRetrying());
  //
  MTRACE("Received reqAck request");
  if (mreq->isRetrying()){
    MTRACE("reqAck clearRetrying");
    mreq->clearRetrying();
  }
  else{
    s_reqAck[mreq->getAction()]->inc(mreq->getStatsFlag());
  }

  port->reqAck(mreq);
}
// }}}

void CCache::setState(MemRequest *mreq)
/* set state {{{1 */
{

  if (incoherent){
    mreq->convert2SetStateAck(ma_setInvalid,true);
  }

  s_reqSetState[mreq->getAction()]->inc(mreq->getStatsFlag());

	I(!mreq->isRetrying());
  port->setState(mreq);
}
// }}}

void CCache::setStateAck(MemRequest *mreq)
/* set state ack {{{1 */
{
	I(!mreq->isRetrying());
  port->setStateAck(mreq);
}
// }}}

void CCache::disp(MemRequest *mreq)
/* displace a CCache line {{{1 */
{
  displacedRecv.inc(mreq->getStatsFlag());
	I(!mreq->isRetrying());
  port->disp(mreq);
}
// }}}

bool CCache::isBusy(AddrType addr) const
/* check if CCache can accept more writes {{{1 */
{
  if(port->isBusy(addr))
    return true;

  if (!mshr->canAccept(addr))
    return true;

  return false;
}
// }}}

void CCache::dump() const
/* Dump some CCache statistics {{{1 */
{

  mshr->dump();
}
// }}}

TimeDelta_t CCache::ffread(AddrType addr)
/* can accept reads? {{{1 */
{
  AddrType addr_r=0;

  Line *l = cacheBank->readLine(addr);
  if (l)
    return 1;   // done!

  l = cacheBank->fillLine(addr, addr_r);
  l->setExclusive(); // WARNING, can create random inconsistencies (no inv others)

  return router->ffread(addr) + 1;
}
// }}}

TimeDelta_t CCache::ffwrite(AddrType addr)
/* can accept writes? {{{1 */
{
  AddrType addr_r=0;

  Line *l = cacheBank->writeLine(addr);
  if (l) {
  }else{
		l = cacheBank->fillLine(addr, addr_r);
	}
	if (router->isTopLevel())
		l->setModified(); // WARNING, can create random inconsistencies (no inv others)
	else
		l->setExclusive(); 

  return router->ffwrite(addr) + 1;
}
// }}}

void CCache::setTurboRatio(float r)
  // {{{1
{
  I(coreCoupledFreq);
}
// }}}

void CCache::setNeedsCoherence()
  // {{{1
{
	needsCoherence = true;
}
// }}}

void CCache::clearNeedsCoherence()
  // {{{1
{
	needsCoherence = false;
}
// }}}

