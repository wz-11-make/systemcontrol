#include "Worker.h"

#include <android-base/logging.h>
#include <sys/prctl.h>

Worker::~Worker() {
    stop();
}

bool Worker::start(const char* name) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mRunning) return true;
    mStopping = false;
    mRunning = true;
    mThread = std::thread([this, n = std::string(name)] {
        prctl(PR_SET_NAME, n.c_str(), 0, 0, 0);
        loop();
    });
    return true;
}

void Worker::stop() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning) return;
        mStopping = true;
        mCv.notify_all();
    }
    if (mThread.joinable()) mThread.join();
    mRunning = false;
}

bool Worker::post(Task task) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning || mStopping) return false;
        mQueue.push_back(std::move(task));
    }
    mCv.notify_one();
    return true;
}

int Worker::depth() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return static_cast<int>(mQueue.size());
}

void Worker::loop() {
    for (;;) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCv.wait(lock, [this] { return mStopping || !mQueue.empty(); });
            if (mStopping) return;
            task = std::move(mQueue.front());
            mQueue.pop_front();
        }
        // 任务自己负责吞掉所有异常路径(平台 -fno-exceptions,这里没有 try)
        task();
    }
}
