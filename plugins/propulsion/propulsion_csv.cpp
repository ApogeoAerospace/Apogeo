/** @file propulsion_csv.cpp
 * @brief Strict numeric CSV profile with transactional table construction.
 */
#include "propulsion_csv.h"

#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace propulsion {
namespace {
constexpr const char* HEADER =
    "ambient_pressure_pa,chamber_pressure_pa,mixture_ratio,thrust_n,isp_s,interpolation";

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r");
    if (begin == std::string::npos) return {};
    return value.substr(begin, value.find_last_not_of(" \t\r") - begin + 1);
}

[[noreturn]] void fail(const std::string& path, std::size_t line, const std::string& reason) {
    throw std::runtime_error(path + ":" + std::to_string(line) + ": " + reason);
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    for (;;) {
        const auto end = line.find(',', begin);
        fields.push_back(trim(line.substr(begin, end == std::string::npos ? end : end - begin)));
        if (end == std::string::npos) return fields;
        begin = end + 1;
    }
}

double number(const std::string& value, const std::string& name,
              const std::string& path, std::size_t line) {
    std::istringstream input(value);
    input.imbue(std::locale::classic());
    double result = 0.0;
    if (!(input >> result) || !std::isfinite(result)) {
        fail(path, line, name + " must be a finite number");
    }
    input >> std::ws;
    if (!input.eof()) fail(path, line, name + " contains trailing characters");
    return result;
}
} // namespace

EngineCurve read_engine_curve_csv(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail(path, 0, "cannot open CSV");
    EngineCurve candidate;
    std::string line;
    std::size_t line_number = 0;
    bool have_header = false;
    while (std::getline(input, line)) {
        ++line_number;
        if (line_number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        line = trim(line);
        if (line.empty()) continue;
        if (line.find('"') != std::string::npos) fail(path, line_number, "quoted fields are unsupported");
        if (!have_header) {
            if (line != HEADER) fail(path, line_number, "expected version-1 CSV header");
            have_header = true;
            continue;
        }
        const auto fields = split(line);
        if (fields.size() != 6) fail(path, line_number, "expected exactly 6 columns");
        const double pressure = number(fields[0], "ambient_pressure_pa", path, line_number);
        const double chamber = number(fields[1], "chamber_pressure_pa", path, line_number);
        const double ratio = number(fields[2], "mixture_ratio", path, line_number);
        const double thrust = number(fields[3], "thrust_n", path, line_number);
        const double isp = number(fields[4], "isp_s", path, line_number);
        if (pressure < 0 || chamber <= 0 || ratio <= 0 || thrust < 0 || isp <= 0) {
            fail(path, line_number, "pressure/thrust must be nonnegative; chamber/mixture/ISP must be positive");
        }
        if (fields[5] != "linear") fail(path, line_number, "unsupported interpolation (expected linear)");
        if (!candidate.samples.empty()) {
            if (pressure <= candidate.samples.back().ambient_pressure_pa) {
                fail(path, line_number, "ambient pressures must be strictly increasing");
            }
            if (chamber != candidate.chamber_pressure_pa || ratio != candidate.mixture_ratio) {
                fail(path, line_number, "chamber pressure and mixture ratio must remain constant");
            }
        }
        candidate.chamber_pressure_pa = chamber;
        candidate.mixture_ratio = ratio;
        candidate.samples.push_back({pressure, thrust, isp});
    }
    if (input.bad() || (!input.eof() && input.fail())) fail(path, line_number, "CSV read failed");
    if (!have_header || candidate.samples.size() < 2) fail(path, line_number, "expected header and at least two data rows");
    return candidate;
}
} // namespace propulsion
