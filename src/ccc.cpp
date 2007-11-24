/*****************************************************************************
Copyright (c) 2001 - 2007, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
This header file contains the definition of UDT/CCC base class.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 01/07/2007
*****************************************************************************/


#include "core.h"
#include "ccc.h"


CCC::CCC():
m_dPktSndPeriod(1.0),
m_dCWndSize(16.0),
m_iACKPeriod(0),
m_iACKInterval(0),
m_iRTO(-1),
m_bUserDefinedRTO(false)
{
}

void CCC::setACKTimer(const int& msINT)
{
   m_iACKPeriod = msINT;
}

void CCC::setACKInterval(const int& pktINT)
{
   m_iACKInterval = pktINT;
}

void CCC::setRTO(const int& usRTO)
{
   m_iRTO = usRTO;
   m_bUserDefinedRTO = true;
}

void CCC::sendCustomMsg(CPacket& pkt) const
{
   CUDT* u = CUDT::getUDTHandle(m_UDT);
   if (NULL != u)
      *(u->m_pChannel) << pkt;
}

const CPerfMon* CCC::getPerfInfo()
{
   CUDT* u = CUDT::getUDTHandle(m_UDT);
   if (NULL != u)
      u->sample(&m_PerfInfo, false);

   return &m_PerfInfo;
}
