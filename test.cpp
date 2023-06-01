#include "flag.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

void parse(flag::FlagSet& flags, std::vector<const char *> &v) {
  auto error = flags.Parse(static_cast<int>(v.size()), v.data());
  CAPTURE(error->What());
  REQUIRE(error.has_value() == false);
  REQUIRE(flags.Parsed() == true);
}

flag::Error parseError(flag::FlagSet& flags, std::vector<const char *> &v) {
    auto error = flags.Parse(static_cast<int>(v.size()), v.data());
    REQUIRE(error.has_value());
    REQUIRE(flags.Parsed());
    return *error;
}

using ArgsT = std::vector<const char *>;

TEST_CASE("Parse failures") {
    flag::FlagSet flags;
    std::optional<flag::Error> error;
    REQUIRE(flags.Parsed() == false);

    auto parse = [&](std::vector<const char *> &v) {
        error = flags.Parse(static_cast<int>(v.size()), v.data());
    };

    SECTION("Attempt to parse 0") {
        ArgsT args{};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::NumArgs);
    }

    SECTION("Bad argument name: ---") {
        ArgsT args{"program", "---"};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::BadSyntax);
    }

    SECTION("Bad argument name: --=") {
        ArgsT args{"program", "--="};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::BadSyntax);
    }

    SECTION("Undefined flag: -d") {
        ArgsT args{"program", "-d"};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::UndefinedFlag);
    }

    SECTION("Help with --help") {
        ArgsT args{"program", "--help", "--", "arg"};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::Help);
    }

    SECTION("Help with -h") {
        ArgsT args{"program", "-h", "--", "arg"};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::Help);
    }

    REQUIRE(flags.Parsed() == true);
}

TEST_CASE("Parse successes") {
    flag::FlagSet flags;
    std::optional<flag::Error> error{};
    REQUIRE(flags.Parsed() == false);

    auto parse = [&](std::vector<const char *> &v) {
        error = flags.Parse(static_cast<int>(v.size()), v.data());
    };

    SECTION("No arguments") {
        ArgsT args{"program"};
        parse(args);
        REQUIRE(flags.Args().empty() == true);
    }

    SECTION("Positional args") {
        ArgsT args{"program", "arg1", "arg2"};
        parse(args);
        REQUIRE(flags.Args().size() == 2);
        REQUIRE(flags.Args()[0] == "arg1");
        REQUIRE(flags.Args()[1] == "arg2");
    }

    SECTION("Positional args with '-' as an argument") {
        ArgsT args{"program", "-", "arg2"};
        parse(args);
        REQUIRE(flags.Args().size() == 2);
        REQUIRE(flags.Args()[0] == "-");
        REQUIRE(flags.Args()[1] == "arg2");
    }

    SECTION("Positional args with separator") {
        ArgsT args{"program", "--", "arg1", "arg2"};
        parse(args);
        REQUIRE(flags.Args().size() == 2);
        REQUIRE(flags.Args()[0] == "arg1");
        REQUIRE(flags.Args()[1] == "arg2");
    }
}

TEST_CASE("bool") {
    bool b{false};
    flag::FlagSet flags;
    std::string flagName{"b"};
    flags.Var(b, flagName, "A boolean");

    SECTION("-b not passed") {
        ArgsT args{"program"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE(b == false);
    }

    SECTION("-b passed") {
        ArgsT args{"program", "-b"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE(b == true);
    }

    SECTION("-b=true") {
        ArgsT args{"program", "-b=true"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE(b == true);
    }

    SECTION("-b=false") {
        ArgsT args{"program", "-b=false"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE(b == false);
    }
}

TEST_CASE("string") {
    std::string s{};
    flag::FlagSet flags;
    flags.Var(s, "s", "A string flag");
    REQUIRE(flags.Parsed() == false);

    SECTION("String passed") {
        ArgsT args{"program", "-s=foo"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE(s == "foo");
    }
}

TEST_CASE("int") {
    int i{};
    flag::FlagSet flags;
    flags.Var(i, "i", "An int flag");
    REQUIRE(flags.Parsed() == false);

    SECTION("A positive integer") {
        ArgsT args{"program", "-i=1"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE(i == 1);
    }

    SECTION("A negative integer") {
        ArgsT args{"program", "-i=-1"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE(i == -1);
    }

    SECTION("Non-numeric argument") {
        ArgsT args{"program", "-i=+1"};
        auto error = parseError(flags, args);
        REQUIRE(error.Type() == flag::Error::EType::BadValue);
        REQUIRE(i == 0);
    }
}

TEST_CASE("float") {
    float f{};
    flag::FlagSet flags{};
    flags.Var(f, "f", "A float flag");

    using Catch::Matchers::WithinAbs;

    SECTION("A positive integer") {
        ArgsT args{"program", "-f=1"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(f, WithinAbs(1, 0.01));
    }

    SECTION("A positive float") {
        ArgsT args{"program", "-f=1.4"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(f, WithinAbs(1.4, 0.01));
    }

    SECTION("A negative integer") {
        ArgsT args{"program", "-f=-1"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(f, WithinAbs(-1, 0.01));
    }

    SECTION("A negative float") {
        ArgsT args{"program", "-f=-1.93"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(f, WithinAbs(-1.93, 0.01));
    }

    SECTION("Non-numeric argument") {
        ArgsT args{"program", "-f=abc"};
        auto error = parseError(flags, args);
        REQUIRE(error.Type() == flag::Error::EType::BadValue);
        REQUIRE_THAT(f, WithinAbs(0, 0.01));
    }
}

TEST_CASE("double") {
    double d{};
    flag::FlagSet flags{};
    flags.Var(d, "d", "A double flag");

    using Catch::Matchers::WithinAbs;

    SECTION("A positive integer") {
        ArgsT args{"program", "-d=1"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(d, WithinAbs(1, 0.01));
    }

    SECTION("A positive float") {
        ArgsT args{"program", "-d=1.4"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(d, WithinAbs(1.4, 0.01));
    }

    SECTION("A negative integer") {
        ArgsT args{"program", "-d=-1"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(d, WithinAbs(-1, 0.01));
    }

    SECTION("A negative float") {
        ArgsT args{"program", "-d=-1.93"};
        parse(flags, args);
        REQUIRE(flags.Args().empty());
        REQUIRE_THAT(d, WithinAbs(-1.93, 0.01));
    }

    SECTION("Non-numeric argument") {
        ArgsT args{"program", "-d=abc"};
        auto error = parseError(flags, args);
        REQUIRE(error.Type() == flag::Error::EType::BadValue);
        REQUIRE_THAT(d, WithinAbs(0, 0.01));
    }
}
