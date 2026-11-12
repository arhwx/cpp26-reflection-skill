#include "solution.hpp"

#include <print>
#include <string>

enum class Color : unsigned char { red = 1, green = 2, blue = 4 };

enum Level : short { low = -1, mid = 0, high = 1 };

struct Point {
    int x;
    int y;
};

template <class T, int N>
struct Buffer {
    T items[N];
    int used;
};

template <class... Ts>
struct List {};

template <bool On>
struct Flag {};

template <int Delta>
struct Offset {};

struct Node {
    long value;
    Node* next;
    const Node* prev;
};

union Number {
    int i;
    double d;
};

class Shape : public Point {
public:
    Color color;
    const char* label;
    double (*area)(const Shape&);
    Buffer<Point, 2> corners;
    struct {
        short lo;
        short hi;
    } range;

private:
    Number weight;
};

using Id = unsigned long long;

struct Entry {
    Id id;
    Level level;
    Number value;
    List<Id, Flag<true>, Offset<-2>> tags;
    Point& anchor;
    float scores[3];
};

template <class T>
void show(const char* title) {
    std::println("== {}", title);
    std::print("{}", type_dump<T>());
}

int main() {
    show<int>("int");
    show<const char* const&>("reference to const pointer");
    show<Shape>("Shape");
    show<Node>("Node");
    show<Entry>("Entry");
    show<int (*[2])(char, Point&&)>("array of function pointers");
    show<void(Color, const Color&)>("function type");
    show<List<>>("empty list");
    show<const volatile Id>("const volatile alias");
}
