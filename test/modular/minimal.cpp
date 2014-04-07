#include "sg/superglue.hpp"

struct Options : public DefaultOptions<Options> {};

int main() {
  SuperGlue<Options> sg;
  return 0;
}

