// Entry point unico da suite de testes. Os casos ficam organizados em arvores
// espelhadas por modulo, mas o main permanece na raiz de `test/`.
#include "gtest/gtest.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
