#include "Engine.h"

#include "MockEngine.h"
#ifdef AGENT_WITH_LLAMA
#include "LlamaEngine.h"
#endif

std::unique_ptr<Engine> Engine::create() {
#ifdef AGENT_WITH_LLAMA
    return std::unique_ptr<Engine>(new LlamaEngine());
#else
    return std::unique_ptr<Engine>(new MockEngine());
#endif
}
