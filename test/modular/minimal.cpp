#include "superglue.hpp"

struct Options : public DefaultOptions<Options> {};

int main() {
  ThreadManager<Options> tm;
  return 0;
}

