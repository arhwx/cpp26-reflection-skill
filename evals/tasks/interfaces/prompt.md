I want Go-style interfaces: an interface is a plain struct of member function declarations, and any type that has those methods satisfies it, with no inheritance and no change to the type. A non-owning handle `dyn<I>` should erase the type but keep normal call syntax, like `s->area()`. The same interface should also work as a concept, and when a type does not fit I want to be told which methods are missing.

Everything goes in `namespace iface`:
- `consteval std::string_view iface::why_not(std::meta::info type, std::meta::info interface)` (`constexpr` is fine too): an empty string when `type` satisfies `interface`, otherwise a message, as described below.
- `template <class T, class I> concept iface::implements`: true exactly when `T` satisfies `I`. It is also used as a type constraint, `template <iface::implements<Shape> T>`.
- `template <class I> class iface::dyn`: a non-owning handle to an object of any type that satisfies `I`.
  - `template <class T> dyn(T& object)`: an implicit constructor. The handle refers to `object`, which must outlive it. Constructing a `dyn<I>` from a type that does not satisfy `I` must not compile.
  - `dyn` is copyable, and a copy refers to the same object.
  - `operator->() const` returns a pointer to an object that has one member for each method of `I`, with the method's name. `s->m(args...)` takes arguments of the method's parameter types, calls the matching member function of the original object with them and returns its result. This works through a `const dyn<I>` too, also for methods that are not `const`, and changes made by those methods are visible in the original object.

Interfaces and methods:
- The methods of an interface are its named non-static member functions. Constructors, the destructor, assignment operators and other operators are not methods. The interface's methods are only declared and are never called.
- A type `T` satisfies `I` when, for each method of `I`, `T` has a non-static member function with the same name, the same return type and the same parameter types. When the method of `I` is `const`, the member function of `T` must be `const` too. When it is not `const`, a `const` or a non-`const` member function fits.
- `T` may have more member functions than `I` asks for, including other overloads with the same name, like `Square::scale(int)` in the test. Only the member function that fits is used.

What `why_not` returns:
- An empty string when the type satisfies the interface.
- Otherwise `<type> does not satisfy <interface>: missing ` followed by the names of the methods of the interface that have no fitting member function, in the order they are declared in the interface, separated by `, `. Type names are unqualified.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

// an interface is a plain struct of declarations
struct Shape {
    double area() const;
    void scale(double factor);
    std::string name() const;
};

struct Named {
    std::string name() const;
    void rename(const std::string& to);
};

// the types know nothing about the interfaces
struct Rect {
    double w, h;
    double area() const { return w * h; }
    void scale(double factor) { w *= factor, h *= factor; }
    std::string name() const { return "rect"; }
};

struct Square {
    double side;
    double area() const { return side * side; }
    void scale(double factor) { side *= factor; }
    void scale(int times) { side *= times * 10; }  // an overload the interface does not ask for
    std::string name() const { return "square"; }
    void rotate() {}
};

struct Blob {
    double area() const { return 1; }
    void scale(int) {}  // wrong parameter type
};

struct Gauge {
    double area() { return 0; }  // not const, but Shape asks for a const area
    void scale(double) {}
    std::string name() const { return "gauge"; }
};

struct Pet {
    std::string label;
    std::string name() const { return label; }
    void rename(const std::string& to) { label = to; }
};

double total_area(const std::vector<iface::dyn<Shape>>& shapes) {
    double sum = 0;
    for (const auto& s : shapes) sum += s->area();
    return sum;
}

template <iface::implements<Shape> T>  // the same interface as a compile-time constraint
void describe(const T& s) {
    std::println("{} with area {}", s.name(), s.area());
}

int main() {
    Rect r{2, 3};
    Square q{4};
    std::vector<iface::dyn<Shape>> shapes{r, q};
    for (const auto& s : shapes) std::println("{}: {}", s->name(), s->area());
    for (const auto& s : shapes) s->scale(0.5);
    std::println("total after scale: {}", total_area(shapes));
    std::println("rect is now {} x {}", r.w, r.h);
    describe(q);

    Pet pet{"tom"};
    iface::dyn<Named> named = pet;
    named->rename("rex");
    std::println("renamed: {} {}", named->name(), pet.label);

    std::println("Rect implements Shape: {}", iface::implements<Rect, Shape>);
    std::println("Square implements Shape: {}", iface::implements<Square, Shape>);
    std::println("Pet implements Named: {}", iface::implements<Pet, Named>);
    std::println("Rect implements Named: {}", iface::implements<Rect, Named>);
    std::println("Blob implements Shape: {}", iface::implements<Blob, Shape>);
    std::println("why_not(Rect, Shape): \"{}\"", iface::why_not(^^Rect, ^^Shape));
    std::println("{}", iface::why_not(^^Blob, ^^Shape));
    std::println("{}", iface::why_not(^^Gauge, ^^Shape));
    std::println("{}", iface::why_not(^^Rect, ^^Named));
    std::println("{}", iface::why_not(^^Pet, ^^Shape));
}
```

It must print exactly:

```
rect: 6
square: 16
total after scale: 5.5
rect is now 1 x 1.5
square with area 4
renamed: rex rex
Rect implements Shape: true
Square implements Shape: true
Pet implements Named: true
Rect implements Named: false
Blob implements Shape: false
why_not(Rect, Shape): ""
Blob does not satisfy Shape: missing scale, name
Gauge does not satisfy Shape: missing area
Rect does not satisfy Named: missing rename
Pet does not satisfy Shape: missing area, scale
```

Write the complete header to `solution.hpp` in the current directory.
