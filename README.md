# kthreadpool
kthreadpool is a single header thread pool class that allows easy iteration over arrays and vectors. C++20 is required and has been tested on Linux with GCC and Clang, and Windows with MSVC. Just drop kthreadpool.hpp anywhere in your project and include it.

## Usage

kthreadpool offers several ways to use it, with the simplest being the `Iterate` method:
```cpp
std::vector<T> objects;
const auto iterFunc = [](T* obj) -> void
{
    obj->DoSomething();
};
// vector overload
KThreadPool::Iterate(iterFunc, threadCount, objects);
// array overload
KThreadPool::Iterate(iterFunc, threadCount, objects.data, objects.size());
```
The function passed to `Iterate` must return `void` and take `T*` as its first argument. The container can hold `T` or `T*`. The `threadCount` argument can be `0`, falling back to `KThreadPool::DefaultThreadCount` which defaults to the number of cores available to the machine, which can be retrieved using `KThreadPool::GetCpuCoreCount()`. 

The function signatures that the `Iterate` function will accept are as follows:
```cpp
void iterFunc(T* obj);
void iterFunc(T* obj, size_t index); // the index of the object within the container
void iterFunc(T* obj, size_t index, T* container); // pointer to the start of the container
```
With all of these signatures, any number of arguments can be appended to the end:
```cpp
void iterFunc(T* obj, int add, float extra, bool args);
// ...
KThreadPool::Iterate(iterFunc, threadCount, objects, add, extra, args);
```
For the best performance, you should sort your data so that the objects with the largest expected processing time are at the *end* of the array. When calling the `Iterate` function, the last objects will be iterated first. This reduces the likelyhood that one thread will be stuck running a large computation after all other threads have finished.

If sorting is undesirable, a weighted iteration function is also available:
```cpp
// ...
const auto iterWeight = [](const T* obj) -> double
{
    return obj->width * obj->height;
};
KThreadPool::IterateWeighted(iterFunc, iterWeight, threadCount, objects);
```
The weight function must take a `const T*` and return `double`. An example usage is an array of 2D textures that all need to have work done with each texel. Larger textures will take longer, thus `IterateWeighted` will predetermine start and end indices into the array for each thread, and will attempt to give each thread an equal amount of work.

It is also possible to directly add functions to an existing pool instead of using the iterate functions:
```cpp
const auto job = [](int value) -> void
{
    DoSomething(value);
};
KThreadPool pool(threadCount);
pool.AddFunctionToPool(job, 5);
pool.WaitForFinish(); // blocks calling thread until all jobs are finsihed
std::cout << "Finished";
```

If a pool has active or pending jobs, it will constantly check if a new one was added. This may not be preferred in cases where the pool is expected to be idle for extended durations. A rest period between checks can be declared like this:
```cpp
KThreadPool(threadCount, .01);
```
In this example, the each thread will wait `.01` seconds between checks for new jobs whenever they become idle. This rest period is ignored when the last check resulted in taking a new job.

## License

Do whatever you want.
