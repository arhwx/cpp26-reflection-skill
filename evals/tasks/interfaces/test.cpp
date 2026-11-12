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
