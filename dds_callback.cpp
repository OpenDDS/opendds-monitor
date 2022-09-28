#include "dds_callback.h"
#include "ThreadPool.h"

EmitterBase::~EmitterBase() = default;

void EmitterBase::AddToThreadPool(std::function<void(void)> fn)
{
    auto spThreadPool = m_threadPool.lock();
    if (spThreadPool) {
        spThreadPool->enqueue(fn);
    }
}

/**
 * @}
 */
