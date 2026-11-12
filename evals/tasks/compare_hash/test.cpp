#include "solution.hpp"

#include <cstddef>
#include <print>
#include <string>
#include <unordered_set>
#include <utility>

struct Point {
    int x;
    int y;
};

class Account {
public:
    Account(std::string owner, long balance) : owner_(std::move(owner)), balance_(balance) {}
    void freeze() { frozen_ = true; }

private:
    std::string owner_;
    long balance_;
    bool frozen_ = false;
};

struct Sample {
    double value;
    char unit;
    std::string label;
};

struct PointHash {
    std::size_t operator()(const Point& p) const { return members_hash(p); }
};

struct PointEq {
    bool operator()(const Point& a, const Point& b) const { return members_equal(a, b); }
};

int main() {
    Point a{1, 2}, b{1, 2}, c{1, 3};
    std::println("{} {}", members_equal(a, b), members_equal(a, c));
    std::println("{} {}", members_hash(a) == members_hash(b), members_hash(a) == members_hash(c));

    Account x("ann", 10), y("ann", 10), z("ann", 20), w("ann", 10);
    w.freeze();
    std::println("{} {} {}", members_equal(x, y), members_equal(x, z), members_equal(x, w));
    std::println("{} {}", members_hash(x) == members_hash(y), members_hash(x) == members_hash(z));

    Sample s1{2.5, 'm', "len"}, s2{2.5, 'm', "len"}, s3{2.5, 'm', "Len"};
    std::println("{} {}", members_equal(s1, s2), members_equal(s1, s3));
    std::println("{}", members_hash(s1) == members_hash(s2));

    std::unordered_set<Point, PointHash, PointEq> set{{1, 2}, {2, 1}, {1, 2}, {0, 0}};
    std::println("{}", set.size());
}
