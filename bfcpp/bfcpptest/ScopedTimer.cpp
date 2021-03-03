#include "ScopedTimer.hpp"

namespace framework
{
  ScopedTimer::ScopedTimer()
  {
    restart();
  }


  long long ScopedTimer::stopLong()
  {
    return stop().count();
  }


  milliseconds ScopedTimer::stop()
  {
    return std::chrono::duration_cast<milliseconds> (steady_clock::now() - m_start);
  }


  void ScopedTimer::restart()
  {
    m_start = steady_clock::now();
  }
}