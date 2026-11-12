// dependency_injection: a container that builds any class from its constructor parameters, like dagger or spring
// nothing is registered. each object is made once, after the objects it needs, and a dependency cycle does not compile
#include <algorithm>
#include <format>
#include <memory>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

namespace di {
using std::meta::info;

// the public constructors of a class, other than copy and move
consteval std::vector<info> constructors(info type) {
    std::vector<info> out;
    for (info m : members_of(type, std::meta::access_context::unprivileged()))
        if (is_constructor(m) && !is_copy_constructor(m) && !is_move_constructor(m) && !is_deleted(m)) out.push_back(m);
    return out;
}
// what the constructor asks for: a parameter U& or const U& asks for the shared U
consteval std::vector<info> needs(info type) {
    std::vector<info> out;
    for (info p : parameters_of(constructors(type)[0])) out.push_back(remove_cvref(type_of(p)));
    return out;
}
// why a class cannot be built, or "ok". path holds the classes being built, to find cycles
consteval std::string problem(info type, std::vector<info> path = {}) {
    std::string name(display_string_of(type));
    if (auto it = std::ranges::find(path, type); it != path.end()) {
        std::string cycle = "cycle:";
        for (; it != path.end(); ++it) cycle += " " + std::string(display_string_of(*it)) + " ->";
        return cycle + " " + name;
    }
    if (!is_class_type(type) || constructors(type).size() != 1) return name + " needs exactly one constructor besides copy and move";
    path.push_back(type);
    for (info dep : needs(type))
        if (std::string why = problem(dep, path); why != "ok") return why;
    return "ok";
}
consteval std::string_view check(info type) { return std::define_static_string(problem(type)); }

class injector {
    struct object {
        std::type_index type;
        std::unique_ptr<void, void (*)(void*)> owner;
    };
    std::vector<object> objects;  // in the order their constructors finished

public:
    ~injector() {  // newest first, so every object goes before the objects it uses
        while (!objects.empty()) objects.pop_back();
    }

    template <class T>
        requires(check(^^T) == "ok")
    T& get() {
        for (object& o : objects)
            if (o.type == typeid(T)) return *static_cast<T*>(o.owner.get());
        static constexpr auto deps = std::define_static_array(needs(^^T));
        T* made = [&]<std::size_t... I>(std::index_sequence<I...>) {
            return new T{get<typename [:deps[I]:]>()...};  // a braced list runs the gets left to right
        }(std::make_index_sequence<deps.size()>());
        objects.push_back({typeid(T), {made, [](void* p) { delete static_cast<T*>(p); }}});
        return *made;
    }
};
}  // namespace di

// ---- user code. java dagger: @Inject Server(Config config, Users users, Logger log) { ... }
struct Config {
    Config() { std::println("  create Config"); }
    ~Config() { std::println("  destroy Config"); }
    int port = 8080;
};
struct Logger {
    Logger() { std::println("  create Logger"); }
    ~Logger() { std::println("  destroy Logger"); }
    void write(std::string_view text) { std::println("  log: {}", text); }
};
struct Database {
    Database(const Config&, Logger& log) : log(log) { std::println("  create Database"); }
    ~Database() { std::println("  destroy Database"); }
    Logger& log;
};
struct Users {
    explicit Users(Database& db) : db(db) { std::println("  create Users"); }
    ~Users() { std::println("  destroy Users"); }
    Database& db;
};
struct Server {
    Server(const Config& config, Users& users, Logger& log) : config(config), users(users), log(log) {
        std::println("  create Server");
    }
    ~Server() { std::println("  destroy Server"); }
    void start() { log.write(std::format("listening on port {}", config.port)); }
    const Config& config;
    Users& users;
    Logger& log;
};

// get does not compile for these: a cycle, and a class with two ways to build it
struct Billing;
struct Orders { explicit Orders(Billing&); };
struct Billing { explicit Billing(Orders&); };
struct Cache { Cache(); explicit Cache(int size); };
template <class T>
concept buildable = requires(di::injector& i) { i.get<T>(); };
static_assert(buildable<Server> && !buildable<Orders> && !buildable<Cache>);

int main() {
    {
        di::injector app;
        std::println("get Server");
        Server& server = app.get<Server>();
        server.start();
        std::println("same Server again: {}", &app.get<Server>() == &server);
        std::println("one Logger for all: {}", &server.log == &server.users.db.log);
        std::println("end of scope");
    }
    std::println("{}", di::check(^^Orders));
    std::println("{}", di::check(^^Cache));
}

// output:
// get Server
//   create Config
//   create Logger
//   create Database
//   create Users
//   create Server
//   log: listening on port 8080
// same Server again: true
// one Logger for all: true
// end of scope
//   destroy Server
//   destroy Users
//   destroy Database
//   destroy Logger
//   destroy Config
// cycle: Orders -> Billing -> Orders
// Cache needs exactly one constructor besides copy and move
