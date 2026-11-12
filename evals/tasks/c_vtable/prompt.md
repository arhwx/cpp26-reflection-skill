Our application loads plugins written in C++, but the boundary between the host and a plugin must be plain C so that compiler upgrades do not break it. Today every plugin class has a hand-written C struct of function pointers, one thunk per method and a hand-written C header, and they go stale. I want the table, the thunks and the C header text generated from the class itself.

Everything goes in `namespace plugin`:
- `template <class T> concept plugin::exportable`: true exactly when `plugin::why_not<T>()` is empty.
- `template <class T> consteval std::string_view plugin::why_not()` (`constexpr` is fine too): an empty string when `T` can be exported, otherwise a message, as described below.
- `plugin::vtable<T>`: an aggregate struct type, the C table of `T`. Naming it for a type that is not exportable must not compile, so that `requires { typename plugin::vtable<T>; }` is false. Its non-static data members are exactly these, in this order, with these names and types, and nothing else:
  - `name`, of type `const char*`;
  - `create`, of type `void* (*)()`;
  - `destroy`, of type `void (*)(void*)`;
  - then one member for each exported method of `T` (see below), in declaration order, named like the method. Its type is a pointer to a function returning the method's return type, whose first parameter is `void*` for a non-`const` method and `const void*` for a `const` method, followed by the method's parameter types in order.
- `template <class T> const plugin::vtable<T>& plugin::table()`: the table of `T`. Every call for the same `T` returns a reference to the same object. Its members:
  - `name` points to the name of `T` without namespaces, as a null-terminated string.
  - `create()` creates a new `T` with `new T()` and returns its address.
  - `destroy(p)` deletes the `T` that `p` points to, which came from `create`.
  - each method member, called with an object pointer from `create` and the method's arguments, calls that method on that object with those arguments and returns its result.
- `template <class T> consteval std::string_view plugin::c_header()` (`constexpr` is fine too), for an exportable `T`: the C declaration of the table, in the format below.

Exported methods of a class `T`: its public non-static member functions that have a name. Constructors, the destructor, operators and static member functions are not exported, and neither are private or protected members. The test has no member function templates.

C types:
- These types are spelled in C as written: `void` (only as a return type), `bool`, `char`, `int`, `unsigned`, `long`, `float`, `double`. Here `unsigned` means `unsigned int`.
- A pointer to one of these types, or to a `const` one, is a C type too, spelled as the pointee followed by `*`, with `const ` in front for a `const` pointee: `int*`, `const char*`, `void*`.
- Every other type, including references, class types and `void` as a parameter type, is not a C type.

What `why_not<T>()` returns, where `<T>` is the name of `T` without namespaces and `<m>` is a method name:
- `<T> has no public default constructor` when `T` cannot be default constructed or destroyed by the host.
- Otherwise the first problem found when the exported methods are visited in declaration order. For each method, check in this order:
  - `<T>::<m> is overloaded` when another exported method has the same name;
  - `<T>::<m> returns a non-C type` when the return type is not a C type;
  - `<T>::<m> parameter <p> has a non-C type` for the first parameter whose type is not a C type, where `<p>` is the parameter name as described below.
- An empty string when there is no problem.

Format of `c_header<T>()`, where every line except the last ends with a newline and there is no newline at the end:

```
typedef struct <T>_vtable {
    const char* name;
    void* (*create)(void);
    void (*destroy)(void* self);
    <method lines>
} <T>_vtable;
```

There is one method line for each exported method, in declaration order, indented by four spaces: `<return> (*<m>)(<self>, <type> <param>, ...);` where `<return>` and `<type>` are C spellings, `<self>` is `void* self` for a non-`const` method and `const void* self` for a `const` method, and each method parameter follows as `, <type> <param>`. A method without parameters has only `<self>` between the parentheses. `<param>` is the parameter's name, or `arg<k>` for a parameter declared without a name, where `<k>` is its position among the method's parameters, counting from 1.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>

class Counter {
public:
    Counter() { std::println("  Counter created"); }
    ~Counter() { std::println("  Counter destroyed with total {}", total_); }

    void add(int amount) {
        total_ += amount;
        ++count_;
    }
    int total() const { return total_; }
    double mean(int) const { return count_ == 0 ? 0.0 : double(total_) / count_; }
    bool reset(bool keep_count, long start) {
        total_ = int(start);
        if (!keep_count) count_ = 0;
        return count_ != 0;
    }
    void copy_to(int* out, unsigned times) const {
        for (unsigned i = 0; i < times; ++i) out[i] = total_;
    }
    const char* label() const { return total_ < 10 ? "small" : "large"; }

    static int version() { return 2; }
    Counter& operator+=(int amount) {
        add(amount);
        return *this;
    }

private:
    void bump() { ++total_; }
    int total_ = 0;
    int count_ = 0;
};

struct Silent {};

struct Overloaded {
    void set(int) {}
    void set(double) {}
};

struct Stringy {
    int size() const { return 0; }
    void rename(const char* from, std::string to) {}
};

struct ByRef {
    int& slot() { return value; }
    int value = 0;
};

struct Configured {
    explicit Configured(int level) : level(level) {}
    int get() const { return level; }
    int level;
};

using CounterTable = plugin::vtable<Counter>;
static_assert(std::is_same_v<decltype(CounterTable::name), const char*>);
static_assert(std::is_same_v<decltype(CounterTable::create), void* (*)()>);
static_assert(std::is_same_v<decltype(CounterTable::destroy), void (*)(void*)>);
static_assert(std::is_same_v<decltype(CounterTable::add), void (*)(void*, int)>);
static_assert(std::is_same_v<decltype(CounterTable::total), int (*)(const void*)>);
static_assert(std::is_same_v<decltype(CounterTable::copy_to), void (*)(const void*, int*, unsigned)>);
static_assert(std::is_same_v<decltype(CounterTable::label), const char* (*)(const void*)>);
static_assert(std::is_standard_layout_v<CounterTable> && std::is_trivially_copyable_v<CounterTable>);
static_assert(sizeof(CounterTable) == 9 * sizeof(void*));
static_assert(sizeof(plugin::vtable<Silent>) == 3 * sizeof(void*));

template <class T>
concept has_table = requires { typename plugin::vtable<T>; };

// a C host sees only the table and a void pointer
void run_host(const CounterTable& t) {
    std::println("host loads plugin {}", t.name);
    void* obj = t.create();
    t.add(obj, 4);
    t.add(obj, 7);
    std::println("total {} mean {} label {}", t.total(obj), t.mean(obj, 0), t.label(obj));
    int out[3] = {};
    t.copy_to(obj, out, 2);
    std::println("copied {} {} {}", out[0], out[1], out[2]);
    bool kept = t.reset(obj, true, 3);
    std::println("reset keeping count: {} total {} mean {}", kept, t.total(obj), t.mean(obj, 0));
    kept = t.reset(obj, false, 5);
    std::println("reset dropping count: {} mean {}", kept, t.mean(obj, 0));
    t.destroy(obj);
}

int main() {
    const CounterTable& t = plugin::table<Counter>();
    std::println("same table: {}", &t == &plugin::table<Counter>());
    run_host(t);

    // two objects behind one table are independent
    void* a = t.create();
    void* b = t.create();
    t.add(a, 1);
    t.add(b, 20);
    std::println("a {} b {}", t.label(a), t.label(b));
    t.destroy(b);
    t.destroy(a);

    std::println("{}", plugin::c_header<Counter>());
    std::println("{}", plugin::c_header<Silent>());
    void* s = plugin::table<Silent>().create();
    plugin::table<Silent>().destroy(s);
    std::println("{} has no methods", plugin::table<Silent>().name);

    std::println("Counter: '{}' {}", plugin::why_not<Counter>(), plugin::exportable<Counter>);
    std::println("Overloaded: '{}' {}", plugin::why_not<Overloaded>(), plugin::exportable<Overloaded>);
    std::println("Stringy: '{}' {}", plugin::why_not<Stringy>(), plugin::exportable<Stringy>);
    std::println("ByRef: '{}' {}", plugin::why_not<ByRef>(), plugin::exportable<ByRef>);
    std::println("Configured: '{}' {}", plugin::why_not<Configured>(), plugin::exportable<Configured>);
    std::println("tables: {} {} {}", has_table<Counter>, has_table<Silent>, has_table<Stringy>);
}
```

It must print exactly:

```
same table: true
host loads plugin Counter
  Counter created
total 11 mean 5.5 label large
copied 11 11 0
reset keeping count: true total 3 mean 1.5
reset dropping count: false mean 0
  Counter destroyed with total 5
  Counter created
  Counter created
a small b large
  Counter destroyed with total 20
  Counter destroyed with total 1
typedef struct Counter_vtable {
    const char* name;
    void* (*create)(void);
    void (*destroy)(void* self);
    void (*add)(void* self, int amount);
    int (*total)(const void* self);
    double (*mean)(const void* self, int arg1);
    bool (*reset)(void* self, bool keep_count, long start);
    void (*copy_to)(const void* self, int* out, unsigned times);
    const char* (*label)(const void* self);
} Counter_vtable;
typedef struct Silent_vtable {
    const char* name;
    void* (*create)(void);
    void (*destroy)(void* self);
} Silent_vtable;
Silent has no methods
Counter: '' true
Overloaded: 'Overloaded::set is overloaded' false
Stringy: 'Stringy::rename parameter to has a non-C type' false
ByRef: 'ByRef::slot returns a non-C type' false
Configured: 'Configured has no public default constructor' false
tables: true true false
```

Write the complete header to `solution.hpp` in the current directory.
