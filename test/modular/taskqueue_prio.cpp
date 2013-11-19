#include "superglue.hpp"

#include "option/taskqueue_prio.hpp"

struct Options : public DefaultOptions<Options> {
    typedef TaskQueuePrioUnsafe<Options> TaskQueueUnsafeType;
};

int main() {
  ThreadManager<Options> tm;
  return 0;
}
