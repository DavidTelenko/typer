#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <argparse/argparse.hpp>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <random>
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>
#include <ranges>
#include <string>
#include <vector>

namespace tpr {

template <class T>
auto read_dictionary(const std::filesystem::path& path, uint64_t words,
                     const std::pmr::polymorphic_allocator<T>& allocator)
    -> std::optional<std::pmr::vector<std::pmr::basic_string<T>>> {
    using fstream = std::basic_fstream<T>;
    using string = std::pmr::basic_string<T>;
    using vector = std::pmr::vector<string>;

    fstream stream(path, std::ios::in);

    if (!stream) {
        return std::nullopt;
    }

    vector dictionary(allocator);
    dictionary.reserve(words);

    for (string buffer(allocator); std::getline(stream, buffer);) {
        dictionary.emplace_back(std::move(buffer));
    }

    return dictionary;
}

auto report_error(const auto& lhs, const auto& rhs) {
    if (lhs != rhs) {
        fmt::print(fg(fmt::terminal_color::red), "{}", rhs);
        return true;
    } else {
        fmt::print("{}", rhs);
        return false;
    }
}

}  // namespace tpr

int main(int argc, const char* argv[]) {
    using Char = char;
    using String = std::pmr::basic_string<Char>;
    using Strings = std::pmr::vector<String>;

    argparse::ArgumentParser program("typer", "0.0.1");

    program.add_description("Generate a typing test");

    program.add_argument("--min", "--min-length", "-u")
        .help("set the minimum length of a word can be '0' to ignore")
        .scan<'u', uint64_t>()
        .default_value(2ull);

    program.add_argument("--max", "--max-length", "-l")
        .help("set the maximum length of a word can be '0' to ignore")
        .scan<'u', uint64_t>()
        .default_value(0ull);

    program.add_argument("--amount", "--words-amount", "-a")
        .help("set words amount in test")
        .scan<'u', uint64_t>()
        .default_value(25ull);

    program.add_argument("--top", "-t")
        .help(
            "select n top words from your list (your file can contain 20k "
            "words but you only want 200 most frequent to appear in test)")
        .scan<'u', uint64_t>()
        .default_value(200ull);

    program.add_argument("--dictionary", "-d")
        .help("path to dictionary file with newline separated words")
        .default_value("res/20k.txt");

    program.add_argument("--dictionary-size", "-s")
        .help("size of words in dictionary file can, used to read file faster")
        .scan<'u', uint64_t>()
        .default_value(20'000ull);

    program.add_argument("--measure-units", "-m")
        .help("units of measure")
        .default_value("wpm")
        .choices("wpm", "cpm", "wps", "cps");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        fmt::print(fg(fmt::terminal_color::red), "{}\n", err.what());
        std::cout << program;
        return 1;
    }

    std::pmr::unsynchronized_pool_resource resource;

    const uint64_t amount = program.get<uint64_t>("--amount");

    if (not amount) {
        fmt::print(fg(fmt::terminal_color::red),
                   "--amount = 0, empty test generated\n");
        return 1;
    }

    const uint64_t top = program.get<uint64_t>("--top");

    if (not amount) {
        fmt::print(fg(fmt::terminal_color::red),
                   "--top = 0 no words selected for test\n");
        return 1;
    }

    const uint64_t min_length = program.get<uint64_t>("--min-length");
    const uint64_t max_length = program.get<uint64_t>("--max-length");

    if (min_length > max_length and max_length) {
        fmt::print(fg(fmt::terminal_color::red),
                   "--min-length must be less than --max-length\n");
        return 1;
    }

    const uint64_t dictionary_size = program.get<uint64_t>("-s");

    const auto dictionary_path_value = program.get("-d");
    const std::filesystem::path dictionary_path(dictionary_path_value);

    if (not std::filesystem::exists(dictionary_path)) {
        fmt::print(fg(fmt::terminal_color::red),
                   "provided --dictionary-path = \"{}\", does not exist\n",
                   dictionary_path_value);
        return 1;
    }

    const auto filter_function = [&max_length, &min_length](const auto& word) {
        return (word.size() <= max_length or not max_length) and
               (word.size() >= min_length or not min_length);
    };

    auto rng = std::mt19937{std::random_device{}()};

    tpr::read_dictionary<Char>(dictionary_path, dictionary_size, &resource)
        .and_then([&](const auto& dictionary) {
            using namespace std::chrono;
            auto filtered =                               //
                dictionary                                //
                | ranges::views::take(top)                //
                | ranges::views::filter(filter_function)  //
                | ranges::views::sample(amount, rng)      //
                | ranges::views::join(' ')                //
                | ranges::to<std::basic_string<Char>>();  //

            std::pmr::basic_string<Char> buffer(&resource);
            buffer.reserve(filtered.size());

            fmt::print("{}\n", filtered);

            Char c;
            std::cin >> c;
            const auto start_time = high_resolution_clock::now();
            std::getline(std::cin, buffer);
            const auto end_time = high_resolution_clock::now();

            const auto duration =
                duration_cast<microseconds>(end_time - start_time);

            size_t errors = 0;

            errors += tpr::report_error(filtered[0], c);
            errors += filtered.size() > (buffer.size() + 1)
                          ? filtered.size() - buffer.size() + 1
                          : (buffer.size() + 1) - filtered.size();

            errors = std::ranges::fold_left(
                ranges::views::zip(filtered | ranges::views::drop(1), buffer),
                errors, [](size_t errors, const auto& element) -> size_t {
                    return errors +
                           tpr::report_error(element.first, element.second);
                });

            fmt::println("\nErros: {}", errors);

            fmt::print(fg(fmt::terminal_color::yellow),
                       "You were typing: {} seconds",
                       duration_cast<milliseconds>(duration));

            return std::make_optional(dictionary);
        })
        .or_else([&dictionary_path, &resource] {
            fmt::print("Error occured: Could not read file \"{}\"",
                       dictionary_path.string());

            return std::make_optional(Strings(&resource));
        });

    return 0;
}
