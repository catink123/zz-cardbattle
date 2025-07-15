#include "../include/utils.hpp"
#include <iomanip>
#include <sstream>
#include <random>

namespace cardbattle {

std::string TimePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto tm = *std::gmtime(&time_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::chrono::system_clock::time_point StringToTimePoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        // Return epoch time if parsing fails
        return std::chrono::system_clock::from_time_t(0);
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string GenerateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex_chars = "0123456789abcdef";
    
    std::string id;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            id += '-';
        }
        id += hex_chars[dis(gen)];
    }
    return id;
}

} // namespace cardbattle 