#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

struct UserV1 {
    int id;
    std::string name;
    [[=sql::varchar(64)]] std::string email;
    int age;
    std::string fax;
    std::optional<std::string> phone;
};

struct UserV2 {
    long long id;
    [[=sql::renamed_from("name")]] std::string full_name;
    [[=sql::varchar(128)]] std::string email;
    std::optional<int> age;
    bool active = true;
    std::string country = "Cote d'Ivoire";
    std::optional<double> score;
    int logins;
    short level = -2;
    [[=sql::renamed_from("phone")]] std::optional<std::string> mobile;
};

struct UserV3 {
    long long id;
    [[=sql::renamed_from("name")]] std::string full_name;
    std::string email;
    int age = 18;
    bool active = true;
    std::string country = "Cote d'Ivoire";
    [[=sql::renamed_from("score")]] float rating = 0.5f;
    int logins;
    [[=sql::renamed_from("phone")]] [[=sql::varchar(20)]] std::string mobile;
};

// both members would continue name
struct Twice {
    int id;
    std::string name;
    [[=sql::renamed_from("name")]] std::string nick;
};

struct WithTags {
    int id;
    std::vector<std::string> tags;
};

struct Unsigned {
    unsigned id;
};

struct Tiny {
    std::optional<long long> id;
    bool flag;
    double ratio = 0.25;
};

template <class Old, class New>
concept migratable = requires { sql::migration<Old, New>("t"); };

template <class Old, class New>
void show(std::string_view title, std::string_view table) {
    std::println("-- {}", title);
    std::vector<std::string> steps = sql::migration<Old, New>(table);
    if (steps.empty())
        std::println("(no changes)");
    for (const std::string& step : steps)
        std::println("{}", step);
}

int main() {
    show<UserV1, UserV2>("v1 to v2", "users");
    show<UserV2, UserV3>("v2 to v3", "users");
    show<UserV1, UserV3>("v1 to v3", "accounts");
    show<UserV3, UserV3>("v3 to v3", "users");
    show<UserV3, UserV1>("v3 to v1", "users");
    show<UserV1, Tiny>("v1 to tiny", "t");

    std::println("v1 to v2: {}", migratable<UserV1, UserV2>);
    std::println("v1 to twice: {}", migratable<UserV1, Twice>);
    std::println("twice to v1: {}", migratable<Twice, UserV1>);
    std::println("v1 to tags: {}", migratable<UserV1, WithTags>);
    std::println("tags to v1: {}", migratable<WithTags, UserV1>);
    std::println("v1 to unsigned: {}", migratable<UserV1, Unsigned>);
}
