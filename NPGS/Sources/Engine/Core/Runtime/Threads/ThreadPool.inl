#include "ThreadPool.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_THREAD_BEGIN

template <typename Func, typename... Args>
inline auto FThreadPool::Submit(Func&& Pred, Args&&... Params)
{
    using ReturnType = std::invoke_result_t<Func, Args...>;
    auto Task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Func>(Pred), std::forward<Args>(Params)...));
    std::future<ReturnType> Future = Task->get_future();
    {
        std::unique_lock<std::mutex> Mutex(_Mutex);
        _Tasks.emplace([Task]() -> void { (*Task)(); });
    }
    _Condition.notify_one();
    return Future;
}

NPGS_INLINE void FThreadPool::ChangeHyperThread()
{
    _kHyperThreadIndex = 1 - _kHyperThreadIndex;
}

NPGS_INLINE int FThreadPool::GetMaxThreadCount() const
{
    return _kMaxThreadCount;
}

template <typename DataType, typename ResultType>
inline void MakeChunks(int MaxThread, std::vector<DataType>& Data, std::vector<std::vector<DataType>>& DataLists,
                       std::vector<std::promise<std::vector<ResultType>>>& Promises,
                       std::vector<std::future<std::vector<ResultType>>>& ChunkFutures)
{
    for (std::size_t i = 0; i != Data.size(); ++i)
    {
        std::size_t ThreadId = i % MaxThread;
        DataLists[ThreadId].push_back(std::move(Data[i]));
    }

    Promises.resize(MaxThread);

    for (int i = 0; i != MaxThread; ++i)
    {
        ChunkFutures.push_back(Promises[i].get_future());
    }
}

_THREAD_END
_RUNTIME_END
_NPGS_END
