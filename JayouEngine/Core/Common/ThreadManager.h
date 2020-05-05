//
// ThreadManager.h
//

#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include "TypeDef.h"

namespace Utility
{
	namespace ThreadManager
	{
		class Thread
		{
		public:

			template<typename TLambda>
			void CreateThreadDetach(const TLambda& lambda)
			{
				m_threads.push_back(std::thread(lambda));
				m_threads.back().detach();
			}

		private:

			std::vector<std::thread> m_threads;
			
		};
	}
}