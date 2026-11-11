#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

// 单线程串行执行队列。
//
// 为什么是单线程:llama.cpp 的 context 不是线程安全的,而且推理本来就该串行。
// binder 线程只负责入队,立刻返回。
class Worker {
public:
    using Task = std::function<void()>;

    Worker() = default;
    ~Worker();

    bool start(const char* name);
    void stop();

    // 返回 false 表示队列已关闭(进程正在退出)
    bool post(Task task);

    int depth() const;

private:
    void loop();

    mutable std::mutex mMutex;
    std::condition_variable mCv;
    std::deque<Task> mQueue;
    std::thread mThread;
    bool mRunning = false;
    bool mStopping = false;
};
