#include "solution.hpp"

#include <print>
#include <string>

namespace db {
struct column {
    int index;
};
}

struct User {
    [[=json::rename("user_id")]] int id;
    std::string name;
    [[=json::skip]] std::string password;
    [[=db::column{4}]] int age;
    [[=json::rename("e-mail")]] std::string email;
};

struct Point {
    int x;
    int y;
};

struct Secret {
    [[=json::skip]] int key;
};

int main() {
    User u{42, "ann", "swordfish", 31, "ann@example.com"};
    std::println("{}", json::to_json(u));
    std::println("{}", json::to_json(Point{-3, 7}));
    std::println("{}", json::to_json(Secret{1234}));
}
