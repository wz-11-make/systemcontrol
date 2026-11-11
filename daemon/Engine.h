#pragma once
#include <string>

// 纯 C++,完全不知道 JNI / binder 的存在。
// 以后要换宿主(Java Service、测试程序)时,这个文件一行都不用改。
class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    bool load(const std::string& path);
    void unload();
    bool ready() const { return mReady; }

private:
    bool mReady = false;
};
