#include <iostream>
#include <memory>
#include <vector>

// struct Foo {
//   Foo() { std::cout << "Foo::Foo\n"; }
//   ~Foo() { std::cout << "Foo::~Foo\n"; }
//   void bar() { std::cout << "Foo::bar\n"; }
// };

// void f(const Foo &foo) {
//   std::cout << "f(const Foo&)\n";
// }

// void test_unique() {
//   std::unique_ptr<Foo> p1(new Foo);  // p1 owns Foo
//   if (p1) p1->bar();

//   {
//     std::unique_ptr<Foo> p2(std::move(p1));  // now p2 owns Foo
//     f(*p2);

//     p1 = std::move(p2);  // ownership returns to p1
//     std::cout << "destroying p2...\n";
//   }

//   if (p1) p1->bar();
//   // Foo instance is destroyed when p1 goes out of scope
// }

void test_auto() {
  std::vector<int> numbers;
  numbers.push_back(1);
  numbers.push_back(2);
  numbers.push_back(3);
  numbers.push_back(4);
  numbers.push_back(5);
  numbers.push_back(6);
  numbers.push_back(7);
  for (auto xyz : numbers) {
    std::cout << xyz << std::endl;
  }
}

int main() {
  //  test_unique();
  test_auto();
  return 0;
}
