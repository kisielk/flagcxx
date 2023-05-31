#pragma once

#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace flag {
    class Error {
    public:
        enum class EType {
            Help,
            NumArgs,  // Wrong number of arguments.
            BadSyntax,// Bad flag syntax.
            UndefinedFlag,
            MissingValue,
            BadValue,
        };

        Error(EType type, std::string message) : type(type), message(std::move(message)) {}

        [[nodiscard]] inline EType Type() { return type; }
        [[nodiscard]] inline std::string What() { return message; }

    private:
        EType type;
        std::string message;
    };

    class FlagError {
    public:
        explicit FlagError(std::string message) : message(std::move(message)) {}

        [[nodiscard]] inline std::string What() { return message; }

    private:
        std::string message;
    };

    struct Flag {
        using SetFn = std::function<std::optional<FlagError>(std::string_view)>;

        Flag(SetFn fn, std::string usage, bool isBool = false) : setFn(fn), usage(std::move(usage)), isBool(isBool) {}

        SetFn setFn;
        std::string usage{};
        bool isBool{false};
    };

    class FlagSet {
    public:
        /// Parses a command line.
        /// @returns an optional error if one occurred.
        [[nodiscard]] std::optional<Error> Parse(int argc, const char **argv);

        template<typename T>
        void Var(T &var, std::string name, std::string usage);

        /// Returns whether a command line has been parsed.
        [[nodiscard]] inline bool Parsed() const { return parsed; }

        /// @returns the arguments remaining after flag parsing.
        [[nodiscard]] inline const std::vector<std::string> &Args() const { return args; }

    private:
        bool parsed{false};

        std::vector<std::string> args{};
        std::unordered_map<std::string_view, Flag> flags{};
    };

    std::optional<Error> FlagSet::Parse(int argc, const char **argv) {
        parsed = true;

        if (argc < 1) {
            return Error(Error::EType::NumArgs, "At least 1 argument is needed.");
        }

        // Drop program name.
        argc--;
        argv++;

        while (argc) {
            auto s = std::string_view(*argv);
            if (s.size() < 2 || s[0] != '-') {
                // Flags have ended, positional arguments from here on.
                break;
            }

            auto numMinuses = 1;
            if (s[1] == '-') {
                numMinuses++;
                if (s.size() == 2) {
                    // Argument is --, terminate flags.
                    argc--;
                    argv++;
                    break;
                }
            }
            argc--;
            argv++;

            s.remove_prefix(numMinuses);
            if (s[0] == '-' || s[0] == '=') {
                std::stringstream ss;
                ss << "Bad flag syntax: " << s;
                return Error(Error::EType::BadSyntax, ss.str());
            }

            // it's a flag, does it have a value?
            auto hasValue = false;
            std::string_view value{};

            for (size_t i = 1; i < s.size(); ++i) {
                if (s[i] == '=') {
                    value = s;
                    value.remove_prefix(i);
                    s.remove_suffix(s.size() - i);
                    hasValue = true;
                }
            }
            auto name = s;

            auto flag = flags.find(name);
            if (flag == flags.end()) {
                if (name == "help" || name == "h") {
                    // Special case for usage.
                    return Error(Error::EType::Help, "");
                }
                std::stringstream ss;
                ss << "Flag provided but not defined: " << name;
                return Error(Error::EType::UndefinedFlag, ss.str());
            }

            auto flagVal = flag->second;
            if (flagVal.isBool) {
                if (hasValue) {
                    auto err = flagVal.setFn(value);
                    if (err) {
                        std::stringstream ss;
                        ss << "Bad boolean value " << value << " for flag " << name << ": " << err->What();
                        return Error(Error::EType::BadValue, ss.str());
                    }
                } else {
                    auto err = flagVal.setFn("true");
                    if (err) {
                        std::stringstream ss;
                        ss << "Bad boolean flag " << name << ": " << err->What();
                        return Error(Error::EType::BadValue, ss.str());
                    }
                }
            } else {
                if (!hasValue && argc > 0) {
                    hasValue = true;
                    value = *argv;
                    argc++;
                    argv++;
                }
                if (!hasValue) {
                    std::stringstream ss;
                    ss << "Flag is missing a value: " << name;
                    return Error(Error::EType::MissingValue, ss.str());
                }
                auto err = flagVal.setFn(value);
                if (err) {
                    std::stringstream ss;
                    ss << "Bad value " << value << " for flag " << name << ": " << err->What();
                    return Error(Error::EType::BadValue, ss.str());
                }
            }
        }

        // Slurp up remaining positional arguments.
        while (argc) {
            args.push_back(*argv);
            argc--;
            argv++;
        }

        return {};
    }

    namespace detail {
        template<typename T>
        Flag::SetFn MakeSetFn(T &var);

        template<>
        Flag::SetFn MakeSetFn(int &var) {
            return [&](std::string_view s) {
                // TODO: convert to int
                var = 0;
                return std::optional<FlagError>{};
            };
        }

        template<>
        Flag::SetFn MakeSetFn(std::string& var) {
            return [&](std::string_view s) {
                var = s;
                return std::optional<FlagError>{};
            };
        }
    }// namespace detail

    template<typename T>
    void FlagSet::Var(T &var, std::string name, std::string usage) {
        auto flag = Flag{detail::MakeSetFn(var), usage, false};
        flags.insert({name, flag});
    }

    template<>
    void FlagSet::Var(bool &var, std::string name, std::string usage) {
        auto fn = [&](std::string_view s) {
          var = s.size() > 0;
          return std::optional<FlagError>{};
        };
        auto flag = Flag{fn, usage, true};
        flags.insert({name, flag});
    }

}// namespace flag