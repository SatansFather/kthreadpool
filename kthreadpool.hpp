#pragma once

#include <thread>
#include <tuple>
#include <chrono>
#include <vector>
#include <mutex>
#include <atomic>
#include <type_traits>

class KThreadPool
{
private:

	struct ThreadJobBase
	{
		virtual void Execute() = 0;
	};

	struct IterSection
	{
		size_t Start, End;
	};

	std::vector<std::thread> Threads;

	// number of currently running jobs in the pool
	std::atomic<size_t> ActiveJobCount { 0 };

	// whether or not the pool is pending destroy
	std::atomic<bool> bDestroyingPool { false };

	// functions waiting to be picked up by a thread
	std::vector<ThreadJobBase*> PendingJobs;

	// mutex to allow multiple threads to read from PendingJobs
	std::mutex QueueMutex;

public:

	// default number of threads to be used in a pool, leave at 0 to use the CPU core count
	// setting this to ( GetCpuCoreCount() - 1 ) can be useful to prevent the user's computer from locking up during long tasks
	static int DefaultThreadCount;

private:

	template <typename Functor, typename... TArgs>
	struct ThreadJob : public ThreadJobBase
	{
		Functor Function;
		std::tuple<TArgs...> Args;
		bool bDeleteOnFinish = true;

		ThreadJob(Functor func, TArgs... args) 
			: Function(func)
		{
			Args = std::make_tuple(args...);
		}

		ThreadJob(bool deleteOnFinish, Functor func, TArgs... args) 
			: bDeleteOnFinish(deleteOnFinish), Function(func)
		{		
			Args = std::make_tuple(args...);
		}

		virtual void Execute() override
		{
			std::apply(Function, Args);
			if (bDeleteOnFinish) delete this;
		}
	};

public:

	// count - number of threads in the pool, set to 0 to use DefaultThreadCount	
	// restTime - time threads wait between checking for new jobs to be posted,
	//    non-zero values can be useful if the pool is expected to be idle a lot 	
	KThreadPool(int count = 0, double restTime = 0);

	KThreadPool() = default;
	~KThreadPool();

private:

	void AddJobToPool(ThreadJobBase* job);

	// thread attempts to get the next job from the pool
	bool TakeNewJob();

public:

	// number of cores on the CPU
	static int GetCpuCoreCount();

	// add a function to the pool to be processed by the next available thread
	template <typename Functor, typename... TArgs>
	void AddFunctionToPool(Functor func, TArgs... args);

	bool IsPendingDestroy() { return bDestroyingPool; }
	size_t GetPendingJobCount();

	void WaitForFinish();

	template <typename Functor, typename T, typename... TArgs>
	static void Iterate(Functor func, int poolSize, std::vector<T>& data, TArgs&&... args);

	template <typename Functor, typename T, typename... TArgs>
	static void Iterate(Functor func, int poolSize, T* data, size_t elementCount, TArgs&&... args);

	template <typename Functor, typename WeightFunctor, typename T, typename... TArgs>
	static void IterateWeighted(Functor func, WeightFunctor weightFunc, int poolSize, std::vector<T>& data, TArgs&&... args);

	template <typename Functor, typename WeightFunctor, typename T, typename... TArgs>
	static void IterateWeighted(Functor func, WeightFunctor weightFunc, int poolSize, T* data, size_t elementCount, TArgs&&... args);

private: 

	template <typename Functor, typename T, typename... TArgs>
	void IterCallback(Functor func, T* data, size_t i, TArgs&&... args);

};

int KThreadPool::DefaultThreadCount = 0;

KThreadPool::KThreadPool(int count, double restTime)
{
	if (count == 0)
		count = DefaultThreadCount == 0 ? GetCpuCoreCount() : DefaultThreadCount;
	
	if (count <= 0) 
		count = 1;

	Threads.reserve(count);

	// keeps threads alive while waiting for a new job to consume
	const auto waitForJob = [](KThreadPool* pool, size_t threadIndex, double restTime) -> void
	{
		while (true)
		{
			bool tookNew = pool->TakeNewJob();

			if (!tookNew)
			{
				if (!pool->IsPendingDestroy())
				{
					if (restTime > 0)
					{
						std::this_thread::sleep_for(std::chrono::duration<double>(restTime));
					}		
				}
				else
				{
					return;
				}
			}
		}
	};

	for (int i = 0; i < count; i++)
	{
		Threads.push_back(std::thread(waitForJob, this, i, restTime));
	}
}

KThreadPool::~KThreadPool()
{
	bDestroyingPool = true; // allows threads to exit when they finish
	for (std::thread& t : Threads) 
		t.join();
}

int KThreadPool::GetCpuCoreCount()
{
	return (int)std::thread::hardware_concurrency();
}

template <typename Functor, typename... TArgs>
void KThreadPool::AddFunctionToPool(Functor func, TArgs... args)
{
	AddJobToPool(new ThreadJob(func, std::forward<TArgs>(args)...));
}

void KThreadPool::AddJobToPool(ThreadJobBase* job)
{
	std::lock_guard<std::mutex> lock(QueueMutex);
	PendingJobs.push_back(job);
}

bool KThreadPool::TakeNewJob()
{
	ThreadJobBase* job = nullptr;

	{
		std::lock_guard<std::mutex> lock(QueueMutex);
		if (PendingJobs.size() > 0)
		{
			// use last index so we're not constantly shifting the entire array
			// keep in mind, most recently queued will run first
			job = PendingJobs[PendingJobs.size() - 1];
			PendingJobs.erase(PendingJobs.begin() + (PendingJobs.size() - 1));
			ActiveJobCount++;
		}
	}

	if (job)
	{
		job->Execute();
		ActiveJobCount--;

		return true;
	}

	return false;
}

size_t KThreadPool::GetPendingJobCount()
{
	std::lock_guard<std::mutex> lock(QueueMutex);
	return PendingJobs.size();
}

void KThreadPool::WaitForFinish()
{
	while (GetPendingJobCount() > 0 || ActiveJobCount > 0) {}
}

template <typename Functor, typename T, typename... TArgs>
void KThreadPool::Iterate(Functor func, int poolSize, std::vector<T>& data, TArgs&&... args)
{
	Iterate(func, poolSize, data.data(), data.size(), std::forward<TArgs>(args)...);
}

template <typename Functor, typename T, typename... TArgs>
void KThreadPool::Iterate(Functor func, int poolSize, T* data, size_t elementCount, TArgs&&... args)
{
	const auto iterCallback = [](KThreadPool* pool, Functor func, T* data, size_t i, TArgs... args) -> void
	{
		pool->IterCallback(func, data, i, std::forward<TArgs>(args)...);
	};

	typedef ThreadJob<decltype(iterCallback), KThreadPool*, Functor, T*, size_t, TArgs...> Job;
	std::vector<Job> jobs;
	jobs.reserve(elementCount);

	// thread pool needs to exist in its own scope so that it runs its destructor while jobs vector is still valid
	//    because pool.PendingJobs holds pointers to the jobs vector in this function
	// keeping them in the same scope would be fine but then jobs must be declared first so it runs destructor last
	//    but im also not sure if the standard guarantees that destructor order
	{
		if (poolSize == 0) poolSize = GetCpuCoreCount();
		if (poolSize > elementCount) poolSize = (int)elementCount;
		KThreadPool pool(poolSize);

		for (size_t i = 0; i < elementCount; i++)
		{
			jobs.push_back(Job(false, iterCallback, &pool, func, data, i, std::forward<TArgs>(args)...));
		}

		{
			// add pending jobs to be picked up by threads
			std::lock_guard<std::mutex> lock(pool.QueueMutex);
			pool.PendingJobs.reserve(elementCount);
			for (Job& job : jobs)
				pool.PendingJobs.push_back(&job);
		}
	}
}

template <typename Functor, typename WeightFunctor, typename T, typename... TArgs>
void KThreadPool::IterateWeighted(Functor func, WeightFunctor weightFunc, int poolSize, std::vector<T>& data, TArgs&&... args)
{
	IterateWeighted(func, weightFunc, poolSize, data.data(), data.size(), std::forward<TArgs>(args)...);
}

template <typename Functor, typename WeightFunctor, typename T, typename... TArgs>
void KThreadPool::IterateWeighted(Functor func, WeightFunctor weightFunc, int poolSize, T* data, size_t elementCount, TArgs&&... args)
{
	typedef typename std::remove_pointer<T>::type type;
	static_assert(std::is_invocable<WeightFunctor, type*>::value, "weight function must take a pointer to T as its only argument");
	static_assert(std::is_same<double, decltype(weightFunc(std::declval<const type*>()))>::value, 
		"weight function take a const pointer to object type and must return double");

	// find total weight
	double total = 0;
	for (size_t i = 0; i < elementCount; i++)
	{
		if constexpr (std::is_pointer<T>::value)
			total += weightFunc(data[i]);
		else
			total += weightFunc(&data[i]);
	}

	if (poolSize == 0) poolSize = GetCpuCoreCount();
	if (poolSize > elementCount) poolSize = (int)elementCount;
	KThreadPool pool(poolSize);

	// get target weight for each thread to have an even workload
	const double targetWeight = total / pool.Threads.size();
	std::vector<IterSection> sections(pool.Threads.size());

	double accumWeight = 0;
	size_t sectionIndex = 0;
	size_t newStart = 0;
	for (size_t i = 0; i < elementCount; i++)
	{
		double objWeight = 0;

		if constexpr (std::is_pointer<T>::value)
			objWeight = weightFunc(data[i]);
		else
			objWeight = weightFunc(&data[i]);

		if (i < elementCount - 1 && accumWeight + objWeight <= targetWeight)
		{
			// has not exceeded target
			accumWeight += objWeight;
		}
		else
		{
			// would put us over target, add and reset
			sections[sectionIndex].Start = newStart;
			sections[sectionIndex].End = i;

			accumWeight = 0;
			newStart = i + 1;
			sectionIndex++;
		}
	}

	const auto iterSection = [](KThreadPool* pool, Functor func, T* data, size_t start, size_t end, TArgs&&... args) -> void
	{
		for (size_t i = start; i < end; i++)
		{
			pool->IterCallback(func, data, i, std::forward<TArgs>(args)...);
		}
	};

	for (const IterSection& section : sections)
	{
		pool.AddFunctionToPool(iterSection, &pool, func, data, section.Start, section.End, std::forward<TArgs>(args)...);
	}
}

template <typename Functor, typename T, typename... TArgs>
void KThreadPool::IterCallback(Functor func, T* data, size_t i, TArgs&&... args)
{
	if constexpr (std::is_pointer<T>::value)
	{
		if constexpr (std::is_invocable<Functor, T, size_t, T, TArgs...>::value)
			func(data[i], i, *data, std::forward<TArgs>(args)...);
		else if constexpr (std::is_invocable<Functor, T, size_t, TArgs...>::value)
			func(data[i], i, std::forward<TArgs>(args)...);
		else
			func(data[i], std::forward<TArgs>(args)...);
	}
	else
	{
		if constexpr (std::is_invocable<Functor, T*, size_t, T*, TArgs...>::value)
			func(&data[i], i, data, std::forward<TArgs>(args)...);
		else if constexpr (std::is_invocable<Functor, T*, size_t, TArgs...>::value)
			func(&data[i], i, std::forward<TArgs>(args)...);
		else
			func(&data[i], std::forward<TArgs>(args)...);
	}
}
