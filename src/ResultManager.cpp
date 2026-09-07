// src/ResultManager.cpp
// [ВНИМАНИЕ] Этот файл НЕ входит в сборку (не указан в Makefile).
// Класс дублирует часть логики Calculator::saveBestPairs/saveFinalResults.
// TODO: либо интегрировать ResultManager в конвейер Calculator, либо удалить,
// чтобы не поддерживать две параллельные реализации сохранения результатов.
#include "ResultManager.hpp"
#include "Utils.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace cusum {

ResultManager::ResultManager(const Config& config) : config_(config) {}

void ResultManager::addResult(double k, double H, double ARL) {
    Result result;
    result.k = k;
    result.H = H;
    result.ARL = ARL;
    result.deviation = std::abs(ARL - config_.target_ARL);
    results_.push_back(result);
}

void ResultManager::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file for writing: " << filename << std::endl;
        return;
    }
    
    file << "k,H,ARL\n";
    for (const auto& r : results_) {
        file << r.k << "," << r.H << "," << r.ARL << "\n";
    }
    file.close();
}

void ResultManager::loadFromFile(const std::string& filename) {
    if (!fileExists(filename)) {
        return;
    }
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        return;
    }
    
    results_.clear();
    std::string line;
    bool firstLine = true;
    
    while (std::getline(file, line)) {
        if (firstLine) {
            firstLine = false;
            continue; // Skip header
        }
        auto parts = split(trim(line), ',');
        if (parts.size() >= 3) {
            Result result;
            result.k = std::stod(parts[0]);
            result.H = std::stod(parts[1]);
            result.ARL = std::stod(parts[2]);
            result.deviation = std::abs(result.ARL - config_.target_ARL);
            results_.push_back(result);
        }
    }
    file.close();
}

void ResultManager::saveFinalResults() {
    if (results_.empty()) {
        std::cout << "No results to save." << std::endl;
        return;
    }
    
    // Save to final file
    saveToFile(config_.final_file);
    
    // Save best pairs
    saveBestPairs(config_.top_n);
}

void ResultManager::saveBestPairs(int n) {
    if (results_.empty()) return;
    
    auto sorted = results_;
    std::sort(sorted.begin(), sorted.end(),
              [](const Result& a, const Result& b) {
                  return a.deviation < b.deviation;
              });
    
    std::ofstream file(config_.best_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open best file for writing: " << config_.best_file << std::endl;
        return;
    }
    
    file << "k,H,ARL,Deviation\n";
    int count = std::min(n, static_cast<int>(sorted.size()));
    for (int i = 0; i < count; ++i) {
        const auto& r = sorted[i];
        file << r.k << "," << r.H << "," << r.ARL << "," << r.deviation << "\n";
    }
    file.close();
}

std::vector<Result> ResultManager::getBestPairs(int n) const {
    auto sorted = results_;
    std::sort(sorted.begin(), sorted.end(),
              [](const Result& a, const Result& b) {
                  return a.deviation < b.deviation;
              });
    
    int count = std::min(n, static_cast<int>(sorted.size()));
    return std::vector<Result>(sorted.begin(), sorted.begin() + count);
}

} // namespace cusum