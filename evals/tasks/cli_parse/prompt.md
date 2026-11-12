I want command line parsing driven by a plain options struct: the member names are the option names.

API: `template <class T> std::optional<T> parse_args(int argc, const char* const* argv)`.

Rules:
- Start from `T{}`, so members keep their default values unless set.
- `argv[0]` is the program name and is skipped. Every other argument must be `--name=value` or `--name`, where `name` is exactly a member name of `T`.
- Members are `int`, `bool` or `std::string`.
- `int`: the whole value must be a decimal integer, optionally negative. Anything else is an error.
- `bool`: `--name` alone sets it to true; `--name=true` and `--name=false` also work. Any other value is an error.
- `std::string`: the value is taken as is and may be empty.
- `--name` without `=value` is an error for non-bool members.
- An unknown name or an argument that does not start with `--` is an error.
- On any error, return `std::nullopt`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstddef>
#include <optional>
#include <print>
#include <string>

struct Options {
    int port = 80;
    bool verbose = false;
    std::string host = "localhost";
    int retries = 3;
    bool dry_run = false;
};

template <std::size_t N>
void run(const char* (&args)[N]) {
    std::optional<Options> o = parse_args<Options>(static_cast<int>(N), args);
    if (!o) {
        std::println("error");
        return;
    }
    std::println("port={} verbose={} host={} retries={} dry_run={}", o->port, o->verbose, o->host, o->retries,
                 o->dry_run);
}

int main() {
    const char* a1[] = {"prog"};
    const char* a2[] = {"prog", "--port=8080", "--verbose", "--host=example.org"};
    const char* a3[] = {"prog", "--dry_run", "--retries=-1", "--verbose=false"};
    const char* a4[] = {"prog", "--host=", "--verbose=true", "--port=1"};
    const char* a5[] = {"prog", "--color=red"};
    const char* a6[] = {"prog", "--port=80x"};
    const char* a7[] = {"prog", "--port"};
    const char* a8[] = {"prog", "verbose"};
    const char* a9[] = {"prog", "--verbose=yes"};
    run(a1);
    run(a2);
    run(a3);
    run(a4);
    run(a5);
    run(a6);
    run(a7);
    run(a8);
    run(a9);
}
```

It must print exactly:

```
port=80 verbose=false host=localhost retries=3 dry_run=false
port=8080 verbose=true host=example.org retries=3 dry_run=false
port=80 verbose=false host=localhost retries=-1 dry_run=true
port=1 verbose=true host= retries=3 dry_run=false
error
error
error
error
error
```

Write the complete header to `solution.hpp` in the current directory.
