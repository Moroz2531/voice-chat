#include <iostream>
#include <syncstream>

#include "mainserver.hpp"

int main() {
  server::MainServer sv;
  sv.run();
  std::osyncstream(std::cout) << "port: " << ntohs(sv.port()) << '\n';
  std::cout << "Для выхода нажмите любую кнопку\n";

  std::getchar();

  return 0;
}
