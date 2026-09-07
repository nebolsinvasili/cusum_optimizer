#include "Utils.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace cusum {

RandomGenerator& RandomGenerator::getInstance() {
    // thread_local: у каждого потока (OpenMP-витка) — свой генератор, иначе
    // параллельный доступ к общему rng_ даёт гонку данных и смещает статистику
    // (систематическое занижение ARL на многопоточной симуляции).
    static thread_local RandomGenerator instance;
    return instance;
}

RandomGenerator::RandomGenerator() {
    rng_.seed(std::random_device{}());
}

double RandomGenerator::normal(double mean, double sigma) {
    return mean + sigma * normal_dist_(rng_);
}

void RandomGenerator::setSeed(unsigned int seed) {
    rng_.seed(seed);
}

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    file << content;
}

void deleteFile(const std::string& filename) {
    if (fileExists(filename)) {
        std::remove(filename.c_str());
    }
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string formatTime(double seconds) {
    if (seconds < 0) seconds = 0;
    int hours = static_cast<int>(seconds / 3600);
    int minutes = static_cast<int>((seconds - hours * 3600) / 60);
    int secs = static_cast<int>(seconds - hours * 3600 - minutes * 60);
    
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setw(2) << minutes << ":"
       << std::setw(2) << secs;
    return ss.str();
}

double roundTo(double value, int decimals) {
    double factor = std::pow(10.0, decimals);
    return std::round(value * factor) / factor;
}

bool isClose(double a, double b, double eps) {
    return std::abs(a - b) < eps;
}

double calculateMean(const std::vector<int>& data) {
    if (data.empty()) return 0.0;
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

double calculateStdDev(const std::vector<int>& data, double mean) {
    if (data.size() < 2) return 0.0;
    double sq_sum = std::accumulate(data.begin(), data.end(), 0.0,
        [mean](double acc, int val) {
            double diff = static_cast<double>(val) - mean;
            return acc + diff * diff;
        });
    return std::sqrt(sq_sum / (data.size() - 1));
}

// Линейная интерполяция перцентиля: позиция index = p/100 * (N-1)
// (индекс в 0-терминах), затем значения на соседних целых индексах
// интерполируются. Работает только на отсортированных данных.
double calculatePercentile(const std::vector<int>& data, double percentile) {
    if (data.empty()) return 0.0;
    if (percentile <= 0.0) return static_cast<double>(data.front());
    if (percentile >= 100.0) return static_cast<double>(data.back());
    
    double index = (percentile / 100.0) * (data.size() - 1);
    int lower = static_cast<int>(std::floor(index));
    int upper = static_cast<int>(std::ceil(index));
    
    if (lower == upper) return static_cast<double>(data[lower]);
    
    double fraction = index - lower;
    return static_cast<double>(data[lower]) + fraction * (data[upper] - data[lower]);
}

std::map<int, int> calculateHistogram(const std::vector<int>& data, int binSize) {
    std::map<int, int> histogram;
    for (int val : data) {
        int bin = (val / binSize) * binSize;
        histogram[bin]++;
    }
    return histogram;
}

} // namespace cusum