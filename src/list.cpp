/*****************************************************************************
Copyright © 2001 - 2004, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Lilbrary (UDT)

Laboratory for Advanced Computing (LAC)
National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.lac.uic.edu/

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software (UDT) and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimers.

Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimers in the documentation
and/or other materials provided with the distribution.

Neither the names of the University of Illinois, LAC/NCDM, nor the names
of its contributors may be used to endorse or promote products derived
from this Software without specific prior written permission.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*****************************************************************************/

/*****************************************************************************
This file contains the implementation of UDT loss lists and irregular packet
list management modules.

All the lists are static linked lists in ascending order of sequence numbers.
*****************************************************************************/

/*****************************************************************************
written by 
   Yunhong Gu [ygu@cs.uic.edu], last updated 01/23/2004
*****************************************************************************/


#include "udt.h"


// Definition of >, <, >=, and <= with sequence number wrap

inline const bool CList::greaterthan(const __int32& seqno1, const __int32& seqno2) const
{
   if ((seqno1 > seqno2) && (seqno1 - seqno2 < m_iSeqNoTH))
      return true;
 
   if (seqno1 < seqno2 - m_iSeqNoTH)
      return true;

   return false;
}

inline const bool CList::lessthan(const __int32& seqno1, const __int32& seqno2) const
{
   return greaterthan(seqno2, seqno1);
}

inline const bool CList::notlessthan(const __int32& seqno1, const __int32& seqno2) const
{
   if (seqno1 == seqno2)
      return true;

   return greaterthan(seqno1, seqno2);
}

inline const bool CList::notgreaterthan(const __int32& seqno1, const __int32& seqno2) const
{
   if (seqno1 == seqno2)
      return true;

   return lessthan(seqno1, seqno2);
}

// return the distance between two sequence numbers, parameters are pre-checked
inline const __int32 CList::getLength(const __int32& seqno1, const __int32& seqno2) const
{
   if (seqno2 >= seqno1)
      return seqno2 - seqno1 + 1;
   else if (seqno2 < seqno1 - m_iSeqNoTH)
      return seqno2 - seqno1 + m_iMaxSeqNo + 1;
   else
      return 0;
}

//Definition of ++, and -- with sequence number wrap

inline const __int32 CList::incSeqNo(const __int32& seqno) const
{
   return (seqno + 1) % m_iMaxSeqNo;
}

inline const __int32 CList::decSeqNo(const __int32& seqno) const
{
   return (seqno - 1 + m_iMaxSeqNo) % m_iMaxSeqNo;
}


CSndLossList::CSndLossList(const __int32& size, const __int32& th, const __int32& max):
m_iSize(size)
{
   m_iSeqNoTH = th;
   m_iMaxSeqNo = max;

   m_piData1 = new __int32 [m_iSize];
   m_piData2 = new __int32 [m_iSize];
   m_piNext = new __int32 [m_iSize];

   // -1 means there is no data in the node
   for (__int32 i = 0; i < size; i ++)
   {
      m_piData1[i] = -1;
      m_piData2[i] = -1;
   }

   m_iLength = 0;
   m_iHead = -1;

   // sender list needs mutex protection
   #ifndef WIN32
      pthread_mutex_init(&m_ListLock, 0);
   #else
      m_ListLock = CreateMutex(NULL, false, "SndLossListLock");
   #endif
}

CSndLossList::~CSndLossList()
{
   delete [] m_piData1;
   delete [] m_piData2;
   delete [] m_piNext;
}

__int32 CSndLossList::insert(const __int32& seqno1, const __int32& seqno2)
{
   CGuard listguard(m_ListLock);

   __int32 origlen = m_iLength;

   if (0 == m_iLength)
   {
      // insert data into an empty list

      m_iHead = 0;
      m_piData1[m_iHead] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[m_iHead] = seqno2;

      m_piNext[m_iHead] = -1;

      m_iLength += getLength(seqno1, seqno2);

      return m_iLength - origlen;
   }

   // otherwise find the position where the data can be inserted

   __int32 offset = seqno1 - m_piData1[m_iHead];

   if (offset < -m_iSeqNoTH)
      offset += m_iMaxSeqNo;
   else if (offset > m_iSeqNoTH)
      offset -= m_iMaxSeqNo;

   __int32 loc = (m_iHead + offset + m_iSize) % m_iSize;

   if (offset < 0)
   {
      // Insert data prior to the head pointer

      m_piData1[loc] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[loc] = seqno2;

      // new node becomes head
      m_piNext[loc] = m_iHead;
      m_iHead = loc;

      m_iLength += getLength(seqno1, seqno2);
   }
   else if (offset > 0)
   {
      if (seqno1 == m_piData1[loc])
      {
         // first seqno is equivlent, compare the second
         if (-1 == m_piData2[loc])
         {
            m_iLength += getLength(seqno1, seqno2) - 1;
            m_piData2[loc] = seqno2;
         }
         else if (greaterthan(seqno2, m_piData2[loc]))
         {
            // new seq pair is longer than old pair, e.g., insert [3, 7] to [3, 5], becomes [3, 7]
            m_iLength += getLength(m_piData2[loc], seqno2) - 1;
            m_piData2[loc] = seqno2;
         }
         else
            // Do nothing if it is already there
            return 0;
      }
      else
      {
         // searching the prior node
         __int32 i = (loc - 1 + m_iSize) % m_iSize;
         while (-1 == m_piData1[i])
            i = (i - 1 + m_iSize) % m_iSize;

         if ((-1 == m_piData2[i]) || lessthan(m_piData2[i], seqno1))
         {
            // no overlap, create new node
            m_piData1[loc] = seqno1;
            if (seqno2 != seqno1)
               m_piData2[loc] = seqno2;

            m_piNext[loc] = m_piNext[i];
            m_piNext[i] = loc;

            m_iLength += getLength(seqno1, seqno2);
         }
         else
         {
            // overlap, coalesce with prior node, insert(3, 7) to [2, 5], ... becomes [2, 7]
            if (lessthan(m_piData2[i], seqno2))
            {
               m_iLength += getLength(m_piData2[i], seqno2) - 1;
               m_piData2[i] = seqno2;

               loc = i;
            }
            else
               return 0;
         }
      }
   }
   else
   {
      // insert to head node
      if (seqno2 != seqno1)
      {
         if (-1 == m_piData2[loc])
         {
            m_iLength += getLength(seqno1, seqno2) - 1;
            m_piData2[loc] = seqno2;
         }
         else if (greaterthan(seqno2, m_piData2[loc]))
         {
            m_iLength += getLength(m_piData2[loc], seqno2) - 1;
            m_piData2[loc] = seqno2;
         }
         else 
            return 0;
      }
      else
         return 0;
   }

   // coalesce with next node. E.g., [3, 7], ..., [6, 9] becomes [3, 9] 
   if ((-1 != m_piNext[loc]) && (-1 != m_piData2[loc]))
   {
      __int32 i = m_piNext[loc];

      if (notgreaterthan(m_piData1[i], incSeqNo(m_piData2[loc])))
      {
         // coalesce if there is overlap
         if (-1 != m_piData2[i])
         {
            if (m_piData2[i] > m_piData2[loc])
            {
               if (notlessthan(m_piData2[loc], m_piData1[i]))
                  m_iLength -= getLength(m_piData1[i], m_piData2[loc]);

               m_piData2[loc] = m_piData2[i];
            }
            else
               m_iLength -= getLength(m_piData1[i], m_piData2[i]);
         }
         else
         {
            if (m_piData1[i] == incSeqNo(m_piData2[loc]))
               m_piData2[loc] = m_piData1[i];
            else
               m_iLength --;
         }

         m_piData1[i] = -1;
         m_piData2[i] = -1;
         m_piNext[loc] = m_piNext[i];
      }
   }

   return m_iLength - origlen;
}

void CSndLossList::remove(const __int32& seqno)
{
   CGuard listguard(m_ListLock);

   if (0 == m_iLength)
      return;

   // Remove all from the head pointer to a node with a larger seq. no. or the list is empty

   __int32 offset = seqno - m_piData1[m_iHead];

   if (offset < -m_iSeqNoTH)
      offset += m_iMaxSeqNo;
   else if (offset > m_iSeqNoTH)
      offset -= m_iMaxSeqNo;

   __int32 loc = (m_iHead + offset + m_iSize) % m_iSize;

   if (0 == offset)
   {
      // It is the head. Remove the head and point to the next node
      loc = (loc + 1) % m_iSize;

      if (-1 == m_piData2[m_iHead])
      {
         loc = m_piNext[m_iHead];
      }
      if (greaterthan(m_piData2[m_iHead], incSeqNo(seqno)))
      {
         m_piData1[loc] = incSeqNo(seqno);
         m_piData2[loc] = m_piData2[m_iHead];
         m_piNext[loc] = m_piNext[m_iHead];
      }
      else
      {
         m_piData1[loc] = incSeqNo(seqno);
         m_piNext[loc] = m_piNext[m_iHead];
      }

      m_piData1[m_iHead] = -1;
      m_piData2[m_iHead] = -1;

      m_iHead = loc;

      m_iLength --;
   }
   else if (offset > 0)
   {
      //locate prior node
      __int32 i = (loc - 1 + m_iSize) % m_iSize;
      while (-1 == m_piData1[i])
         i = (i - 1 + m_iSize) % m_iSize;

      __int32 h = m_iHead;

      if (seqno == m_piData1[loc])
      {
         // target node is not empty, remove part/all of the seqno in the node.
         __int32 temp = loc;
         loc = (loc + 1) % m_iSize;         

         if (-1 == m_piData2[temp])
         {
            m_iHead = m_piNext[temp];
         }
         else if (greaterthan(m_piData2[temp], incSeqNo(seqno)))
         {
            // remove part, e.g., [3, 7] becomes [], [5, 7] after remove(4)
            m_piData1[loc] = incSeqNo(seqno);
            m_piData2[loc] = m_piData2[temp];
            m_iHead = loc;
            m_piNext[loc] = m_piNext[temp];
         }
         else
         {
            // [3, 7] becomes [], [7, -1] after remove(6)
            m_piData1[loc] = incSeqNo(seqno);
            m_iHead = loc;
            m_piNext[loc] = m_piNext[temp];
         }

         m_piData1[temp] = -1;
         m_piData2[temp] = -1;

         m_iLength --;

         m_piNext[i] = m_iHead;
      }
      else
      {
         // targe node is empty, check prior node
         loc = (loc + 1) % m_iSize;

         if (-1 == m_piData2[i])
         {
            m_iHead = m_piNext[i];
         }
         else if (greaterthan(m_piData2[i], seqno))
         {
            // remove part/all seqno in the prior node
            m_piData1[loc] = incSeqNo(seqno);
            if (greaterthan(m_piData2[i], incSeqNo(seqno)))
            {
               m_piData2[loc] = m_piData2[i];
               m_iLength += getLength(m_piData1[loc], m_piData2[loc]);
            }
            else
               m_iLength ++;

            m_piNext[loc] = m_piNext[i];
            m_piNext[i] = loc;

            m_iHead = loc;
         }
      }

      // Remove all nodes prior to the new head
      while (h != m_iHead)
      {
         if (m_piData2[h] != -1)
            m_iLength -= getLength(m_piData1[h], m_piData2[h]);
         else
            m_iLength --;

         m_piData1[h] = -1;
         m_piData2[h] = -1;

         h = m_piNext[h];
      }
   }
}

__int32 CSndLossList::getLossLength()
{
   CGuard listguard(m_ListLock);

   return m_iLength;
}

__int32 CSndLossList::getLostSeq()
{
   CGuard listguard(m_ListLock);

   if (0 == m_iLength)
     return -1;

   // return the first loss seq. no.
   __int32 seqno = m_piData1[m_iHead];

   // head moves to the next node
   __int32 loc = (m_iHead + 1) % m_iSize;

   // shift to next node, e.g., [3, 7] becomes [], [4, 7]
   if (-1 == m_piData2[m_iHead])
   {
      loc = m_piNext[m_iHead];
   }
   else if (greaterthan(m_piData2[m_iHead], incSeqNo(seqno)))
   {
      m_piData1[loc] = incSeqNo(seqno);
      m_piData2[loc] = m_piData2[m_iHead];
      m_piNext[loc] = m_piNext[m_iHead];
   }
   else
   {
      m_piData1[loc] = incSeqNo(seqno);
      m_piNext[loc] = m_piNext[m_iHead];
   }

   // remove old head
   m_piData1[m_iHead] = -1;
   m_piData2[m_iHead] = -1;
   m_iHead = loc;
   m_iLength --;

   return seqno;
}


//
CRcvLossList::CRcvLossList(const __int32& size, const __int32& th, const __int32& max):
m_iSize(size)
{
   m_iSeqNoTH = th;
   m_iMaxSeqNo = max;

   m_piData1 = new __int32 [m_iSize];
   m_piData2 = new __int32 [m_iSize];
   m_pLastFeedbackTime = new timeval [m_iSize];
   m_piCount = new __int32 [m_iSize];
   m_piNext = new __int32 [m_iSize];

   // -1 means there is no data in the node
   for (__int32 i = 0; i < size; i ++)
   {
      m_piData1[i] = -1;
      m_piData2[i] = -1;
   }

   m_iLength = 0;
   m_iHead = -1;
}

CRcvLossList::~CRcvLossList()
{
   delete [] m_piData1;
   delete [] m_piData2;
   delete [] m_pLastFeedbackTime;
   delete [] m_piNext;
}

void CRcvLossList::insert(const __int32& seqno1, const __int32& seqno2)
{
   // Data to be inserted must be larger than all those in the list
   // guaranteed by the UDT receiver

   if (0 == m_iLength)
   {
      // insert data into an empty list

      m_iHead = 0;
      m_piData1[m_iHead] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[m_iHead] = seqno2;

      gettimeofday(m_pLastFeedbackTime + m_iHead, 0);
      m_piCount[m_iHead] = 2;

      m_piNext[m_iHead] = -1;
      m_iLength += getLength(seqno1, seqno2);

      return;
   }

   // otherwise searching for the position where the node should be

   __int32 offset = seqno1 - m_piData1[m_iHead];

   if (offset < -m_iSeqNoTH)
      offset += m_iMaxSeqNo;

   __int32 loc = (m_iHead + offset) % m_iSize;

   // searching the previous node
   __int32 i = (loc - 1 + m_iSize) % m_iSize;
   while (-1 == m_piData1[i])
      i = (i - 1 + m_iSize) % m_iSize;

   if ((-1 != m_piData2[i]) && (incSeqNo(m_piData2[i]) == seqno1))
   {
      // coalesce with prior node, e.g., [2, 5], [6, 7] becomes [2, 7]
      loc = i;
      m_piData2[loc] = seqno2;
   }
   else
   {
      // create new node
      m_piData1[loc] = seqno1;

      if (seqno2 != seqno1)
         m_piData2[loc] = seqno2;

      m_piNext[i] = loc;
      m_piNext[loc] = -1;
   }

   // Initilize time stamp
   gettimeofday(m_pLastFeedbackTime + loc, 0);
   m_piCount[loc] = 2;

   m_iLength += getLength(seqno1, seqno2);
}

void CRcvLossList::remove(const __int32& seqno)
{
   // locate the position of "seqno" in the list

   __int32 offset = seqno - m_piData1[m_iHead];

   if (offset < -m_iSeqNoTH)
      offset += m_iMaxSeqNo;

   __int32 loc = (m_iHead + offset) % m_iSize;

   if (seqno == m_piData1[loc])
   {
      // This is a seq. no. that starts the loss sequence

      if (-1 == m_piData2[loc])
      {
         // there is only 1 loss in the sequence, delete it from the node

         if (m_iHead == loc)
            m_iHead = m_piNext[m_iHead];
         else
         {
            // searching previous node
            __int32 i = (loc - 1 + m_iSize) % m_iSize;
            while (-1 == m_piData1[i])
               i = (i - 1 + m_iSize) % m_iSize;

            m_piNext[i] = m_piNext[loc];;
         }

         m_piData1[loc] = -1;
      }
      else
      {
         // there are more than 1 loss in the sequence
         // move the node to the next and update the starter as the next loss inSeqNo(seqno)

         // find next node
         __int32 i = (loc + 1) % m_iSize;

         // remove the "seqno" and change the starter as next seq. no.
         m_piData1[i] = incSeqNo(m_piData1[loc]);

         // process the sequence end
         if (greaterthan(m_piData2[loc], incSeqNo(m_piData1[loc])))
            m_piData2[i] = m_piData2[loc];

         // replicate the time stamp and report counter
         m_pLastFeedbackTime[i] = m_pLastFeedbackTime[loc];
         m_piCount[i] = m_piCount[loc];

         // remove the current node
         m_piData1[loc] = -1;
         m_piData2[loc] = -1;
 
         // update list pointer
         m_piNext[i] = m_piNext[loc];

         if (m_iHead == loc)
            m_iHead = i;
         else
         {
            // searching for the previous node
            __int32 j = (i - 1 + m_iSize) % m_iSize;
            while (-1 == m_piData1[j])
               j = (j - 1 + m_iSize) % m_iSize;

            m_piNext[j] = i;
         }
      }

      m_iLength --;

      return;
   }
   else if (offset > 0)
   {
      // There is no loss sequence in the current position
      // the "seqno" may not be in the list, or it is contained in a previous node

      // no loss at all, return
      if (0 == m_iLength)
          return;

      // searching previous node
      __int32 i = (loc - 1 + m_iSize) % m_iSize;
      while (-1 == m_piData1[i])
         i = (i - 1 + m_iSize) % m_iSize;

      // not contained in this node, return
      if ((-1 == m_piData2[i]) || greaterthan(seqno, m_piData2[i]))
          return;

      if (seqno == m_piData2[i])
      {
         // it is the sequence end

         if (seqno == incSeqNo(m_piData1[i]))
            m_piData2[i] = -1;
         else
            m_piData2[i] = decSeqNo(seqno);
      }
      else
      {
         // split the sequence

         // construct the second sequence from incSeqNo(seqno) to the original sequence end
         // located at "loc + 1"
         loc = (loc + 1) % m_iSize;

         m_piData1[loc] = incSeqNo(seqno);
         if (greaterthan(m_piData2[i], incSeqNo(seqno)))
            m_piData2[loc] = m_piData2[i];

         // the first (original) sequence is between the original sequence start to decSeqNo(seqno)
         if (seqno == incSeqNo(m_piData1[i]))
            m_piData2[i] = -1;
         else
            m_piData2[i] = decSeqNo(seqno);

         // replicate the time stamp and report counter
         m_pLastFeedbackTime[loc] = m_pLastFeedbackTime[i];
         m_piCount[loc] = m_piCount[i];

         // update the list pointer
         m_piNext[loc] = m_piNext[i];
         m_piNext[i] = loc;
      }

      m_iLength --;

      return;
   }
}

__int32 CRcvLossList::getLossLength() const
{
   return m_iLength;
}

__int32 CRcvLossList::getFirstLostSeq() const
{
   if (0 == m_iLength)
      return -1;

   return m_piData1[m_iHead];
}

void CRcvLossList::getLossArray(__int32* array, __int32* len, const __int32& limit, const __int32& interval)
{
   timeval currtime;
   gettimeofday(&currtime, 0);

   __int32 i  = m_iHead;

   len[0] = 0;
   len[1] = 0;

   while ((len[1] < limit - 1) && (-1 != i))
   {
      if ((currtime.tv_sec - m_pLastFeedbackTime[i].tv_sec) * 1000000 + currtime.tv_usec - m_pLastFeedbackTime[i].tv_usec > m_piCount[i] * interval)
      {
         array[len[1]] = m_piData1[i];
         if (-1 != m_piData2[i])
         {
            // there are more than 1 loss in the sequence

            array[len[1]] |= 0x80000000;
            len[1] ++;
            array[len[1]] = m_piData2[i];
            len[0] += getLength(m_piData1[i], m_piData2[i]);
         }
         else
            // there is only 1 loss in the current node
            len[0] ++;

         len[1] ++;

         // update the timestamp
         gettimeofday(m_pLastFeedbackTime + i, 0);
         // update how many times this loss has been fed back, the "k" in UDT paper
         m_piCount[i] ++;
      }

      i = m_piNext[i];
   }
}


//
CIrregularPktList::CIrregularPktList(const __int32& size, const __int32& th, const __int32& max):
m_iSize(size)
{
   m_iSeqNoTH = th;
   m_iMaxSeqNo = max;

   m_piData = new __int32 [m_iSize];
   m_piErrorSize = new __int32 [m_iSize];
   m_piNext = new __int32 [m_iSize];

   // -1 means there is no data in the node
   for (__int32 i = 0; i < size; i ++)
      m_piData[i] = -1;

   m_iCurrErrSize = 0;
   m_iLength = 0;
   m_iHead = -1;
}

CIrregularPktList::~CIrregularPktList()
{
   delete [] m_piData;
   delete [] m_piErrorSize;
   delete [] m_piNext;
}

__int32 CIrregularPktList::currErrorSize() const
{
   return m_iCurrErrSize;
}

__int32 CIrregularPktList::currErrorSize(const __int32& seqno) const
{
   if (0 == m_iLength)
      return 0;

   __int32 size = 0;
   __int32 i = m_iHead;

   // calculate the sum of the size error until the node with a seq. no. larger than "seqno"
   while ((-1 != i) && notgreaterthan(m_piData[i], seqno))
   {
      size += m_piErrorSize[i];
      i = m_piNext[i];
   }

   return size;
}

void CIrregularPktList::addIrregularPkt(const __int32& seqno, const __int32& errsize)
{
   if (0 == m_iLength)
   {
      // insert into an empty list

      m_iHead = 0;
      m_piData[m_iHead] = seqno;
      m_piErrorSize[m_iHead] = errsize;
      m_piNext[m_iHead] = -1;
      m_iCurrErrSize += errsize;
      m_iLength ++;

      return;
   }

   // positioning...

   __int32 offset = seqno - m_piData[m_iHead];

   if (offset < -m_iSeqNoTH)
      offset += m_iMaxSeqNo;
   else if (offset > m_iSeqNoTH)
      offset -= m_iMaxSeqNo;

   __int32 loc = (m_iHead + offset + m_iSize) % m_iSize;

   if (offset < 0)
   {
      // insert at head

      m_piData[loc] = seqno;
      m_piErrorSize[loc] = errsize;
      m_piNext[loc] = m_iHead;
      m_iHead = loc;
      m_iCurrErrSize += errsize;
      m_iLength ++;

      return;
   }
   else if (offset > 0)
   {
      // return if it is already there
      if (seqno == m_piData[loc])
         return;

      __int32 i = (loc - 1 + m_iSize) % m_iSize;

      // looking for the prior node
      while (-1 == m_piData[i])
         i = (i - 1 + m_iSize) % m_iSize;

      // insert the node
      m_piNext[loc] = m_piNext[i];
      m_piNext[i] = loc;

      m_piData[loc] = seqno;
      m_piErrorSize[loc] = errsize;
      m_iCurrErrSize += errsize;
      m_iLength ++;
   }
}

void CIrregularPktList::deleteIrregularPkt(const __int32& seqno)
{
   // remove all node until the one with seq. no. large than the parameter

   __int32 i = m_iHead;
   while ((-1 != i) && notgreaterthan(m_piData[i], seqno))
   {
      m_piData[i] = -1;
      m_iCurrErrSize -= m_piErrorSize[i];
      m_iLength --;
      i = m_piNext[i];
   }

   m_iHead = i;
}
