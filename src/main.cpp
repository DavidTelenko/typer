#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <argparse/argparse.hpp>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <random>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/sample.hpp>
#include <range/v3/view/take.hpp>
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

auto forward_draw() {}

}  // namespace tpr

int main(int argc, char* argv[]) {
    using Char = char;
    using String = std::pmr::basic_string<Char>;
    using Strings = std::pmr::vector<String>;

    argparse::ArgumentParser program("typer", "0.0.1");

    program.add_description("Generate a typing test");

    program.add_argument("--min", "--min-length", "-u")
        .help("set the minimum length of a word can be '0' to ignore")
        .scan<'u', uint64_t>()
        .default_value(2);

    program.add_argument("--max", "--max-length", "-l")
        .help("set the maximum length of a word can be '0' to ignore")
        .scan<'u', uint64_t>()
        .default_value(0);

    program.add_argument("--amount", "--words-amount", "-a")
        .help("set words amount in test")
        .scan<'u', uint64_t>()
        .default_value(25);

    program.add_argument("--top", "-t")
        .help("select n top words from your list (your file can contain 20k words but you only want 200 most frequent to appear in test)")
        .scan<'u', uint64_t>()
        .default_value(200);

    program.add_argument("--dictionary", "-d")
        .help("path to dictionary file with newline separated words")
        .default_value("res/20k.txt");

    program.add_argument("--dictionary-size", "-s")
        .help("size of words in dictionary file can, used to read file faster")
        .scan<'u', uint64_t>()
        .default_value(20'000);

    program.add_argument("--measure-units", "-m")
        .help("units of measure")
        .default_value("wpm");


    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        fmt::print(fg(fmt::terminal_color::red), "{}\n", err.what());
        std::cout << program;
        return 1;
    }

    std::pmr::unsynchronized_pool_resource resource;

    const uint64_t amount = program.get<uint64_t>("--amount");
    const uint64_t top = program.get<uint64_t>("--top");
    const uint64_t min_length = program.get<uint64_t>("--min_length");
    const uint64_t max_length = program.get<uint64_t>("--max_length");
    const uint64_t dictionary_size = program.get<uint64_t>("-s");

    const std::filesystem::path dictionary_path("res/20k.txt");

    const auto filter_function = [&max_length, &min_length](const auto& word) {
        return (word.size() <= max_length or not max_length) and
               (word.size() >= min_length or not min_length);
    };

    auto rng = std::mt19937{std::random_device{}()};

    tpr::read_dictionary<Char>(dictionary_path, dictionary_size, &resource)
        .and_then([&filter_function, &rng, &top, &amount](const auto& dictionary) {
            auto filtered =                               //
                dictionary                                //
                | ranges::views::take(top)                //
                | ranges::views::filter(filter_function)  //
                | ranges::views::sample(amount, rng);     //

            for (const auto& word : filtered) {
                fmt::print("{} ", word);
            }

            return std::make_optional(dictionary);
        })
        .or_else([&dictionary_path, &resource] {
            fmt::print("Error occured: Could not read file \"{}\"",
                       dictionary_path.string());

            return std::make_optional(Strings(&resource));
        });

    return 0;
}
