#include "IntervalTimer.hpp"


namespace binancews
{
	IntervalTimer::IntervalTimer() : m_running(false), m_period(std::chrono::milliseconds{ 100 })
	{

	}

	IntervalTimer::IntervalTimer(const std::chrono::milliseconds period) : m_period(period), m_running(false)
	{

	}


	IntervalTimer::IntervalTimer(const std::chrono::seconds period) : m_period(period), m_running(false)
	{

	}


	IntervalTimer::~IntervalTimer()
	{
		stop();
	}


	void IntervalTimer::start(std::function<void()> callback, const std::chrono::milliseconds period)
	{
		m_period = period;
		start(callback);
	}


	void IntervalTimer::start(std::function<void()> callback)
	{
		m_callback = callback;
		m_running = true;

		m_future = std::async(std::launch::async, [this]
		{
			using namespace std::chrono_literals;
			const std::chrono::milliseconds CheckPeriod = 100ms;

			auto time = std::chrono::steady_clock::now();
			auto triggerTime = time + m_period;

			while (m_running.load())
			{
				std::this_thread::sleep_for(std::min<std::chrono::milliseconds>(CheckPeriod, m_period));

				time = std::chrono::steady_clock::now();
				if (time >= triggerTime)
				{
					triggerTime += m_period;

					try
					{
						m_callback();
					}
					catch (...)
					{
						// ignore callers' issues
					}
				}
			}
		});
	}


	void IntervalTimer::stop()
	{
		m_running = false;

		if (m_future.valid())
		{
			m_future.wait();
		}
	}
}