#include "superglue.hpp"

#include "option/taskqueue_prio.hpp"

struct Options : public DefaultOptions<Options> {
    typedef TaskQueuePrio<Options> ReadyListType;
    typedef TaskQueuePrio<Options> WaitListType;
};

int main() {
  ThreadManager<Options> tm;
  return 0;
}
