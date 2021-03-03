#ifndef __SCOPED_TIMER_H_
#define __SCOPED_TIMER_H_

#include <chrono>

///
/// Created by subatomicdev: https://github.com/subatomicdev
/// Use all you want.
namespace framework
{
    using std::chrono::steady_clock;
    using std::chrono::duration;
    using std::chrono::milliseconds;

    /// Provides a timer, in milliseconds. 
    /// 
    /// The timer is started on object construction and ended by calling stop();
    ///
    /// The time, in milliseconds, is returned by stop().
    ///
    /// NOTE: only call stop() once, but elapsed() can be called multiple times.
    ///
    /// Usage:  
    ///    ScopedTimer timer;   // this starts the timer
    ///     .......
    ///    cout << "Time taken: " << timer.stop();
    class ScopedTimer
    {
    public:
        /// Starts the timer.
        ScopedTimer();
        ~ScopedTimer() = default;


        /// Stops the timer and returns the time. Only call once until next call to restart().
        long long stopLong();

        /// Stops the timer and returns the time. Only call once until next call to restart().
        milliseconds stop();

        void restart();

    private:
        ScopedTimer& operator= (const ScopedTimer&) = delete;
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer(const ScopedTimer&&) = delete;

    private:
        steady_clock::time_point m_start;
    };
}


#endif
