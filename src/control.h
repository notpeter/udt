/*****************************************************************************
Copyright © 2001 - 2007, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 4

National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.ncdm.uic.edu/

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at
your option) any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
*****************************************************************************/

/*****************************************************************************
This file contains the definition of classes for UDT congestion control block.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 07/17/2007
*****************************************************************************/

#ifndef _UDT_CONTROL_H_
#define _UDT_CONTROL_H_

#include "udt.h"
#include "common.h"
#include <list>
#include <set>
#include <map>


class CUDT;

struct CHistoryBlock
{
   uint32_t m_IP[4];
   int m_iIPversion;
   uint64_t m_ullTimeStamp;
   int m_iRTT;
   int m_iBandwidth;
   int m_iLossRate;
   int m_iReorderDistance;
};

struct CIPComp
{
   bool operator()(const CHistoryBlock* hb1, const CHistoryBlock* hb2) const;
};

struct CTSComp
{
   bool operator()(const CHistoryBlock* hb1, const CHistoryBlock* hb2) const;
};

class CHistory
{
public:
   CHistory();
   CHistory(const unsigned int& size);
   ~CHistory();

public:
   void update(const sockaddr* addr, const int& ver, const int& rtt, const int& bw);
   void update(const CHistoryBlock* hb);
   int lookup(const sockaddr* addr, const int& ver, CHistoryBlock* hb);

private:
   void convert(const sockaddr* addr, const int& ver, uint32_t* ip);

private:
   unsigned int m_uiSize;
   std::set<CHistoryBlock*, CIPComp> m_sIPIndex;
   std::set<CHistoryBlock*, CTSComp> m_sTSIndex;

   pthread_mutex_t m_Lock;
};

struct CUDTComp
{
   bool operator()(const CUDT* u1, const CUDT* u2) const;
};

class CControl
{
public:
   CControl();
   ~CControl();

public:
   int join(CUDT* udt, const sockaddr* addr, const int& ver, int& rtt, int& bw);
   void leave(CUDT* udt, const int& rtt, const int& bw);

private:
   CHistory* m_pHistoryRecord;
   std::map<CHistoryBlock*, std::set<CUDT*, CUDTComp>, CIPComp> m_mControlBlock;
   std::map<CUDT*, CHistoryBlock*, CUDTComp> m_mUDTIndex;

   pthread_mutex_t m_Lock;
};

#endif
