#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ortools/base/hash.h"

namespace operations_research {
struct Foo {
  Foo() { std::cout << "Foo::Foo\n"; }
  ~Foo() { std::cout << "Foo::~Foo\n"; }
  void bar() { std::cout << "Foo::bar\n"; }
};

void f(const Foo& foo) { std::cout << "f(const Foo&)\n"; }

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

  std::unordered_map<std::string, int> my_map;
  my_map["toto"] = 2;
  for (auto mm : my_map) {
    std::cout << mm.first << " -> " << mm.second << std::endl;
  }
}

void test_chevron() {
  std::cout << "test_chevron" << std::endl;
  std::vector<std::pair<int, int>> toto;
  toto.push_back(std::make_pair(2, 4));
}

class A {
 public:
  virtual ~A() {}
  virtual int V() const { return 1; }
};

class B : public A {
 public:
  ~B() override {}
  int V() const override { return 2; }
};

void test_override() {
  std::cout << "test_override" << std::endl;
  B* b = new B();
  if (b->V() != 2) {
    std::cout << "Problem with override" << std::endl;
  }
}
}  // namespace operations_research

int main() {
  operations_research::test_unique();
  operations_research::test_auto();
  operations_research::test_chevron();
  operations_research::test_override();
  return 0;
}
