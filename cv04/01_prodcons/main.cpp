#include <iostream>
#include <mutex>
#include <condition_variable>
#include <array>
#include <chrono>
#include <random>
#include <memory>

// Semafor - rozumne bezpecna implementace
class CSemaphore
{
	private:
		std::mutex mMutex;
		std::condition_variable mCondVar;
		size_t mCount;

	public:
		CSemaphore(size_t count = 0) : mCount(count) {
		}

		inline void notify(int count)
		{
			std::unique_lock<std::mutex> lock(mMutex);
			mCount += count;
			mCondVar.notify_one();
		}

		inline void wait(int count)
		{
			std::unique_lock<std::mutex> lock(mMutex);
			while (mCount < count)
				mCondVar.wait(lock);

			mCount -= count;
		}

		inline size_t get()
		{
			return mCount;
		}
};

constexpr size_t Max_Work_Items = 32;

struct TWork_Item
{
	int work;
};

class CWork_Queue
{
	private:
		std::array<TWork_Item, Max_Work_Items> mQueue;

		size_t mReadCursor = 0;
		size_t mWriteCursor = 0;

		// TODO: doplnit

		std::mutex mQueueMtx;

	public:
		CWork_Queue()
		{
			for (size_t i = 0; i < Max_Work_Items; i++)
				mQueue[i] = {};
		}

		void enqueue(TWork_Item&& workitem)
		{
			// TODO: doplnit

			// zamek fronty
			{
				std::lock_guard<std::mutex> lck(mQueueMtx);
				mQueue[mWriteCursor] = workitem;

				mWriteCursor = (mWriteCursor + 1) % Max_Work_Items;
			}

			// TODO: doplnit
		}

		TWork_Item dequeue()
		{
			TWork_Item item;

			// TODO: doplnit

			// zamek fronty
			{
				std::lock_guard<std::mutex> lck(mQueueMtx);
				item = mQueue[mReadCursor];

				mReadCursor = (mReadCursor + 1) % Max_Work_Items;
			}

			// TODO: doplnit

			return item;
		}
};

constexpr size_t Producers_Count = 2;
constexpr size_t Consumers_Count = 5;

void Producer(CWork_Queue& queue)
{
	static std::random_device rdev;
	std::default_random_engine reng(rdev());
	std::uniform_int_distribution<size_t> rdist(100, 2000);
	std::uniform_int_distribution<int> workdist(1, 100);

	while (true)
	{
		// spani = "simulujeme" praci
		std::this_thread::sleep_for(std::chrono::milliseconds(rdist(reng)));

		TWork_Item item;
		item.work = workdist(reng);

		std::cout << "Produced: " << item.work << std::endl;
		queue.enqueue(std::move(item));
	}
}

void Consumer(CWork_Queue& queue)
{
	static std::random_device rdev;
	std::default_random_engine reng(rdev());
	std::uniform_int_distribution<size_t> rdist(400, 3000);
	std::uniform_int_distribution<int> workdist(1, 100);

	while (true)
	{
		TWork_Item item = queue.dequeue();

		std::cout << "Consumed: " << item.work << std::endl;

		// spani = "simulujeme" zpracovani polozky
		std::this_thread::sleep_for(std::chrono::milliseconds(rdist(reng)));
	}
}

int main(int argc, char** argv)
{
	CWork_Queue queue;

	std::vector<std::unique_ptr<std::thread>> threads;

	for (size_t i = 0; i < Producers_Count; i++)
		threads.push_back(std::make_unique<std::thread>(&Producer, std::ref(queue)));

	for (size_t i = 0; i < Consumers_Count; i++)
		threads.push_back(std::make_unique<std::thread>(&Consumer, std::ref(queue)));

	for (auto& thr : threads)
	{
		if (thr->joinable())
			thr->join();
	}
	
	return 0;
}
