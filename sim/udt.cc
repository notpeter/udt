//
// Author: Yunhong Gu, ygu@cs.uic.edu
//
// Description: 
//
// Assumption: This code does NOT process sequence number wrap, which will overflow after 2^31 packets.
//             But I assume that you won't run NS for that long time :)
//
// Last Update: 03/31/2004
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
max_flow_window_(25600)
{
   bind("mtu_", &mtu_);
   bind("max_flow_window_", &max_flow_window_);

   flow_window_size_ = 1;
   snd_interval_ = 0.000001;

   ack_interval_ = syn_interval_;
   nak_interval_ = syn_interval_;
   exp_interval_ = 1.0;
   
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

   rtt_ = syn_interval_;
   
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

   syn_timer_.resched(0);
   ack_timer_.resched(0);
   nak_timer_.resched(0);
   exp_timer_.resched(0);
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
            snd_loss_list_.removeall(snd_last_ack_);
         }
         else
            break;

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

         //bandwidth_ = bandwidth_ * 0.875 + udth->bandwidth() * 0.125;
         bandwidth_ = udth->bandwidth();

         //printf("window: %d %d %d %d\n", flow_window_size_, udth->lrecv(), udth->rtt(), bandwidth_);

         exp_timer_.resched(exp_interval_);

         ++ local_ack_;

         if (snd_interval_ > rtt_)
         {
            snd_interval_ = rtt_;
            snd_timer_.resched(0);
         }

         break;

      case 3:
         slow_start_ = false;

         last_dec_int_ = snd_interval_;

         if (udth->loss()[0] > last_dec_seq_)
         {
            freeze_ = true;
            snd_interval_ = snd_interval_ * 1.125;

            nak_count_ = 1;

            avg_nak_num_ = 1 + int(ceil(avg_nak_num_ * 0.875 + nak_count_ * 0.125));

            dec_random_ = int(rand() * double(avg_nak_num_) / (RAND_MAX + 1.0)) + 1;

            last_dec_seq_ = snd_curr_seqno_;
         }
         else if (0 == (++ nak_count_ % dec_random_))
         {
            snd_interval_ = snd_interval_ * 1.125;

            last_dec_seq_ = snd_curr_seqno_;
         }

 	 //printf("loss %f %d %d %d\n", snd_interval_, nak_count_, dec_random_, last_dec_seq_);

         local_loss_ += udth->losslen();

         for (int i = 0; i < udth->losslen(); i ++)
            if (udth->loss()[i] >= snd_last_ack_)
               snd_loss_list_.insert(udth->loss()[i]);

         exp_timer_.resched(exp_interval_);

         snd_timer_.resched(0);

         break;
     
      case 4:
         if (slow_start_)
            slow_start_ = false;

         last_dec_int_ = snd_interval_;

         snd_interval_ = snd_interval_ * 1.125;
         //printf("delay ------ %f %d \n", snd_interval_, flow_window_size_);

         last_dec_seq_ = snd_curr_seqno_;
         nak_count_ = -16;
         dec_count_ = 1;

         break;

      case 6:
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
      int loss[MAX_LOSS_LEN];
      int c = 0;

      for (int i = rcv_curr_seqno_ + 1; i < udth->seqno(); i ++)
      {
         rcv_loss_list_.insertattail(i);
         if (c < MAX_LOSS_LEN)
         {
            loss[c] = i;
            c ++;
         }
      }

      sendCtrl(3, c, loss);
//      printf("%d\n", c);
   }

   if (udth->seqno() > rcv_curr_seqno_)
   {
      rcv_curr_seqno_ = udth->seqno();
   }
   else
   {
      rcv_loss_list_.remove(udth->seqno());
   }

   Packet::free(pkt);
   return;
}

void UdtAgent::sendmsg(int nbytes, const char* /*flags*/)
{
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
      if (rcv_loss_list_.getlosslength() == 0)
         ack = rcv_curr_seqno_ + 1;
      else
         ack = rcv_loss_list_.getfirstlostseq();

      if (ack > rcv_last_ack_)
      {
         rcv_last_ack_ = ack;
      }
      else if (Scheduler::instance().clock() - rcv_last_ack_time_ < 2 * rtt_)
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
         p = allocpkt(32 + lparam);
         udth = hdr_udt::access(p);

         udth->flag() = 1;
         udth->type() = 3;
         udth->losslen() = lparam;
         memcpy(udth->loss(), rparam, MAX_LOSS_LEN);

         ch = hdr_cmn::access(p);
         ch->size() = 32 + lparam;
         Agent::send(p, 0);
      }
      else if (rcv_loss_list_.getlosslength() > 0)
      {
         int losslen;
         int loss[MAX_LOSS_LEN];
         rcv_loss_list_.getlossarray(loss, losslen, MAX_LOSS_LEN, rtt_);
 
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

   if (snd_loss_list_.getlosslength() > 0)
   {
      nextseqno = snd_loss_list_.getfirstlostseq();
      snd_loss_list_.remove(nextseqno);
   }
   else if (snd_curr_seqno_ - snd_last_ack_ < flow_window_size_)
   {
      nextseqno = ++ snd_curr_seqno_;
      if (0 == nextseqno % 16)
         probe = true;
   }
   else
   {
      if (freeze_)
      {
         snd_timer_.resched(syn_interval_ + snd_interval_);
         freeze_ = false;
      }
      else
         snd_timer_.resched(snd_interval_);

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
   if ((local_send_ > 0) && (!slow_start_))
   {

      if ((local_loss_ > 0) || (local_ack_ == 0))
      {
         local_loss_ = 0;

         syn_timer_.resched(syn_interval_);

         return; 
      }
      else if (!slow_start_)
      {
         double inc;

         if (bandwidth_ < 1.0 / snd_interval_)
            inc = 1.0/mtu_;
         else if ((snd_interval_ > last_dec_int_) && (bandwidth_ / 9.0 < (bandwidth_ - 1.0 / snd_interval_)))
         {
            inc = pow(10, ceil(log10(bandwidth_ / 9.0 * mtu_ * 8))) * 0.0000015 / mtu_;

            if (inc < 1.0/mtu_)
               inc = 1.0/mtu_;
         }
         else
         {
            inc = pow(10, ceil(log10((bandwidth_ - 1.0 / snd_interval_) * mtu_ * 8))) * 0.0000015 / mtu_;

            if (inc < 1.0/mtu_)
               inc = 1.0/mtu_;
         }

         snd_interval_ = (snd_interval_ * syn_interval_) / (snd_interval_ * inc + syn_interval_);

         //printf("inc ++ %f %f\n", snd_interval_, inc);
      }
   }
   else
      local_loss_ = 0;

   if (snd_interval_ < 0.000001)
      snd_interval_ = 0.000001;

   //printf("snd_interval_ %f\n", snd_interval_);

   local_ack_ = 0;

   syn_timer_.resched(syn_interval_);
}

void UdtAgent::timeOut()
{
   if (snd_curr_seqno_ >= snd_last_ack_)
      for (int i = snd_last_ack_; i <= snd_curr_seqno_; i ++)
         snd_loss_list_.insert(i);

   exp_interval_ = 2 * rtt_ + syn_interval_;

   exp_timer_.resched(exp_interval_);
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
LossList::LossList():
attr_(0),
next_(NULL)
{
   tail_ = this;
}

LossList::LossList(const int& seqno):
attr_(seqno),
next_(NULL)
{
   timestamp_ = Scheduler::instance().clock();
}

void LossList::insert(const int& seqno)
{
   LossList *p = new LossList(seqno);

   LossList *q = this;

   while ((NULL != q->next_) && (q->next_->attr_ < seqno))
      q = q->next_;

   if ((NULL != q->next_) && (seqno == q->next_->attr_))
   {
      delete p;
      return;
   }

   p->next_ = q->next_;
   q->next_ = p;

   attr_ ++;
}

void LossList::insertattail(const int& seqno)
{
   LossList *p = new LossList(seqno);

   tail_->next_ = p;
   tail_ = p;

   attr_ ++;
}

void LossList::remove(const int& seqno)
{
   LossList *p = this;

   while ((NULL != p->next_) && (p->next_->attr_ < seqno))
      p = p->next_;

   if ((NULL != p->next_) && (p->next_->attr_ == seqno))
   {
      LossList *q = p->next_;
      p->next_ = q->next_;
      if (tail_ == q)
         tail_ = p;

      delete q;

      attr_ --;
   }
}

void LossList::removeall(const int& seqno)
{
   LossList *p = this;
   LossList *q;

   while ((NULL != p) && (NULL != p->next_) && (p->next_->attr_ <= seqno))
   {
      q = p->next_;
      p->next_ = q->next_;
      if (tail_ == q)
         tail_ = p;
      delete q;

      attr_ --;

      p = p->next_;
   }
}

int LossList::getlosslength() const
{
   return attr_;
}

int LossList::getfirstlostseq() const
{
   return (NULL == next_) ? -1 : next_->attr_;
}

void LossList::getlossarray(int* array, int& len, const int& limit, const double& interval) const
{
   LossList *p = this->next_;

   double currtime = Scheduler::instance().clock();

   for (len = 0; (NULL != p) && (len < limit); p = p->next_)
   {
     if (currtime - p->timestamp_ > interval)
      {
         array[len] = p->attr_;
         p->timestamp_ = Scheduler::instance().clock();
         len ++;
      }
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
      probe_window_[i] = rtt_window_[i] = pct_window_[i] = pdt_window_[i] = 0.0;

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
            temp = pkt_window_[i];
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

   pct /= size_ - 1;
   if (0 != pdt)
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
