#include <iostream>
#include <memory>
#include <vector>

#include "base/hash.h"

namespace operations_research {
#if defined(__linux__) || defined(_MSC_VER) || defined(__APPLE__)
struct Foo {
  Foo() { std::cout << "Foo::Foo\n"; }
  ~Foo() { std::cout << "Foo::~Foo\n"; }
  void bar() { std::cout << "Foo::bar\n"; }
};

void f(const Foo &foo) {
  std::cout << "f(const Foo&)\n";
}

void test_unique() {
  std::cout << "test_unique" << std::endl;
  std::unique_ptr<Foo> p1(new Foo);  // p1 owns Foo
  if (p1) p1->bar();

  {
    std::unique_ptr<Foo> p2(std::move(p1));  // now p2 owns Foo
    f(*p2);

    p1 = std::move(p2);  // ownership returns to p1
    std::cout << "destroying p2...\n";
  }

  if (p1) p1->bar();
  // Foo instance is destroyed when p1 goes out of scope
}
#else
void test_unique() {
  std::cout << "test_unique not launched" << std::endl;
}
#endif

void test_auto() {
  std::cout << "test_auto" << std::endl;
  std::vector<int> numbers;
  numbers.push_back(1);
  numbers.push_back(2);
  numbers.push_back(3);
  numbers.push_back(4);
  numbers.push_back(5);
  numbers.push_back(6);
  numbers.push_back(7);
  for (int vec : numbers) {
    std::cout << vec << std::endl;
  }

  hash_map<std::string, int> my_map;
  my_map["toto"] = 2;
  for (auto mm : my_map) {
    std::cout << mm.first << " -> " << mm.second << std::endl;
  }
}

void test_chevron() {
  std::cout << "test_chevron" << std::endl;
  std::vector<std::pair<int,int>> toto;
  toto.push_back(std::make_pair(2, 4));
}
}  // namespace operations_research

int main() {
  operations_research::test_unique();
  operations_research::test_auto();
  operations_research::test_chevron();
  return 0;
}
