#pragma once

#include <chrono>
#include <functional>
#include <future>


namespace bfcpp 
{
	class IntervalTimer
	{
	public:
		IntervalTimer();
		IntervalTimer(const std::chrono::milliseconds period);
		IntervalTimer(const std::chrono::seconds period);

		~IntervalTimer();

		void start(std::function<void()> callback);
		void start(std::function<void()> callback, const std::chrono::milliseconds period);

		void stop();

	private:
		std::chrono::milliseconds m_period;
		std::future<void> m_future;
		std::atomic_bool m_running;
		std::function<void()> m_callback;
	};
}