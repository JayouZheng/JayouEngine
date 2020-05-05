//
// TimerManager.h
//

#pragma once

#include <chrono>
#include "TypeDef.h"

using namespace std::chrono;
using hclock = high_resolution_clock;
using htime_point = hclock::time_point;

namespace Utility
{
	namespace TimerManager
	{
		class PerformanceCounter
		{
		public:

			void BeginCounter(const std::string& name)
			{
				if (m_counterResults.find(name) != m_counterResults.end())
					m_counterResults.erase(name);
				htime_point beginPoint = hclock::now();
				m_counterBeginPoint[name] = beginPoint;
			}

			void EndCounter(const std::string& name)
			{
				if (m_counterResults.find(name) != m_counterResults.end())
					return;
				htime_point endPoint = hclock::now();
				m_counterResults[name] = duration_cast<duration<double>>(endPoint - m_counterBeginPoint[name]).count();
			}

			double GetCounterResult(const std::string& name)
			{
				if (m_counterResults.find(name) != m_counterResults.end())
					return m_counterResults[name];
				else return 0.0f;
			}

		private:

			std::unordered_map<std::string, htime_point> m_counterBeginPoint;
			std::unordered_map<std::string, double> m_counterResults;
		};

		// Helper class for animation and simulation timing.
		class BaseTimer
		{
		public:
			BaseTimer() noexcept(false) :
				m_deltaTime(0),
				m_totalTime(0),
				m_leftOverTime(0),
				m_frameCount(0),
				m_framesPerSecond(0),
				m_framesThisSecond(0),
				m_oneSecond(0),
				m_isFixedTimeStep(false),
				m_targetDelta(1.0f / 60) // 60 frames per second.
			{
				m_lastTime = hclock::now();

				// Initialize max delta to 1/10 of a second (10 frames per second).
				m_maxDelta = 0.1f;
			}

			// Get delta time since the previous Update call.
			double GetDeltaSeconds() const { return m_deltaTime; }

			// Get total time since the start of the program.
			double GetTotalSeconds() const { return m_totalTime; }

			// Get total number of updates since start of the program.
			uint32 GetFrameCount() const { return m_frameCount; }

			// Get the current framerate.
			uint32 GetFramesPerSecond() const { return m_framesPerSecond; }

			// Set whether to use fixed or variable timestep mode.
			void SetFixedTimeStep(bool isFixedTimestep) { m_isFixedTimeStep = isFixedTimestep; }

			// Set how often to call Update when in fixed timestep mode.
			void SetTargetDeltaSeconds(double targetDelta) { m_targetDelta = targetDelta; }

			// After an intentional timing discontinuity (for instance a blocking IO operation)
			// call this to avoid having the fixed timestep logic attempt a set of catch-up 
			// Update calls.

			void Reset()
			{
				m_lastTime = hclock::now();

				m_oneSecond = 0.0f;
				m_leftOverTime = 0.0f;
				m_framesPerSecond = 0;
				m_framesThisSecond = 0;
			}

			// Update timer state, calling the specified Update function the appropriate number of times.
			template<typename TUpdate>
			void Tick(const TUpdate& update)
			{
				// Query the current time.
				htime_point currentTime = hclock::now();

				double deltaTime = duration_cast<duration<double>>(currentTime - m_lastTime).count();

				m_lastTime = currentTime;
				m_oneSecond += deltaTime;

				// Clamp excessively large time deltas (e.g. after paused in the debugger).
				if (deltaTime > m_maxDelta)
				{
					deltaTime = m_maxDelta;
				}

				uint32 lastFrameCount = m_frameCount;

				if (m_isFixedTimeStep)
				{
					// Fixed timestep update logic

					// If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
					// the clock to exactly match the target value. This prevents tiny and irrelevant errors
					// from accumulating over time. Without this clamping, a game that requested a 60 fps
					// fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
					// accumulate enough tiny errors that it would drop a frame. It is better to just round 
					// small deviations down to zero to leave things running smoothly.
					// For example, 49 +49 -50 = 48, 48 +49 -50 = 47, 47 +49 -50 = 46...drop first frame.

					if (std::abs(deltaTime - m_targetDelta) < 0.000025f)
					{
						deltaTime = m_targetDelta; // Clamp to Target ElapsedT icks.
					}

					m_leftOverTime += deltaTime;

					while (m_leftOverTime >= m_targetDelta)
					{
						m_deltaTime = m_targetDelta;
						m_totalTime += m_targetDelta;
						m_leftOverTime -= m_targetDelta;
						m_frameCount++;

						update();
					}
				}
				else
				{
					// Variable timestep update logic.
					m_deltaTime = deltaTime;
					m_totalTime += deltaTime;
					m_leftOverTime = 0;
					m_frameCount++;

					update();
				}

				// Track the current framerate.
				if (m_frameCount != lastFrameCount)
				{
					m_framesThisSecond++;
				}

				if (m_oneSecond >= 1.0f)
				{
					m_framesPerSecond = m_framesThisSecond;
					m_framesThisSecond = 0;
					m_oneSecond = std::fmod(m_oneSecond, 1.0f);
				}
			}

		private:
			// Source timing data in second.
			htime_point m_lastTime;
			double m_maxDelta;
			double m_deltaTime;
			double m_totalTime;
			double m_leftOverTime;

			// Members for tracking the framerate.
			uint32 m_frameCount;
			uint32 m_framesPerSecond;
			uint32 m_framesThisSecond;
			double m_oneSecond;

			// Members for configuring fixed timestep mode.
			bool   m_isFixedTimeStep;
			double m_targetDelta;
		};

#ifdef _WINDOWS

		// Helper class for animation and simulation timing.
		class StepTimer
		{
		public:
			StepTimer() noexcept(false) :
				m_elapsedTicks(0),
				m_totalTicks(0),
				m_leftOverTicks(0),
				m_frameCount(0),
				m_framesPerSecond(0),
				m_framesThisSecond(0),
				m_qpcSecondCounter(0),
				m_isFixedTimeStep(false),
				m_targetElapsedTicks(TicksPerSecond / 60)
			{
				if (!QueryPerformanceFrequency(&m_qpcFrequency)) // counts per second.
				{
					throw std::exception("QueryPerformanceFrequency");
				}

				if (!QueryPerformanceCounter(&m_qpcLastTime))
				{
					throw std::exception("QueryPerformanceCounter");
				}

				// Initialize max delta to 1/10 of a second (10 frames per second).
				m_qpcMaxDelta = static_cast<uint64_t>(m_qpcFrequency.QuadPart / 10);
			}

			// Get elapsed time since the previous Update call.
			uint64 GetDeltaTicks() const { return m_elapsedTicks; }
			double GetDeltaSeconds() const { return TicksToSeconds(m_elapsedTicks); }

			// Get total time since the start of the program.
			uint64 GetTotalTicks() const { return m_totalTicks; }
			double GetTotalSeconds() const { return TicksToSeconds(m_totalTicks); }

			// Get total number of updates since start of the program.
			uint32 GetFrameCount() const { return m_frameCount; }

			// Get the current framerate.
			uint32 GetFramesPerSecond() const { return m_framesPerSecond; }

			// Set whether to use fixed or variable timestep mode.
			void SetFixedTimeStep(bool isFixedTimestep) { m_isFixedTimeStep = isFixedTimestep; }

			// Set how often to call Update when in fixed timestep mode.
			void SetTargetDeltaTicks(uint64 targetElapsed) { m_targetElapsedTicks = targetElapsed; }
			void SetTargetDeltaSeconds(double targetElapsed) { m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

			// Integer format represents time using 10,000,000 ticks per second.
			static const uint64 TicksPerSecond = 10000000;

			static double TicksToSeconds(uint64 ticks) { return static_cast<double>(ticks) / TicksPerSecond; }
			static uint64 SecondsToTicks(double seconds) { return static_cast<uint64_t>(seconds * TicksPerSecond); }

			// After an intentional timing discontinuity (for instance a blocking IO operation)
			// call this to avoid having the fixed timestep logic attempt a set of catch-up 
			// Update calls.

			void Reset()
			{
				if (!QueryPerformanceCounter(&m_qpcLastTime))
				{
					throw std::exception("QueryPerformanceCounter");
				}

				m_leftOverTicks = 0;
				m_framesPerSecond = 0;
				m_framesThisSecond = 0;
				m_qpcSecondCounter = 0;
			}

			// Update timer state, calling the specified Update function the appropriate number of times.
			template<typename TUpdate>
			void Tick(const TUpdate& update)
			{
				// Query the current time.
				LARGE_INTEGER currentTime;

				if (!QueryPerformanceCounter(&currentTime))
				{
					throw std::exception("QueryPerformanceCounter");
				}

				uint64 timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

				m_qpcLastTime = currentTime;
				m_qpcSecondCounter += timeDelta;

				// Clamp excessively large time deltas (e.g. after paused in the debugger).
				if (timeDelta > m_qpcMaxDelta)
				{
					timeDelta = m_qpcMaxDelta;
				}

				// Convert QPC units into a canonical tick format. This cannot overflow due to the previous clamp.
				timeDelta *= TicksPerSecond;			// counts * ticks(/per second).
				timeDelta /= m_qpcFrequency.QuadPart;	// counts * ticks(/per second) / counts(/per second).

				uint32 lastFrameCount = m_frameCount;

				if (m_isFixedTimeStep)
				{
					// Fixed timestep update logic

					// If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
					// the clock to exactly match the target value. This prevents tiny and irrelevant errors
					// from accumulating over time. Without this clamping, a game that requested a 60 fps
					// fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
					// accumulate enough tiny errors that it would drop a frame. It is better to just round 
					// small deviations down to zero to leave things running smoothly.
					// For example, 49 +49 -50 = 48, 48 +49 -50 = 47, 47 +49 -50 = 46...drop first frame.

					if (static_cast<uint64_t>(std::abs(static_cast<int64_t>(timeDelta - m_targetElapsedTicks))) < TicksPerSecond / 4000)
					{
						timeDelta = m_targetElapsedTicks; // Clamp to Target ElapsedT icks.
					}

					m_leftOverTicks += timeDelta;

					while (m_leftOverTicks >= m_targetElapsedTicks)
					{
						m_elapsedTicks = m_targetElapsedTicks;
						m_totalTicks += m_targetElapsedTicks;
						m_leftOverTicks -= m_targetElapsedTicks;
						m_frameCount++;

						update();
					}
				}
				else
				{
					// Variable timestep update logic.
					m_elapsedTicks = timeDelta;
					m_totalTicks += timeDelta;
					m_leftOverTicks = 0;
					m_frameCount++;

					update();
				}

				// Track the current framerate.
				if (m_frameCount != lastFrameCount)
				{
					m_framesThisSecond++;
				}

				if (m_qpcSecondCounter >= static_cast<uint64_t>(m_qpcFrequency.QuadPart))
				{
					m_framesPerSecond = m_framesThisSecond;
					m_framesThisSecond = 0;
					m_qpcSecondCounter %= m_qpcFrequency.QuadPart;
				}
			}

		private:
			// Source timing data uses QPC units.
			LARGE_INTEGER m_qpcFrequency;
			LARGE_INTEGER m_qpcLastTime;
			uint64 m_qpcMaxDelta;

			// Derived timing data uses a canonical tick format.
			uint64 m_elapsedTicks;
			uint64 m_totalTicks;
			uint64 m_leftOverTicks;

			// Members for tracking the framerate.
			uint32 m_frameCount;
			uint32 m_framesPerSecond;
			uint32 m_framesThisSecond;
			uint64 m_qpcSecondCounter;

			// Members for configuring fixed timestep mode.
			bool m_isFixedTimeStep;
			uint64 m_targetElapsedTicks;
		};

		class GameTimer
		{
		public:
			GameTimer();

			float TotalTime()const; // in seconds
			float DeltaTime()const; // in seconds

			void Reset(); // Call before message loop.
			void Start(); // Call when unpaused.
			void Stop();  // Call when paused.
			void Tick();  // Call every frame.

		private:
			double mSecondsPerCount;
			double mDeltaTime;

			__int64 mBaseTime;
			__int64 mPausedTime;
			__int64 mStopTime;
			__int64 mPrevTime;
			__int64 mCurrTime;

			bool mStopped;
		};

#endif // _WINDOWS
	}
}
