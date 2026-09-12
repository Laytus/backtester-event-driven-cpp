#include "backtester/data/csv_bar_reader.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <stdexcept>
#include <optional>
#include <exception>
#include <limits>

namespace backtester::data {

namespace {

void validate_header(const std::string& line) {
    constexpr auto expected_header = "session_date,symbol,open,high,low,close,volume";

    if (line != expected_header) {
        throw std::invalid_argument(
            "Header must be exactly: session_date,symbol,open,high,low,close,volume"
        );
    }
}

std::vector<std::string> split_row(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream{line};

    std::string field;

    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }

    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }

    return fields;
}

bool contains_only_digits(const std::string& value) {
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    return true;
}

market::Bar::SessionDate parse_date(const std::string& value) {
    if (
        value.size() != 10 ||
        value[4] != '-' ||
        value[7] != '-'
    ) {
        throw std::invalid_argument(
            "Session date must use YYYY-MM-DD format"
        );
    }

    const std::string year_str = value.substr(0, 4);
    const std::string month_str = value.substr(5, 2);
    const std::string day_str = value.substr(8, 2);

    if (
        !contains_only_digits(year_str) ||
        !contains_only_digits(month_str) ||
        !contains_only_digits(day_str)
    ) {
        throw std::invalid_argument(
            "Session date must contain numeric year, month, and day"
        );
    }

    std::size_t year_chars{};
    std::size_t month_chars{};
    std::size_t day_chars{};

    int year{};
    unsigned long month{};
    unsigned long day{};

    try {
        year = std::stoi(year_str, &year_chars);
        month = std::stoul(month_str, &month_chars);
        day = std::stoul(day_str, &day_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument(
            "Session date contains invalid numeric components"
        );
    }

    if (
        year_chars != year_str.size() ||
        month_chars != month_str.size() ||
        day_chars != day_str.size()
    ) {
        throw std::invalid_argument(
            "Session date contains invalid numeric components"
        );
    }

    const market::Bar::SessionDate date{
        std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)}
    };

    if (!date.ok()) {
        throw std::invalid_argument(
            "Session date is not a valid calendar date"
        );
    }

    return date;
}

double parse_price(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument(
            "Price field cannot be empty"
        );
    }

    std::size_t parsed_chars{};
    double result{};

    try {
        result = std::stod(value, &parsed_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument(
            "Price field must contain a valid numeric value"
        );
    }

    if (parsed_chars != value.size()) {
        throw std::invalid_argument(
            "Price field contains invalid trailing characters"
        );
    }

    return result;
}

std::uint64_t parse_volume(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument(
            "Volume field cannot be empty"
        );
    }

    if (value.front() == '-') {
        throw std::invalid_argument(
            "Volume cannot be negative"
        );
    }

    std::size_t parsed_chars{};
    unsigned long long result{};

    try {
        result = std::stoull(value, &parsed_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument(
            "Volume field must contain a valid unsigned integer"
        );
    }

    if (parsed_chars != value.size()) {
        throw std::invalid_argument(
            "Volume field must contain an integer value"
        );
    }

    if (result > std::numeric_limits<std::uint64_t>::max()) {
        throw std::invalid_argument(
            "Volume exceeds the supported range"
        );
    }

    return static_cast<std::uint64_t>(result);
}

std::string make_error(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& reason
) {
    return path.string()
        + ":"
        + std::to_string(line_number)
        + ": "
        + reason;
}

} // namespace

std::vector<market::Bar> CsvBarReader::read(
    const std::filesystem::path& path
) {
    std::vector<market::Bar> bar_vector;
    std::size_t line_number = 1;

    std::optional<market::Bar::SessionDate> previous_date;
    std::optional<std::string> expected_symbol;

    std::ifstream file{path};

    if (!file.is_open()) {
        throw std::runtime_error(
            path.string() + ": Cannot open file"
        );
    }

    std::string line;

    if (!std::getline(file, line)) {
        throw std::runtime_error(
            path.string() + ": File is empty"
        );
    }

    try {
        validate_header(line);
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error(
            make_error(path, line_number, error.what())
        );
    }

    while (std::getline(file, line)) {
        ++line_number;

        const std::vector<std::string> line_data = split_row(line);

        if (line_data.size() != 7) {
            throw std::runtime_error(
                make_error(
                    path,
                    line_number,
                    "Row must contain exactly 7 fields"
                )
            );
        }

        try {
            const auto current_date = parse_date(line_data[0]);

            const double open = parse_price(line_data[2]);
            const double high = parse_price(line_data[3]);
            const double low = parse_price(line_data[4]);
            const double close = parse_price(line_data[5]);
            const std::uint64_t volume = parse_volume(line_data[6]);

            if (previous_date.has_value()) {
                if (current_date == *previous_date) {
                    throw std::runtime_error(
                        make_error(
                            path,
                            line_number,
                            "Duplicate session date"
                        )
                    );
                }

                if (current_date < *previous_date) {
                    throw std::runtime_error(
                        make_error(
                            path,
                            line_number,
                            "Session dates must be strictly increasing"
                        )
                    );
                }
            }

            if (
                expected_symbol.has_value() &&
                line_data[1] != expected_symbol
            ) {
                throw std::runtime_error(
                    make_error(
                        path,
                        line_number,
                        "Expected symbol '" + *expected_symbol
                            + "' but found '" + line_data[1] + "'"
                    )
                );
            }

            bar_vector.emplace_back(
                current_date,
                line_data[1],
                open,
                high,
                low,
                close,
                volume
            );

            if (!expected_symbol.has_value()) {
                expected_symbol = line_data[1];
            }

            previous_date = current_date;

        } catch (const std::invalid_argument& error) {
            throw std::runtime_error(
                make_error(path, line_number, error.what())
            );
        }
    }

    if (file.bad()) {
        throw std::runtime_error(
            path.string() + ": I/O error while reading file"
        );
    }

    return bar_vector;
}

}  // namespace backtester::data