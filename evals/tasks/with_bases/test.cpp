#include "solution.hpp"

#include <print>

struct A {
    int a = 1;
};

struct B : A {
    int b = 2;
};

struct C : B {
    int c = 3;
};

struct X {
    int x = 10;
};

struct Y {
    int y = 20;
};

struct M : X, Y {
    int m = 30;
};

struct Empty {};

struct E : Empty {
    int e = 5;
};

struct N : C, M {
    int n = 0;
};

struct Tag : Empty {};

int main() {
    std::println("{}", to_string(A{}));
    std::println("{}", to_string(C{}));
    std::println("{}", to_string(M{}));
    std::println("{}", to_string(E{}));

    N n;
    n.a = 7;
    n.n = 9;
    std::println("{}", to_string(n));
    std::println("{}", to_string(Tag{}));
}
