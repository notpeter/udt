//
// Author: Yunhong Gu, ygu@cs.uic.edu
//
// Descrition: the program is used to simulate UDT on NS-2
//
// Last Update: 05/21/03
// 


#ifndef __NS_UDT_H__
#define __NS_UDT_H__

#include "agent.h"
#include "packet.h"

const int MAX_LOSS_LEN = 300;


struct hdr_udt 
{
   int flag_;
   int seqno_;
   int type_;
   int losslen_;
   int ackseq_;
   int ack_;
   int recv_;
   int rtt_;
   int bandwidth_;
   int loss_[MAX_LOSS_LEN];

   static int off_udt_;
   inline static int& offset() { return off_udt_; }
   inline static hdr_udt* access(Packet* p) {return (hdr_udt*) p->access(off_udt_);}

   int& flag() {return flag_;}
   int& seqno() {return seqno_;}
   int& type() {return type_;}
   int& losslen() {return losslen_;}
   int& ackseq() {return ackseq_;}
   int& ack() {return ack_;}
   int& lrecv() {return recv_;}
   int& rtt() {return rtt_;}
   int& bandwidth() {return bandwidth_;};
   int* loss() {return loss_;}
};


class UdtAgent;

class SndTimer: public TimerHandler 
{
public:
   SndTimer(UdtAgent *a) : TimerHandler() { a_ = a; }

protected:
   virtual void expire(Event *e);
   UdtAgent *a_;
};

class SynTimer: public TimerHandler
{
public:
   SynTimer(UdtAgent *a) : TimerHandler() { a_ = a; }

protected:
   virtual void expire(Event *e);
   UdtAgent *a_;
};

class AckTimer: public TimerHandler
{
public:
   AckTimer(UdtAgent *a) : TimerHandler() { a_ = a; }

protected:
   virtual void expire(Event *e);
   UdtAgent *a_;
};

class NakTimer: public TimerHandler
{
public:
   NakTimer(UdtAgent *a) : TimerHandler() { a_ = a; }

protected:
   virtual void expire(Event *e);
   UdtAgent *a_;
};

class ExpTimer: public TimerHandler
{
public:
   ExpTimer(UdtAgent *a) : TimerHandler() { a_ = a; }

protected:
   virtual void expire(Event *e);
   UdtAgent *a_;
};


#define __int32 int

class CList
{
protected:
   const bool greaterthan(const __int32& seqno1, const __int32& seqno2) const;
   const bool lessthan(const __int32& seqno1, const __int32& seqno2) const;
   const bool notlessthan(const __int32& seqno1, const __int32& seqno2) const;
   const bool notgreaterthan(const __int32& seqno1, const __int32& seqno2) const;

   const __int32 getLength(const __int32& seqno1, const __int32& seqno2) const;

   const __int32 incSeqNo(const __int32& seqno) const;
   const __int32 decSeqNo(const __int32& seqno) const;

protected:
   __int32 m_iSeqNoTH;                  // threshold for comparing seq. no.
   __int32 m_iMaxSeqNo;                 // maximum permitted seq. no.
};

////////////////////////////////////////////////////////////////////////////////
class CSndLossList: public CList
{
public:
   CSndLossList(const __int32& size, const __int32& th, const __int32& max);
   ~CSndLossList();

   __int32 insert(const __int32& seqno1, const __int32& seqno2);
   void remove(const __int32& seqno);
   __int32 getLossLength();
   __int32 getLostSeq();

private:
   __int32* m_piData1;			// sequence number starts
   __int32* m_piData2;			// seqnence number ends
   __int32* m_piNext;			// next node in the list

   __int32 m_iHead;			// first node
   __int32 m_iLength;			// loss length
   __int32 m_iSize;			// size of the static array
   __int32 m_iLastInsertPos;		// position of last insert node
};


////////////////////////////////////////////////////////////////////////////////
class CRcvLossList: public CList
{
public:
   CRcvLossList(const __int32& size, const __int32& th, const __int32& max);
   ~CRcvLossList();

   void insert(const __int32& seqno1, const __int32& seqno2);
   bool remove(const __int32& seqno);
   __int32 getLossLength() const;
   __int32 getFirstLostSeq() const;
   void getLossArray(__int32* array, __int32* len, const __int32& limit, const double& interval);

private:
   __int32* m_piData1;			// sequence number starts
   __int32* m_piData2;			// sequence number ends
   double* m_pLastFeedbackTime;		// last feedback time of the node
   __int32* m_piCount;			// report counter
   __int32* m_piNext;			// next node in the list
   __int32* m_piPrior;			// prior node in the list;

   __int32 m_iHead;			// first node in the list
   __int32 m_iTail;			// last node in the list;
   __int32 m_iLength;			// loss length
   __int32 m_iSize;			// size of the static array
};


class AckWindow
{
public:
   AckWindow();
   ~AckWindow();

   void store(const int& seq, const int& ack);
   double acknowledge(const int& seq, int& ack);

private:
   int* ack_seqno_;
   int* ack_;
   double* ts_;

   const int size_;

   int head_;
   int tail_;
};


class TimeWindow
{
public:
   TimeWindow();
   ~TimeWindow();

   int getbandwidth() const;
   int getpktspeed() const;
   bool getdelaytrend() const;

   void pktarrival();
   void ack2arrival(const double& rtt);

   void probe1arrival();
   void probe2arrival();

private:
   const int size_;

   double* pkt_window_;
   int pkt_window_ptr_;

   double* rtt_window_;
   double* pct_window_;
   double* pdt_window_;
   int rtt_window_ptr_;

   double* probe_window_;
   int probe_window_ptr_;

   double last_arr_time_;
   double probe_time_;
   double curr_arr_time_;

   bool first_round_;
};

class UdtAgent: public Agent
{
friend SndTimer;
friend SynTimer;
friend AckTimer;
friend NakTimer;
friend ExpTimer;

public:
   UdtAgent();
   ~UdtAgent();

   int command(int argc, const char*const* argv);

   virtual void recv(Packet*, Handler*);
   virtual void sendmsg(int nbytes, const char *flags = 0);

protected:
   SndTimer snd_timer_;
   SynTimer syn_timer_;
   AckTimer ack_timer_;
   NakTimer nak_timer_;
   ExpTimer exp_timer_;

   double syn_interval_;
   double ack_interval_;
   double nak_interval_;
   double exp_interval_;

   int mtu_;

   int max_flow_window_;
   int flow_window_size_;
   
   CSndLossList* snd_loss_list_;
   CRcvLossList* rcv_loss_list_;

   double snd_interval_;

   int bandwidth_;

   int nak_count_;
   int dec_count_;
   volatile int snd_last_ack_;
   int local_send_;
   int local_loss_;
   int local_ack_;
   volatile int snd_curr_seqno_;
   int curr_max_seqno_;
   int dec_random_;
   int avg_nak_num_;

   double loss_rate_limit_;
   double loss_rate_;

   AckWindow ack_window_;
   TimeWindow time_window_;

   double rtt_;

   double rcv_interval_;
   int rcv_last_ack_;
   double rcv_last_ack_time_;
   int rcv_last_ack2_;
   int ack_seqno_;
   volatile int rcv_curr_seqno_;
   int local_recv_;
   int last_dec_seq_;
   double last_delay_time_;
   double last_dec_int_;

   bool slow_start_;

   bool freeze_;

   bool firstloss_;

protected:
   void rateControl();
   void flowControl();
   void sendCtrl(int pkttype, int lparam = 0, int* rparam = NULL);
   void sendData();
   void timeOut();
};


#endif
