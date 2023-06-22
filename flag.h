#pragma once

#include <charconv>
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

        [[nodiscard]] inline EType Type() const { return type; }
        [[nodiscard]] inline std::string What() const { return message; }

    private:
        EType type;
        std::string message;
    };

    class FlagError {
    public:
        explicit FlagError(std::string message) : message(std::move(message)) {}

        [[nodiscard]] inline std::string What() const { return message; }

    private:
        std::string message;
    };

    struct Flag {
        using SetFn = std::function<std::optional<FlagError>(std::string_view)>;

        Flag(SetFn fn, std::string_view usage, bool isBool = false) : setFn(std::move(fn)), usage(usage), isBool(isBool) {}

        SetFn setFn;
        std::string_view usage{};
        bool isBool{false};
    };

    class FlagSet {
    public:
        /// Parses a command line.
        /// @returns an optional error if one occurred.
        [[nodiscard]] std::optional<Error> Parse(int argc, const char **argv);

        template<typename T>
        inline void Var(T &var, std::string_view name, std::string_view usage);

        template<typename T>
        inline void Var(std::optional<T> &var, std::string_view name, std::string_view usage);

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
                    value.remove_prefix(i + 1);
                    s.remove_suffix(s.size() - i);
                    hasValue = true;
                }
            }
            auto name = s;

            // Search for the flag name in the flags.
            auto flag = flags.find(name);
            if (flag == flags.end()) {
                // Not found in flags.
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
            args.emplace_back(*argv);
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
                auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), var);
                if (ec == std::errc::invalid_argument) {
                    return std::optional<FlagError>{FlagError("number is not an integer")};
                }
                if (ec == std::errc::result_out_of_range) {
                    return std::optional<FlagError>{FlagError("number is out of range")};
                }
                return std::optional<FlagError>{};
            };
        }

        template<>
        Flag::SetFn MakeSetFn(float &var) {
            return [&](std::string_view s) {
                std::stringstream ss;
                ss << s;
                ss >> var;
                if (ss.fail()) {
                    return std::optional<FlagError>{FlagError("number is not a float")};
                }
                return std::optional<FlagError>{};
            };
        }

        template<>
        Flag::SetFn MakeSetFn(double &var) {
            return [&](std::string_view s) {
              std::stringstream ss;
              ss << s;
              ss >> var;
              if (ss.fail()) {
                  return std::optional<FlagError>{FlagError("number is not a double")};
              }
              return std::optional<FlagError>{};
            };
        }

        template<>
        Flag::SetFn MakeSetFn(std::string &var) {
            return [&](std::string_view s) {
                var = s;
                return std::optional<FlagError>{};
            };
        }

        template<>
        Flag::SetFn MakeSetFn(bool &var) {
            static constexpr std::array<std::string_view, 4> trueValues{"true", "t", "yes", "y"};
            static constexpr std::array<std::string_view, 5> falseValues{"false", "f", "no", "n"};
            return [&](std::string_view s) -> std::optional<FlagError> {
                if (s.empty()) {
                    var = true;
                } else {
                    if (std::find(trueValues.begin(), trueValues.end(), s) != trueValues.end()) {
                        var = true;
                    } else if (std::find(falseValues.begin(), falseValues.end(), s) != falseValues.end()) {
                        var = false;
                    } else {
                        return FlagError("Unknown boolean value");
                    }
                }
                return {};
            };
        }

        template<typename T>
        Flag::SetFn MakeOptionalSetFn(std::optional<T>& var) {
            return [&](std::string_view s) {
                T tmp{};
                auto err = MakeSetFn(tmp)(s);
                if (err) {
                    return err;
                }
                var = tmp;
                return std::optional<FlagError>{};
            };
        }
    }// namespace detail

    template<typename T>
    void FlagSet::Var(T &var, std::string_view name, std::string_view usage) {
        auto flag = Flag{detail::MakeSetFn(var), usage, false};
        flags.insert({name, flag});
    }

    template<typename T>
    void FlagSet::Var(std::optional<T> &var, std::string_view name, std::string_view usage) {
        auto flag = Flag{detail::MakeOptionalSetFn(var), usage, false};
        flags.insert({name, flag});
    }

    template<>
    void FlagSet::Var(bool &var, std::string_view name, std::string_view usage) {
        auto flag = Flag{detail::MakeSetFn(var), usage, true};
        flags.insert({name, flag});
    }

    template<>
    void FlagSet::Var(std::optional<bool> &var, std::string_view name, std::string_view usage) {
        auto flag = Flag{detail::MakeOptionalSetFn(var), usage, true};
        flags.insert({name, flag});
    }

}// namespace flag