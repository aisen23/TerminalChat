#pragma once
#include "CommonApi.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace tc
{
	template<class T>
	class Queue
	{
	public:
		Queue() = default;
		~Queue() { shutdown(); }

	public:
		void pushBack(T item)
		{
			{
				std::scoped_lock lock(m_mtx);
				if (m_stop)
					return;

				m_queue.emplace_back(std::move(item));
			}
			m_cv.notify_one();
		}

		template<class... Args>
		void emplaceBack(Args&&... args)
		{
			{
				std::scoped_lock lock(m_mtx);
				if (m_stop)
					return;

				m_queue.emplace_back(std::forward<Args>(args)...);
			}
			m_cv.notify_one();
		}

		void pushFront(T item)
		{
			{
				std::scoped_lock lock(m_mtx);
				if (m_stop)
					return;

				m_queue.emplace_front(std::move(item));
			}
			m_cv.notify_one();
		}

		template<class... Args>
		void emplaceFront(Args&&... args)
		{
			{
				std::scoped_lock lock(m_mtx);
				if (m_stop)
					return;
				
				m_queue.emplace_front(std::forward<Args>(args)...);
			}
			m_cv.notify_one();
		}

		bool empty()
		{
			std::scoped_lock lock(m_mtx);
			return m_queue.empty();
		}

		size_t count()
		{
			std::scoped_lock lock(m_mtx);
			return m_queue.size();
		}

		void clear()
		{
			std::scoped_lock lock(m_mtx);
			m_queue.clear();
		}

		std::optional<T> popFront()
		{
			std::scoped_lock lock(m_mtx);
			if (m_queue.empty())
				return std::nullopt;

			T t = std::move(m_queue.front());
			m_queue.pop_front();
			return t;
		}

		void wait()
		{
			std::unique_lock lock(m_mtx);
			m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
		}

		void shutdown()
		{
			{
				std::scoped_lock lock(m_mtx);
				m_stop = true;
			}
			m_cv.notify_all();
		}

		void restart()
		{
			std::scoped_lock lock(m_mtx);
			m_stop = false;
		}

		bool stopped()
		{
			std::scoped_lock lock(m_mtx);
			return m_stop;
		}

	private:
		Queue(const Queue&) = delete;
		Queue& operator=(const Queue&) = delete;

	protected:
		bool m_stop{ false };
		std::deque<T> m_queue;
		std::mutex m_mtx;
		std::condition_variable m_cv;
	};
}
