//
// Copyright (c) 2012-2015 Krzysztof Jusiak (krzysztof at jusiak dot net)
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <cstdio>
#include <fstream>
#include <regex>
#include "boost/di.hpp"

namespace {

auto disassemble(const std::string& f, const std::string& progname, const std::regex& rgx) {
#if defined(COVERAGE)
    return std::string{};
#endif
    std::string tmp_file = std::tmpnam(nullptr);
    struct remove_tmp_file {
        explicit remove_tmp_file(const std::string& name)
            : name(name)
        { }

        ~remove_tmp_file() {
            std::remove(name.c_str());
        }

        std::string name;
    } remove(tmp_file);
    std::ofstream commands(tmp_file);
    std::stringstream command;
    std::stringstream result;
    FILE *in = nullptr;

#if defined(__linux)
    command << "gdb --batch -x " << tmp_file;
    commands << "file " << progname << std::endl;
    commands << "disassemble " << f << std::endl;
    commands << "q" << std::endl;
#elif defined(__APPLE__)
    command << "lldb -s " << tmp_file;
    commands << "file " << progname << std::endl;
    commands << "di -n " << f << std::endl;
    commands << "q" << std::endl;
#elif defined(_WIN32) || defined(_WIN64)
    command << "cdb -cf " << tmp_file << " -z " << progname;
    commands << "uf " << f << std::endl;
    commands << "q" << std::endl;
#endif

    if (!(in = popen(command.str().c_str(), "r"))) {
        return result.str();
    }

    char buff[1024];
    auto is_asm = true;
    while (std::fgets(buff, sizeof(buff), in)) {
        std::string str{buff};
        if (std::regex_match(str, std::regex{".*:$"})) {
            is_asm = false;
        }

        if (is_asm) {
            std::smatch match;
            if (std::regex_search(str, match, rgx)) {
                result << match[1];
            }
        }
    }

    pclose(in);
    expect(!result.str().empty());
    return result.str();
}

bool is_same_asm(const char* progname, const std::string& name, const std::regex& rgx = std::regex{".*:(.*)"}) {
    return disassemble("given_" + name, progname, rgx) == disassemble("expected_" + name, progname, rgx);
}

} // namespace

namespace di = boost::di;

struct i { virtual ~i() noexcept = default; virtual int dummy() = 0; };
struct impl : i { impl(int i) : i_(i) { }; int dummy() override { return i_; }; int i_ = 0; };

// ---------------------------------------------------------------------------

auto given_no_bindings() {
    auto injector = di::make_injector();
    return injector.create<int>();
}

auto expected_no_bindings() {
    return 0;
}

test no_bindings = [](auto progname) {
    expect(is_same_asm(progname, "no_bindings"));
};

// ---------------------------------------------------------------------------

auto given_bind_int() {
    auto injector = di::make_injector(
        di::bind<int>().to(42)
    );

    return injector.create<int>();
}

auto expected_bind_int() {
    return 42;
}

test bind_int = [](auto progname) {
    expect(is_same_asm(progname, "bind_int"));
};

// ---------------------------------------------------------------------------

auto given_bind_interface() {
    auto injector = di::make_injector(
        di::bind<i, impl>()
    );

    return injector.create<std::unique_ptr<i>>();
}

auto expected_bind_interface() {
    return std::make_unique<impl>(0);
}

test bind_interface = [](auto progname) {
    expect(is_same_asm(progname, "bind_interface", std::regex{".*:([^ ]*).*"}));
};

// ---------------------------------------------------------------------------

auto name_int = []{};

struct c {
    BOOST_DI_INJECT(c, (named = name_int) int);
};

c::c(int) { }

auto given_bind_named_int() {
    auto injector = di::make_injector(
        di::bind<int>().named(name_int).to(42)
    );

    return injector.create<c>();
}

auto expected_bind_named_int() {
    return c{42};
}

test bind_named_int = [](auto progname) {
    expect(is_same_asm(progname, "bind_named_int"));
};

// ---------------------------------------------------------------------------

auto given_module_no_bindings() {
    struct module {
        auto configure() const noexcept {
            return di::make_injector();
        }
    };

    return di::make_injector(module{}).create<int>();
}

auto expected_module_no_bindings() {
    return 0;
}

test module_no_bindings = [](auto progname) {
    expect(is_same_asm(progname, "module_no_bindings"));
};

// ---------------------------------------------------------------------------

auto given_module_bind_int() {
    struct module {
        auto configure() const noexcept {
            return di::make_injector(
                di::bind<int>().to(42)
            );
        }
    };

    return di::make_injector(module{}).create<int>();
}

auto expected_module_bind_int() {
    return 42;
}

test module_bind_int = [](auto progname) {
    expect(is_same_asm(progname, "module_bind_int"));
};

// ---------------------------------------------------------------------------

auto given_bind_interface_shared() {
    auto injector = di::make_injector(
        di::bind<i, impl>().in(di::shared)
    );

    return injector.create<std::shared_ptr<i>>();
}

auto expected_bind_interface_shared() {
    return std::make_shared<impl>(0);
}

#if 0
    test bind_interface_shared = [](auto progname) {
        expect(is_same_asm(progname, "bind_interface_shared", std::regex{".*:([^ ]*).*"}));
    };
#endif

// ---------------------------------------------------------------------------
