#include "sg/superglue.hpp"

#include "sg/option/taskqueue_prio.hpp"

struct Options : public DefaultOptions<Options> {
    typedef TaskQueuePrio<Options> ReadyListType;
    typedef TaskQueuePrio<Options> WaitListType;
};

int main() {
  SuperGlue<Options> sg;
  return 0;
}
