#include "Engine.h"
#include <android-base/logging.h>

bool Engine::load(const std::string& path) {
    LOG(INFO) << "Engine::load stub, path=" << path;
    mReady = true;
    return true;
}

void Engine::unload() {
    mReady = false;
}
