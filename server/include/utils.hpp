#pragma once

#include <string>
#include <chrono>
#include <sstream>
#include <random>

namespace cardbattle {

// Helper function to convert time_point to string
std::string TimePointToString(const std::chrono::system_clock::time_point& tp);

// Helper function to convert string to time_point
std::chrono::system_clock::time_point StringToTimePoint(const std::string& str);

// Helper function to generate random IDs
std::string GenerateId();

} // namespace cardbattle 