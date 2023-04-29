#include "kthreadpool.hpp"
#include <iostream>

#define OBJ_COUNT 256

typedef std::chrono::high_resolution_clock   TimeClock;
typedef TimeClock::time_point                TimePoint;

double Since(TimePoint time)
{
	return std::chrono::duration_cast<std::chrono::duration<double>>(TimeClock::now() - time).count();
}

#define START_TIMING(str)            \
std::cout << str << "\n";            \
TimePoint start = TimeClock::now();

#define END_TIMING()                                                \
double total = Since(start);                                        \
std::cout 															\
<< "Took " << total													\
<< " seconds to run " 												\
<< OBJ_COUNT 														\
<< " objects in " 													\
<< ThreadCount << (ThreadCount == 1 ? " thread.\n" : " threads.\n");

struct Object
{
	int Value = 0;

	double GetSleepDuration() const
	{
		return .01 + .05 * (double(Value) / double(OBJ_COUNT));
	}

	void LookBusy()
	{
		std::this_thread::sleep_for(std::chrono::duration<double>(GetSleepDuration()));
	};
};

const int ThreadCount = KThreadPool::GetCpuCoreCount();

std::vector<Object> Objects;
std::vector<Object*> ObjectPtrs;

void Test_ObjPtr()
{
	const auto iter = [](Object* obj) -> void
	{
		obj->LookBusy();
	};

	START_TIMING("ObjectPtr");
	KThreadPool::Iterate(iter, ThreadCount, ObjectPtrs);
	END_TIMING();
}

void Test_ObjPtrIndex()
{
	const auto iter = [](Object* obj, size_t index) -> void
	{
		obj->LookBusy();
	};

	START_TIMING("ObjectPtr Index");
	KThreadPool::Iterate(iter, ThreadCount, ObjectPtrs);
	END_TIMING();
}

void Test_ObjPtrIndexData()
{
	const auto iter = [](Object* obj, size_t index, Object* container) -> void
	{
		obj->LookBusy();
	};

	START_TIMING("ObjectPtr Index Data");
	KThreadPool::Iterate(iter, ThreadCount, ObjectPtrs);
	END_TIMING();
}

void Test_Obj()
{
	const auto iter = [](Object* obj) -> void
	{
		obj->LookBusy();
	};

	START_TIMING("Object");
	KThreadPool::Iterate(iter, ThreadCount, Objects);
	END_TIMING();
}

void Test_ObjIndex()
{
	const auto iter = [](Object* obj, size_t index) -> void
	{
		obj->LookBusy();
	};

	START_TIMING("Object Index");
	KThreadPool::Iterate(iter, ThreadCount, Objects);
	END_TIMING();
}

void Test_ObjIndexData()
{
	const auto iter = [](Object* obj, size_t index, Object* container) -> void
	{
		obj->LookBusy();
	};
	
	START_TIMING("Object Index Data");
	KThreadPool::Iterate(iter, ThreadCount, Objects);
	END_TIMING();
}

void TestWeighted_ObjPtr()
{
	const auto iter = [](Object* obj) -> void
	{
		obj->LookBusy();
	};
	const auto iterWeight = [](const Object* obj) -> double
	{
		return obj->GetSleepDuration();
	};

	START_TIMING("Weighted ObjectPtr");
	KThreadPool::IterateWeighted(iter, iterWeight, ThreadCount, ObjectPtrs);
	END_TIMING();
}

void TestWeighted_ObjPtrIndex()
{
	const auto iter = [](Object* obj, size_t index) -> void
	{
		obj->LookBusy();
	};
	const auto iterWeight = [](const Object* obj) -> double
	{
		return obj->GetSleepDuration();
	};

	START_TIMING("Weighted ObjectPtr Index");
	KThreadPool::IterateWeighted(iter, iterWeight, ThreadCount, ObjectPtrs);
	END_TIMING();
}

void TestWeighted_ObjPtrIndexData()
{
	const auto iter = [](Object* obj, size_t index, Object* container) -> void
	{
		obj->LookBusy();
	};
	const auto iterWeight = [](const Object* obj) -> double
	{
		return obj->GetSleepDuration();
	};

	START_TIMING("Weighted ObjectPtr Index Data");
	KThreadPool::IterateWeighted(iter, iterWeight, ThreadCount, ObjectPtrs);
	END_TIMING();
}

void TestWeighted_Obj()
{
	const auto iter = [](Object* obj) -> void
	{
		obj->LookBusy();
	};
	const auto iterWeight = [](const Object* obj) -> double
	{
		return obj->GetSleepDuration();
	};

	START_TIMING("Weighted Object");
	KThreadPool::IterateWeighted(iter, iterWeight, ThreadCount, Objects);
	END_TIMING();
}

void TestWeighted_ObjIndex()
{
	const auto iter = [](Object* obj, size_t index) -> void
	{
		obj->LookBusy();
	};
	const auto iterWeight = [](const Object* obj) -> double
	{
		return obj->GetSleepDuration();
	};

	START_TIMING("Weighted Object Index");
	KThreadPool::IterateWeighted(iter, iterWeight, ThreadCount, Objects);
	END_TIMING();
}

void TestWeighted_ObjIndexData()
{
	const auto iter = [](Object* obj, size_t index, Object* container) -> void
	{
		obj->LookBusy();
	};
	const auto iterWeight = [](const Object* obj) -> double
	{
		return obj->GetSleepDuration();
	};

	START_TIMING("Weighted Object Index Data");
	KThreadPool::IterateWeighted(iter, iterWeight, ThreadCount, Objects);
	END_TIMING();
}

int main()
{
	Objects.resize(OBJ_COUNT);
	ObjectPtrs.resize(OBJ_COUNT);

	for (int i = 0; i < OBJ_COUNT; i++)
	{
		Objects[i].Value = i;
		ObjectPtrs[i] = new Object;
		ObjectPtrs[i]->Value = i;
	}

	Test_ObjPtr();
	Test_ObjPtrIndex();
	Test_ObjPtrIndexData();
	Test_Obj();
	Test_ObjIndex();
	Test_ObjIndexData();
	TestWeighted_ObjPtr();
	TestWeighted_ObjPtrIndex();
	TestWeighted_ObjPtrIndexData();
	TestWeighted_Obj();
	TestWeighted_ObjIndex();
	TestWeighted_ObjIndexData();

	KThreadPool pool(ThreadCount);
	const auto job = [](double time) -> void
	{
		std::this_thread::sleep_for(std::chrono::duration<double>(time));
	};
	std::cout << "Adding functions...\n";
	for (int i = 0; i < ThreadCount; i++)
		pool.AddFunctionToPool(job, 1);
	pool.WaitForFinish();
	std::cout << "Finished\n";

	return 0;
}
