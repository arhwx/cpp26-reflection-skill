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
