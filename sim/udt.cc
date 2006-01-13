//
// Author: Yunhong Gu, ygu@cs.uic.edu
//
// Description: 
//
// Assumption: This code does NOT process sequence number wrap, which will overflow after 2^31 packets.
//             But I assume that you won't run NS for that long time :)
//
// Last Update: 01/05/2006
//

#include <stdlib.h>
#include <math.h>
#include "ip.h"
#include "udt.h"

int hdr_udt::off_udt_;

static class UDTHeaderClass : public PacketHeaderClass 
{
public:
   UDTHeaderClass() : PacketHeaderClass("PacketHeader/UDT", sizeof(hdr_udt)) 
   {
      bind_offset(&hdr_udt::off_udt_);
   }
} class_udthdr;

static class UdtClass : public TclClass 
{
public:
   UdtClass() : TclClass("Agent/UDT") {}
   TclObject* create(int, const char*const*) 
   {
      return (new UdtAgent());
   }
} class_udt;


UdtAgent::UdtAgent(): Agent(PT_UDT),
syn_timer_(this),
ack_timer_(this),
nak_timer_(this),
exp_timer_(this),
snd_timer_(this),
syn_interval_(0.01),
mtu_(1500),
max_flow_window_(100000)
{
   bind("mtu_", &mtu_);
   bind("max_flow_window_", &max_flow_window_);

   snd_loss_list_ = new CSndLossList(max_flow_window_, 1 << 29, 1 << 30);
   rcv_loss_list_ = new CRcvLossList(max_flow_window_, 1 << 29, 1 << 30);

   flow_window_size_ = 2;
   snd_interval_ = 0.000001;

   ack_interval_ = syn_interval_;
   nak_interval_ = syn_interval_;
   exp_interval_ = 1.01;
   
   nak_count_ = 0;
   dec_count_ = 0;
   snd_last_ack_ = 0;
   local_send_ = 0;
   local_loss_ = 0;
   local_ack_ = 0;
   snd_curr_seqno_ = -1;
   curr_max_seqno_ = 0;
   avg_nak_num_ = 2;
   dec_random_ = 2;

   loss_rate_limit_ = 0.01;
   loss_rate_ = 0;

   rtt_ = 1;
   
   rcv_interval_ = snd_interval_;
   rcv_last_ack_ = 0;
   rcv_last_ack_time_ = Scheduler::instance().clock();
   rcv_last_ack2_ = 0;
   ack_seqno_ = -1;
   rcv_curr_seqno_ = -1;
   local_recv_ = 0;
   last_dec_seq_ = -1;
   last_delay_time_ = Scheduler::instance().clock();
   last_dec_int_ = 1.0;

   slow_start_ = true;
   freeze_ = false;

   ack_timer_.resched(ack_interval_);
   nak_timer_.resched(nak_interval_);
}

UdtAgent::~UdtAgent()
{
}

int UdtAgent::command(int argc, const char*const* argv)
{
   return Agent::command(argc, argv);
}

void UdtAgent::recv(Packet *pkt, Handler*)
{
   hdr_udt* udth = hdr_udt::access(pkt);

   double r;

   if (1 == udth->flag())
   {
      switch (udth->type())
      {
      case 2:
         sendCtrl(6, udth->ackseq());

         if (udth->ack() > snd_last_ack_)
         {
            snd_last_ack_ = udth->ack();
            snd_loss_list_->remove((int)snd_last_ack_);
         }
         else
            break;

         snd_timer_.resched(0);

         if (rtt_ == syn_interval_)
            rtt_ = udth->rtt() / 1000000.0;
         else
            rtt_ = rtt_ * 0.875 + udth->rtt() / 1000000.0 * 0.125;

         if (slow_start_)
            flow_window_size_ = snd_last_ack_;
         else if (udth->lrecv() > 0)
            flow_window_size_ = int(ceil(flow_window_size_ * 0.875 + udth->lrecv() * (rtt_ + syn_interval_) * 0.125));

         if (flow_window_size_ > max_flow_window_)
         {
            slow_start_ = false;

            flow_window_size_ = max_flow_window_;
         }

         bandwidth_ = int(bandwidth_ * 0.875 + udth->bandwidth() * 0.125);

         exp_timer_.resched(exp_interval_);

         rateControl();

         if (snd_interval_ > rtt_)
         {
            snd_interval_ = rtt_;
            snd_timer_.resched(0);
         }

         break;

      case 3:
         slow_start_ = false;

         last_dec_int_ = snd_interval_;

         if ((udth->loss()[0] & 0x7FFFFFFF) > last_dec_seq_)
         {
            freeze_ = true;
            snd_interval_ = snd_interval_ * 1.125;

            avg_nak_num_ = 1 + int(ceil(double(avg_nak_num_) * 0.875 + double(nak_count_) * 0.125));

            dec_random_ = int(rand() * double(avg_nak_num_) / (RAND_MAX + 1.0)) + int(ceil(avg_nak_num_/5.0));

            nak_count_ = 1;

            last_dec_seq_ = snd_curr_seqno_;
         }
         else if (0 == (++ nak_count_ % dec_random_))
         {
            snd_interval_ = snd_interval_ * 1.125;

            last_dec_seq_ = snd_curr_seqno_;
         }

         if (snd_interval_ > rtt_)
            snd_interval_ = rtt_;

         local_loss_ ++;

         for (int i = 0, n = udth->losslen(); i < n; ++ i)
         {
            if ((udth->loss()[i] & 0x80000000) && ((udth->loss()[i] & 0x7FFFFFFF) >= snd_last_ack_))
            {
               snd_loss_list_->insert(udth->loss()[i] & 0x7FFFFFFF, udth->loss()[i + 1]);
               ++ i;
            }
            else if (udth->loss()[i] >= snd_last_ack_)
            {

               snd_loss_list_->insert(udth->loss()[i], udth->loss()[i]);
            }
         }

         exp_timer_.resched(exp_interval_);

         snd_timer_.resched(0);

         break;
     
      case 4:
/*
         if (slow_start_)
            slow_start_ = false;

         last_dec_int_ = snd_interval_;

         snd_interval_ = snd_interval_ * 1.125;

         last_dec_seq_ = snd_curr_seqno_;
         nak_count_ = -16;
         dec_count_ = 1;
*/
         break;

      case 6:
      {
         int ack;
         double rtt = ack_window_.acknowledge(udth->ackseq(), ack);

         if (rtt > 0)
         {
            time_window_.ack2arrival(rtt);

//            if ((time_window_.getdelaytrend()) && (Scheduler::instance().clock() - last_delay_time_ > 2 * rtt_))
//               sendCtrl(4);

            if (rtt_ == syn_interval_)
               rtt_ = rtt;
            else
               rtt_ = rtt_ * 0.875 + rtt * 0.125;

            nak_interval_ = rtt_;
            if (nak_interval_ < syn_interval_)
               nak_interval_ = syn_interval_;

            if (rcv_last_ack2_ < ack)
               rcv_last_ack2_ = ack;
         }

         break;
      }

      default:
         break;
      }

      Packet::free(pkt);
      return;
   }
   

   time_window_.pktarrival();

   if (0 == udth->seqno() % 16)
      time_window_.probe1arrival();
   else if (1 == udth->seqno() % 16)
      time_window_.probe2arrival();

   local_recv_ ++;

   int offset = udth->seqno() - rcv_last_ack_;
 
   if (offset < 0)
   {
      Packet::free(pkt);
      return;
   }

   if (udth->seqno() > rcv_curr_seqno_ + 1)
   {
      int c;

      if (rcv_curr_seqno_ + 1 == udth->seqno() - 1)
         c = 1;
      else
         c = 2;

      int* loss = new int[c];

      if (c == 2)
      {
         loss[0] = (rcv_curr_seqno_ + 1) | 0x80000000;
         loss[1] = udth->seqno() - 1;
      }
      else
         loss[0] = rcv_curr_seqno_ + 1;

      sendCtrl(3, c, loss);

      delete [] loss;
   }

   if (udth->seqno() > rcv_curr_seqno_)
   {
      rcv_curr_seqno_ = udth->seqno();
   }
   else
   {
      rcv_loss_list_->remove(udth->seqno());
   }

   Packet::free(pkt);
   return;
}

void UdtAgent::sendmsg(int nbytes, const char* /*flags*/)
{
   if (curr_max_seqno_ == snd_curr_seqno_ + 1)
      exp_timer_.resched(exp_interval_);

   curr_max_seqno_ += nbytes/1468;

   snd_timer_.resched(0);
}

void UdtAgent::sendCtrl(int pkttype, int lparam, int* rparam)
{
   Packet* p;
   hdr_udt* udth;
   hdr_cmn* ch;

   int ack;

   switch (pkttype)
   {
   case 2:
      if (rcv_loss_list_->getLossLength() == 0)
         ack = rcv_curr_seqno_ + 1;
      else
         ack = rcv_loss_list_->getFirstLostSeq();

      if (ack > rcv_last_ack_)
      {
         rcv_last_ack_ = ack;
      }
      else if (Scheduler::instance().clock() - rcv_last_ack_time_ <= 2 * rtt_)
      {
         ack_timer_.resched(ack_interval_);

         break;
      }

      if (rcv_last_ack_ > rcv_last_ack2_)
      {
         p = allocpkt(40);
         udth = hdr_udt::access(p);

         udth->flag() = 1;
         udth->type() = 2;
         udth->lrecv() = time_window_.getpktspeed();
         udth->bandwidth() = time_window_.getbandwidth();
         udth->rtt() = int(rtt_ * 1000000.0);

         ack_seqno_ ++;
         udth->ackseq() = ack_seqno_;
         udth->ack() = rcv_last_ack_;

         ch = hdr_cmn::access(p);
         ch->size() = 40;
         Agent::send(p, 0);

         ack_window_.store(ack_seqno_, rcv_last_ack_);

         rcv_last_ack_time_ = Scheduler::instance().clock();
      }

      ack_timer_.resched(ack_interval_);

      break;

   case 3:
      if (rparam != NULL)
      {
         p = allocpkt(32 + lparam * 4);
         udth = hdr_udt::access(p);

         udth->flag() = 1;
         udth->type() = 3;
         udth->losslen() = lparam;
         memcpy(udth->loss(), rparam, lparam * 4);

         ch = hdr_cmn::access(p);
         ch->size() = 32 + lparam * 4;
         Agent::send(p, 0);
      }
      else if (rcv_loss_list_->getLossLength() > 0)
      {
         int losslen;
         int* loss = new int[MAX_LOSS_LEN];
         rcv_loss_list_->getLossArray(loss, &losslen, MAX_LOSS_LEN, rtt_);
 
         if (losslen > 0)
         {
            p = allocpkt(32 + losslen);
            udth = hdr_udt::access(p);

            udth->flag() = 1;
            udth->type() = 3;
            udth->losslen() = losslen;
            memcpy(udth->loss(), loss, MAX_LOSS_LEN);

            ch = hdr_cmn::access(p);
            ch->size() = 32 + losslen;
            Agent::send(p, 0);
         }

         delete [] loss;
      }

      nak_timer_.resched(nak_interval_);

      break;

   case 4:
      p = allocpkt(32);
      udth = hdr_udt::access(p);

      udth->flag() = 1;
      udth->type() = 4;

      ch = hdr_cmn::access(p);
      ch->size() = 32;
      Agent::send(p, 0);

      last_delay_time_ = Scheduler::instance().clock();

      break;

   case 6:
      p = allocpkt(32);
      udth = hdr_udt::access(p);

      udth->flag() = 1;
      udth->type() = 6;
      udth->ackseq() = lparam;

      ch = hdr_cmn::access(p);
      ch->size() = 32;
      Agent::send(p, 0);

      break;
   }
}

void UdtAgent::sendData()
{
   bool probe = false;

   if (snd_last_ack_ == curr_max_seqno_)
      snd_timer_.resched(snd_interval_);

   int nextseqno;

   if (snd_loss_list_->getLossLength() > 0)
   {
      nextseqno = snd_loss_list_->getLostSeq();
   }
   else if (snd_curr_seqno_ - snd_last_ack_ < flow_window_size_)
   {
      nextseqno = ++ snd_curr_seqno_;
      if (0 == nextseqno % 16)
         probe = true;
   }
   else
   {
/*
      if (freeze_)
      {
         snd_timer_.resched(syn_interval_ + snd_interval_);
         freeze_ = false;
      }
      else
         snd_timer_.resched(snd_interval_);
*/
      return;
   }

   Packet* p;

   p = allocpkt(mtu_);
   hdr_udt* udth = hdr_udt::access(p);
   udth->flag() = 0;
   udth->seqno() = nextseqno;

   hdr_cmn* ch = hdr_cmn::access(p);
   ch->size() = mtu_;
   Agent::send(p, 0);

   local_send_ ++;

   if (probe)
   {
      snd_timer_.resched(0);
      return;
   }

   if (freeze_)
   {
      snd_timer_.resched(syn_interval_ + snd_interval_);
      freeze_ = false;
   }
   else
      snd_timer_.resched(snd_interval_);
}

void UdtAgent::rateControl()
{
   if (slow_start_)
      return;

   double inc = 0.0;

   if (bandwidth_ < 1.0 / snd_interval_)
      inc = 1.0/mtu_;
   else
   {
      inc = pow(10, ceil(log10((bandwidth_ - 1.0 / snd_interval_) * mtu_ * 8))) * 0.0000015 / mtu_;

      if (inc < 1.0/mtu_)
         inc = 1.0/mtu_;
   }

   snd_interval_ = (snd_interval_ * syn_interval_) / (snd_interval_ * inc + syn_interval_);

   if (snd_interval_ < 0.000001)
      snd_interval_ = 0.000001;
}

void UdtAgent::timeOut()
{
   if (snd_curr_seqno_ >= snd_last_ack_)
   {
      snd_loss_list_->insert(int(snd_last_ack_), int(snd_curr_seqno_));
   }

   exp_interval_ = 1.0; //rtt_ + syn_interval_;

   exp_timer_.resched(exp_interval_);

   snd_timer_.resched(0);
}

/////////////////////////////////////////////////////////////////
void SndTimer::expire(Event*)
{
   a_->sendData();
}

void SynTimer::expire(Event*)
{
   a_->rateControl();
}

void AckTimer::expire(Event*)
{
   a_->sendCtrl(2);
}

void NakTimer::expire(Event*)
{
   a_->sendCtrl(3);
}

void ExpTimer::expire(Event*)
{
   a_->timeOut();
}

////////////////////////////////////////////////////////////////////

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
   for (__int32 i = 0; i < size; ++ i)
   {
      m_piData1[i] = -1;
      m_piData2[i] = -1;
   }

   m_iLength = 0;
   m_iHead = -1;
   m_iLastInsertPos = -1;
}

CSndLossList::~CSndLossList()
{
   delete [] m_piData1;
   delete [] m_piData2;
   delete [] m_piNext;
}

__int32 CSndLossList::insert(const __int32& seqno1, const __int32& seqno2)
{
   if (0 == m_iLength)
   {
      // insert data into an empty list

      m_iHead = 0;
      m_piData1[m_iHead] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[m_iHead] = seqno2;

      m_piNext[m_iHead] = -1;
      m_iLastInsertPos = m_iHead;

      m_iLength += getLength(seqno1, seqno2);

      return m_iLength;
   }

   // otherwise find the position where the data can be inserted
   __int32 origlen = m_iLength;

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
      m_iLastInsertPos = loc;

      m_iLength += getLength(seqno1, seqno2);
   }
   else if (offset > 0)
   {
      if (seqno1 == m_piData1[loc])
      {
         m_iLastInsertPos = loc;

         // first seqno is equivlent, compare the second
         if (-1 == m_piData2[loc])
         {
            if (seqno2 != seqno1)
            {
               m_iLength += getLength(seqno1, seqno2) - 1;
               m_piData2[loc] = seqno2;
            }
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
         __int32 i;
         if ((-1 != m_iLastInsertPos) && lessthan(m_piData1[m_iLastInsertPos], seqno1))
            i = m_iLastInsertPos;
         else
            i = m_iHead;

         while ((-1 != m_piNext[i]) && lessthan(m_piData1[m_piNext[i]], seqno1))
            i = m_piNext[i];

         if ((-1 == m_piData2[i]) || lessthan(m_piData2[i], seqno1))
         {
            m_iLastInsertPos = loc;

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
            m_iLastInsertPos = i;

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
      m_iLastInsertPos = m_iHead;

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
   while ((-1 != m_piNext[loc]) && (-1 != m_piData2[loc]))
   {
      __int32 i = m_piNext[loc];

      if (notgreaterthan(m_piData1[i], incSeqNo(m_piData2[loc])))
      {
         // coalesce if there is overlap
         if (-1 != m_piData2[i])
         {
            if (greaterthan(m_piData2[i], m_piData2[loc]))
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
      else
         break;
   }

   return m_iLength - origlen;
}

void CSndLossList::remove(const __int32& seqno)
{
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
         loc = m_piNext[m_iHead];
      else
      {
         m_piData1[loc] = incSeqNo(seqno);
         if (greaterthan(m_piData2[m_iHead], incSeqNo(seqno)))
            m_piData2[loc] = m_piData2[m_iHead];

         m_piData2[m_iHead] = -1;

         m_piNext[loc] = m_piNext[m_iHead];
      }

      m_piData1[m_iHead] = -1;

      if (m_iLastInsertPos == m_iHead)
         m_iLastInsertPos = -1;

      m_iHead = loc;

      m_iLength --;
   }
   else if (offset > 0)
   {
      __int32 h = m_iHead;

      if (seqno == m_piData1[loc])
      {
         // target node is not empty, remove part/all of the seqno in the node.
         __int32 temp = loc;
         loc = (loc + 1) % m_iSize;         

         if (-1 == m_piData2[temp])
            m_iHead = m_piNext[temp];
         else
         {
            // remove part, e.g., [3, 7] becomes [], [4, 7] after remove(3)
            m_piData1[loc] = incSeqNo(seqno);
            if (greaterthan(m_piData2[temp], incSeqNo(seqno)))
               m_piData2[loc] = m_piData2[temp];
            m_iHead = loc;
            m_piNext[loc] = m_piNext[temp];
            m_piNext[temp] = loc;
            m_piData2[temp] = -1;
         }
      }
      else
      {
         // targe node is empty, check prior node
         __int32 i = m_iHead;
         while ((-1 != m_piNext[i]) && lessthan(m_piData1[m_piNext[i]], seqno))
            i = m_piNext[i];

         loc = (loc + 1) % m_iSize;

         if (-1 == m_piData2[i])
            m_iHead = m_piNext[i];
         else if (greaterthan(m_piData2[i], seqno))
         {
            // remove part seqno in the prior node
            m_piData1[loc] = incSeqNo(seqno);
            if (greaterthan(m_piData2[i], incSeqNo(seqno)))
               m_piData2[loc] = m_piData2[i];

            m_piData2[i] = seqno;

            m_piNext[loc] = m_piNext[i];
            m_piNext[i] = loc;

            m_iHead = loc;
         }
         else
            m_iHead = m_piNext[i];
      }

      // Remove all nodes prior to the new head
      while (h != m_iHead)
      {
         if (m_piData2[h] != -1)
         {
            m_iLength -= getLength(m_piData1[h], m_piData2[h]);
            m_piData2[h] = -1;
         }
         else
            m_iLength --;

         m_piData1[h] = -1;

         if (m_iLastInsertPos == h)
            m_iLastInsertPos = -1;

         h = m_piNext[h];
      }
   }
}

__int32 CSndLossList::getLossLength()
{
   return m_iLength;
}

__int32 CSndLossList::getLostSeq()
{
   if (0 == m_iLength)
     return -1;

   if (m_iLastInsertPos == m_iHead)
      m_iLastInsertPos = -1;

   // return the first loss seq. no.
   __int32 seqno = m_piData1[m_iHead];

   // head moves to the next node
   if (-1 == m_piData2[m_iHead])
   {
      //[3, -1] becomes [], and head moves to next node in the list
      m_piData1[m_iHead] = -1;
      m_iHead = m_piNext[m_iHead];
   }
   else
   {
      // shift to next node, e.g., [3, 7] becomes [], [4, 7]
      __int32 loc = (m_iHead + 1) % m_iSize;

      m_piData1[loc] = incSeqNo(seqno);
      if (greaterthan(m_piData2[m_iHead], incSeqNo(seqno)))
         m_piData2[loc] = m_piData2[m_iHead];

      m_piData1[m_iHead] = -1;
      m_piData2[m_iHead] = -1;

      m_piNext[loc] = m_piNext[m_iHead];
      m_iHead = loc;
   }

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
   m_pLastFeedbackTime = new double [m_iSize];
   m_piCount = new __int32 [m_iSize];
   m_piNext = new __int32 [m_iSize];
   m_piPrior = new __int32 [m_iSize];

   // -1 means there is no data in the node
   for (__int32 i = 0; i < size; ++ i)
   {
      m_piData1[i] = -1;
      m_piData2[i] = -1;
   }

   m_iLength = 0;
   m_iHead = -1;
   m_iTail = -1;
}

CRcvLossList::~CRcvLossList()
{
   delete [] m_piData1;
   delete [] m_piData2;
   delete [] m_pLastFeedbackTime;
   delete [] m_piCount;
   delete [] m_piNext;
   delete [] m_piPrior;
}

void CRcvLossList::insert(const __int32& seqno1, const __int32& seqno2)
{
   // Data to be inserted must be larger than all those in the list
   // guaranteed by the UDT receiver

   if (0 == m_iLength)
   {
      // insert data into an empty list
      m_iHead = 0;
      m_iTail = 0;
      m_piData1[m_iHead] = seqno1;
      if (seqno2 != seqno1)
         m_piData2[m_iHead] = seqno2;

      m_pLastFeedbackTime[m_iHead] = Scheduler::instance().clock();
      m_piCount[m_iHead] = 2;

      m_piNext[m_iHead] = -1;
      m_piPrior[m_iHead] = -1;
      m_iLength += getLength(seqno1, seqno2);

      return;
   }

   // otherwise searching for the position where the node should be

   __int32 offset = seqno1 - m_piData1[m_iHead];

   if (offset < -m_iSeqNoTH)
      offset += m_iMaxSeqNo;

   __int32 loc = (m_iHead + offset) % m_iSize;

   if ((-1 != m_piData2[m_iTail]) && (incSeqNo(m_piData2[m_iTail]) == seqno1))
   {
      // coalesce with prior node, e.g., [2, 5], [6, 7] becomes [2, 7]
      loc = m_iTail;
      m_piData2[loc] = seqno2;
   }
   else
   {
      // create new node
      m_piData1[loc] = seqno1;

      if (seqno2 != seqno1)
         m_piData2[loc] = seqno2;

      m_piNext[m_iTail] = loc;
      m_piPrior[loc] = m_iTail;
      m_piNext[loc] = -1;
      m_iTail = loc;
   }

   // Initilize time stamp
   m_pLastFeedbackTime[loc] = Scheduler::instance().clock();
   m_piCount[loc] = 2;

   m_iLength += getLength(seqno1, seqno2);
}

bool CRcvLossList::remove(const __int32& seqno)
{
   if (0 == m_iLength)
      return false; 

   // locate the position of "seqno" in the list
   __int32 offset = seqno - m_piData1[m_iHead];

   if (offset < -m_iSeqNoTH)
      offset += m_iMaxSeqNo;

   if (offset < 0)
      return false;

   __int32 loc = (m_iHead + offset) % m_iSize;

   if (seqno == m_piData1[loc])
   {
      // This is a seq. no. that starts the loss sequence

      if (-1 == m_piData2[loc])
      {
         // there is only 1 loss in the sequence, delete it from the node
         if (m_iHead == loc)
         {
            m_iHead = m_piNext[m_iHead];
            if (-1 != m_iHead)
               m_piPrior[m_iHead] = -1;
         }
         else
         {
            m_piNext[m_piPrior[loc]] = m_piNext[loc];
            if (-1 != m_piNext[loc])
               m_piPrior[m_piNext[loc]] = m_piPrior[loc];
            else
               m_iTail = m_piPrior[loc];
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
         m_piPrior[i] = m_piPrior[loc];

         if (m_iHead == loc)
            m_iHead = i;
         else
            m_piNext[m_piPrior[i]] = i;

         if (m_iTail == loc)
            m_iTail = i;
         else
            m_piPrior[m_piNext[i]] = i;
      }

      m_iLength --;

      return true;
   }

   // There is no loss sequence in the current position
   // the "seqno" may be contained in a previous node

   // searching previous node
   __int32 i = (loc - 1 + m_iSize) % m_iSize;
   while (-1 == m_piData1[i])
      i = (i - 1 + m_iSize) % m_iSize;

   // not contained in this node, return
   if ((-1 == m_piData2[i]) || greaterthan(seqno, m_piData2[i]))
       return false;

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
      m_piPrior[loc] = i;

      if (m_iTail == i)
         m_iTail = loc;
      else
         m_piPrior[m_piNext[loc]] = loc;
   }

   m_iLength --;

   return true;
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

void CRcvLossList::getLossArray(__int32* array, __int32* len, const __int32& limit, const double& threshold)
{
   double currtime = Scheduler::instance().clock();

   __int32 i  = m_iHead;

   len = 0;

   while ((*len < limit - 1) && (-1 != i))
   {
      if (currtime - m_pLastFeedbackTime[i] > m_piCount[i] * threshold)
      {
         array[*len] = m_piData1[i];
         if (-1 != m_piData2[i])
         {
            // there are more than 1 loss in the sequence
            array[*len] |= 0x80000000;
            ++ *len;
            array[*len] = m_piData2[i];
         }

         ++ *len;

         // update the timestamp
         m_pLastFeedbackTime[i] = Scheduler::instance().clock();
         // update how many times this loss has been fed back, the "k" in UDT paper
         ++ m_piCount[i];
      }

      i = m_piNext[i];
   }
}





////////////////////////////////////////////////////////////////////////////
//
AckWindow::AckWindow():
size_(1024),
head_(0),
tail_(0)
{
   ack_seqno_ = new int[size_];
   ack_ = new int[size_];
   ts_ = new double[size_];

   ack_seqno_[0] = -1;
}

AckWindow::~AckWindow()
{
   delete [] ack_seqno_;
   delete [] ack_;
   delete [] ts_;
}

void AckWindow::store(const int& seq, const int& ack)
{
   head_ = (head_ + 1) % size_;
   ack_seqno_[head_] = seq;
   ack_[head_] = ack;
   *(ts_ + head_) = Scheduler::instance().clock();

   // overwrite the oldest ACK since it is not likely to be acknowledged
   if (head_ == tail_)
      tail_ = (tail_ + 1) % size_;
}

double AckWindow::acknowledge(const int& seq, int& ack)
{
   if (head_ >= tail_)
   {
      // Head has not exceeded the physical boundary of the window
      for (int i = tail_; i <= head_; i ++)
         // looking for indentical ACK Seq. No.
         if (seq == ack_seqno_[i])
         {
            // return the Data ACK it carried
            ack = ack_[i];

            // calculate RTT
            double rtt = Scheduler::instance().clock() - ts_[i];
            if (i == head_)
               tail_ = head_ = 0;
            else
               tail_ = (i + 1) % size_;

            return rtt;
         }

      // Bad input, the ACK node has been overwritten
      return -1;
   }

   // head has exceeded the physical window boundary, so it is behind to tail
   for (int i = tail_; i <= head_ + size_; i ++)
      // looking for indentical ACK seq. no.
      if (seq == ack_seqno_[i % size_])
      {
         // return Data ACK
         i %= size_;
         ack = ack_[i];

         // calculate RTT
         double currtime = Scheduler::instance().clock();
         double rtt = currtime - ts_[i];
         if (i == head_)
            tail_ = head_ = 0;
         else
            tail_ = (i + 1) % size_;

         return rtt;
      }

   // bad input, the ACK node has been overwritten
   return -1;
}

//
TimeWindow::TimeWindow():
size_(16)
{
   pkt_window_ = new double[size_];
   rtt_window_ = new double[size_];
   pct_window_ = new double[size_];
   pdt_window_ = new double[size_];
   probe_window_ = new double[size_];

   pkt_window_ptr_ = 0;
   rtt_window_ptr_ = 0;
   probe_window_ptr_ = 0;

   first_round_ = true;

   for (int i = 0; i < size_; ++ i)
   {
      pkt_window_[i] = 1.0;
      rtt_window_[i] = pct_window_[i] = pdt_window_[i] = 0.0;
      probe_window_[i] = 1000.0;
   }

   last_arr_time_ = Scheduler::instance().clock();
}

TimeWindow::~TimeWindow()
{
   delete [] pkt_window_;
   delete [] rtt_window_;
   delete [] pct_window_;
   delete [] pdt_window_;
}

int TimeWindow::getbandwidth() const
{
   double temp;
   for (int i = 0; i < ((size_ >> 1) + 1); ++ i)
      for (int j = i; j < size_; ++ j)
         if (probe_window_[i] > probe_window_[j])
         {
            temp = probe_window_[i];
            probe_window_[i] = probe_window_[j];
            probe_window_[j] = temp;
         }

   if (0 == probe_window_[size_ >> 1])
      return 0;

   return int(ceil(1.0 / probe_window_[size_ >> 1]));
}

int TimeWindow::getpktspeed() const
{
   if ((first_round_) && (pkt_window_ptr_ > 0))
   {
      if ((pkt_window_ptr_ > 1) && (pkt_window_[pkt_window_ptr_ - 1] < 2 * pkt_window_[pkt_window_ptr_ - 2]))
         return (int)ceil(1.0 / pkt_window_[pkt_window_ptr_ - 1]);

      return 0;
   }

   double temp;
   for (int i = 0; i < ((size_ >> 1) + 1); ++ i)
      for (int j = i; j < size_; ++ j)
         if (pkt_window_[i] > pkt_window_[j])
         {
            temp = pkt_window_[i];
            pkt_window_[i] = pkt_window_[j];
            pkt_window_[j] = temp;
         }

   double median = pkt_window_[size_ >> 1];
   int count = 0;
   double sum = 0.0;

   for (int i = 0; i < size_; ++ i)
      if ((pkt_window_[i] < (median * 2)) && (pkt_window_[i] > (median / 2)))
      {
         ++ count;
         sum += pkt_window_[i];
      }

   if (count > (size_ >> 1))
      return (int)ceil(1.0 / (sum / count));
   else
      return 0;
}

bool TimeWindow::getdelaytrend() const
{
   double pct = 0.0;
   double pdt = 0.0;

   for (int i = 0; i < size_; ++i)
      if (i != rtt_window_ptr_)
      {
         pct += pct_window_[i];
         pdt += pdt_window_[i];
      }

   pct /= size_ - 1.0;
   if (0.0 != pdt)
      pdt = (rtt_window_[(rtt_window_ptr_ - 1 + size_) % size_] - rtt_window_[rtt_window_ptr_]) / pdt;

   return ((pct > 0.66) && (pdt > 0.45)) || ((pct > 0.54) && (pdt > 0.55));
}

void TimeWindow::pktarrival()
{
   curr_arr_time_ = Scheduler::instance().clock();

   pkt_window_[pkt_window_ptr_] = curr_arr_time_ - last_arr_time_;

   pkt_window_ptr_ = (pkt_window_ptr_ + 1) % size_;
 
   if (0 == pkt_window_ptr_)
      first_round_ = false;

   last_arr_time_ = curr_arr_time_;
}

void TimeWindow::probe1arrival()
{
   probe_time_ = Scheduler::instance().clock();
}

void TimeWindow::probe2arrival()
{
   probe_window_[probe_window_ptr_] = Scheduler::instance().clock() - probe_time_;;

   probe_window_ptr_ = (probe_window_ptr_ + 1) % size_;

   last_arr_time_ = Scheduler::instance().clock();
}

void TimeWindow::ack2arrival(const double& rtt)
{
   rtt_window_[rtt_window_ptr_] = rtt;
   pct_window_[rtt_window_ptr_] = (rtt > rtt_window_[(rtt_window_ptr_ - 1 + size_) % size_]) ? 1 : 0;
   pdt_window_[rtt_window_ptr_] = fabs(rtt - rtt_window_[(rtt_window_ptr_ - 1 + size_) % size_]);

   rtt_window_ptr_ = (rtt_window_ptr_ + 1) % size_;
}
