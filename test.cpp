#include "flag.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Parse failures") {
    flag::FlagSet flags;
    std::optional<flag::Error> error;
    REQUIRE(flags.Parsed() == false);

    auto parse = [&](std::vector<const char *> &v) {
        error = flags.Parse(static_cast<int>(v.size()), v.data());
    };

    SECTION("Attempt to parse 0") {
        std::vector<const char *> args{};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::NumArgs);
    }

    SECTION("Bad argument name: ---") {
        std::vector<const char *> args{"program", "---"};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::BadSyntax);
    }

    SECTION("Bad argument name: --=") {
        std::vector<const char *> args{"program", "--="};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::BadSyntax);
    }

    SECTION("Undefined flag: -d") {
        std::vector<const char *> args{"program", "-d"};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::UndefinedFlag);
    }

    SECTION("Help with --help") {
        std::vector<const char *> args{"program", "--help", "--", "arg"};
        parse(args);
        REQUIRE(error.has_value());
        REQUIRE(error->Type() == flag::Error::EType::Help);
    }

    SECTION("Help with -h") {
        std::vector<const char *> args{"program", "-h", "--", "arg"};
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
        std::vector<const char *> args{"program"};
        parse(args);
        REQUIRE(flags.Args().empty() == true);
    }

    SECTION("Positional args") {
        std::vector<const char *> args{"program", "arg1", "arg2"};
        parse(args);
        REQUIRE(flags.Args().size() == 2);
        REQUIRE(flags.Args()[0] == "arg1");
        REQUIRE(flags.Args()[1] == "arg2");
    }

    SECTION("Positional args with '-' as an argument") {
        std::vector<const char *> args{"program", "-", "arg2"};
        parse(args);
        REQUIRE(flags.Args().size() == 2);
        REQUIRE(flags.Args()[0] == "-");
        REQUIRE(flags.Args()[1] == "arg2");
    }

    SECTION("Positional args with separator") {
        std::vector<const char *> args{"program", "--", "arg1", "arg2"};
        parse(args);
        REQUIRE(flags.Args().size() == 2);
        REQUIRE(flags.Args()[0] == "arg1");
        REQUIRE(flags.Args()[1] == "arg2");
    }

    SECTION("Bool flag") {
        bool b{false};
        flags.Var(b, "b", "A boolean");

        SECTION("-b not passed") {
            std::vector<const char *> args{"program"};
            parse(args);
            REQUIRE(flags.Args().empty());
            REQUIRE(b == false);
        }

        SECTION("-b passed") {
            std::vector<const char *> args{"program", "-b"};
            parse(args);
            REQUIRE(flags.Args().empty());
            REQUIRE(b == true);
        }

        SECTION("-b=true") {
            std::vector<const char *> args{"program", "-b=true"};
            parse(args);
            REQUIRE(flags.Args().empty());
            REQUIRE(b == true);
        }

        SECTION("-b=false") {
            std::vector<const char *> args{"program", "-b=false"};
            parse(args);
            REQUIRE(flags.Args().empty());
            REQUIRE(b == false);
        }
    }

    SECTION("String flag") {
        std::string s{};
        flags.Var(s, "string", "A string flag");

        SECTION("String passed") {
            std::vector<const char*> args{"program", "-s=foo"};
            parse(args);
            REQUIRE(flags.Args().empty());
            REQUIRE(s == "foo");
        }
    }

    if (error) {
        CAPTURE(error->What());
    }
    REQUIRE(error.has_value() == false);
    REQUIRE(flags.Parsed() == true);
}
