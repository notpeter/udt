#include <udt.h>

// This is an example on how to add customized CC to UDT.


// This is a UDP blast (constant rate)
class CUDPBlast: public CCC
{
public:
   CUDPBlast(double usPktSndPeriod)
   {
      m_dPktSndPeriod = usPktSndPeriod;
      m_dCWndSize = 80000.0;
   }
};


// This is a very simple version of Scalable TCP.
// We did not implement slow start in this example
class CScalableTCP: public CCC
{
public:
   virtual void init()
   {
      // initial value of sending rate and congestion window size
      m_dPktSndPeriod = 1.0;
      m_dCWndSize = 16.0;

      // acknowledge every data packets
      setACKInterval(1);
   }

   virtual void onACK(const __int32&)
   {
      if (m_dCWndSize <= 38.0)
         m_dCWndSize += 1.0/m_dCWndSize;
      else
         m_dCWndSize += 0.01 * m_dCWndSize;

      if (m_dCWndSize > m_iMaxCWndSize)
         m_dCWndSize = m_iMaxCWndSize;
   }

   virtual void onLoss(const __int32*, const __int32&)
   {
      if (m_dCWndSize <= 38.0)
         m_dCWndSize *= 0.5;
      else
         m_dCWndSize *= 0.875;

      if (m_dCWndSize < m_iMinCWndSize)
         m_dCWndSize = m_iMinCWndSize;
   }

private:
   static const __int32 m_iMinCWndSize = 16;
   static const __int32 m_iMaxCWndSize = 100000;
};

