#include "dds_callback.h"

EmitterBase::~EmitterBase() = default;

namespace {

class std_fun_event : public OpenDDS::DCPS::EventBase {
public:
  std_fun_event(std::function<void(void)> fn) : m_fn(fn) {}

  void handle_event() { m_fn(); }

private:
  std::function<void(void)> m_fn;
};

}

void EmitterBase::AddToThreadPool(std::function<void(void)> fn)
{
    auto dispatcher = m_dispatcher.lock();
    if (dispatcher) {
        dispatcher->dispatch(OpenDDS::DCPS::make_rch<std_fun_event>(fn));
    }
}

/**
 * @}
 */
