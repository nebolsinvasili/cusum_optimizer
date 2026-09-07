// ============================================================================
// Calculator.cpp — оркестрация расчёта ARL.
//
// Ключевые механики:
//  * Сетка (k, H) перебирается вложенными циклами по отсортированным
//    k_values / H_values. Каждая пара уникально идентифицируется uint64-ключом
//    (makeKey), что позволяет быстро пропускать уже посчитанные комбинации.
//  * Прогресс пишется построчно в temp_file сразу после расчёта пары
//    (flush на каждую строку) — это и есть главная защита от потери данных.
//  * Resume работает в два уровня: (1) completed_combinations_ из temp_file —
//    какие пары вообще посчитаны; (2) checkpoint_file — позиция последней
//    обработанной пары, чтобы не перебирать сетку с самого начала вхолостую.
//  * Проверка «isCombinationCompleted» всё равно важна в resume: если расчёт
//    упал между записью checkpoint и записью строки temp-файла, пара может
//    считаться «пройденной», но отсутствовать в файле.
// ============================================================================

#include "Calculator.hpp"
#include "Simulator.hpp"
#include "ProgressBar.hpp"
#include "Utils.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <thread>
#include <iomanip>

namespace cusum {

Calculator::Calculator(const Config& config) : config_(config) {
    results_.reserve(10000);
    completed_combinations_.reserve(10000);
}

// Упаковка двух double в один uint64-ключ:
//   k, H округляются до 3 знаков, сдвигаются на +10000 (чтобы уйти от
//   отрицательных хранимых значений для диапазона [3.0, 7.0]) и умножаются
//   на 1000. Старшие 32 бита ключа — k, младшие 32 — H.
uint64_t Calculator::makeKey(double k, double H) const {
    uint32_t k_int = static_cast<uint32_t>(roundTo(k, 3) * 1000 + 10000);
    uint32_t H_int = static_cast<uint32_t>(roundTo(H, 3) * 1000 + 10000);
    return (static_cast<uint64_t>(k_int) << 32) | H_int;
}
// FIXME: +10000 и *1000 жёстко завязаны на диапазон k,H из [3.0, 7.0].
// При расширении сетки (например, k < -10 или H > 20) ключ схлопнется
// (50019/50020 могут «столкнуться» в одну пару). Лучше строить ключ из
// канонической строки "k:H" или из битового представления double.

void Calculator::run(const Config& config) {
    config_ = config;
    
    std::ofstream tempFile;
    bool fileOpened = false;
    
    if (config_.resume) {
        loadCompletedCombinations();
        log("Resume mode: loaded " + std::to_string(completed_combinations_.size()) + " completed combinations");
        tempFile.open(config_.temp_file, std::ios::app);
        if (tempFile.is_open()) {
            fileOpened = true;
        }
    } else {
        completed_combinations_.clear();
        results_.clear();
        if (fileExists(config_.temp_file)) {
            deleteFile(config_.temp_file);
            log("Deleted old temp file for fresh start");
        }
        if (fileExists(config_.checkpoint_file)) {
            deleteFile(config_.checkpoint_file);
            log("Deleted old checkpoint file for fresh start");
        }
        tempFile.open(config_.temp_file);
        if (tempFile.is_open()) {
            fileOpened = true;
            tempFile << "k,H,ARL\n";
        }
        log("Fresh start: cleared all previous data");
    }
    
    if (!fileOpened) {
        log("ERROR: Cannot open temp file for writing");
        return;
    }
    
    ChartType chart = (config_.chart_type == "SR") ? ChartType::SR : ChartType::SN;
    
    auto k_values = config_.k_values;
    auto H_values = config_.H_values;
    
    std::sort(k_values.begin(), k_values.end());
    std::sort(H_values.begin(), H_values.end());
    
    int totalCombinations = k_values.size() * H_values.size();
    int completedCount = completed_combinations_.size();
    int remaining = totalCombinations - completedCount;
    
    log("Total combinations: " + std::to_string(totalCombinations));
    log("Already completed: " + std::to_string(completedCount));
    log("Remaining: " + std::to_string(remaining));
    
    if (remaining == 0) {
        log("All combinations already calculated.");
        tempFile.close();
        saveFinalResults();
        printTopResults(config_.top_n);
        return;
    }
    
    bool startProcessing = false;
    double lastK = 0, lastH = 0;
    
    if (config_.resume && fileExists(config_.checkpoint_file)) {
        loadCheckpoint(lastK, lastH);
        log("Resuming from checkpoint: k=" + std::to_string(lastK) + ", H=" + std::to_string(lastH));
    }
    
    ProgressBar progress(remaining, 50);
    auto startTime = std::chrono::steady_clock::now();
    processed_ = 0;
    
    bool hasBest = false;
    double bestK = 0.0, bestH = 0.0, bestARL = 0.0, bestDev = 1e9;
    bool earlyStopped = false;
    
    int updateCounter = 0;
    const int UPDATE_INTERVAL = config_.update_interval;
    
    for (double k : k_values) {
        if (earlyStopped) break;
        
        for (double H : H_values) {
            if (earlyStopped) break;
            
            // startProcessing: при resume пропускаем сетку до контрольной точки
            // (пары в checkpoint), чтобы не перебирать всё заново вхолостую.
            // В fresh-режиме всегда начинаем с первой пары.
            if (!startProcessing) {
                if (config_.resume && isClose(k, lastK) && isClose(H, lastH)) {
                    startProcessing = true;
                    log("Started processing from k=" + std::to_string(k) + ", H=" + std::to_string(H));
                }
                if (!config_.resume && k == k_values.front() && H == H_values.front()) {
                    startProcessing = true;
                }
                if (!startProcessing) continue;
            }
            
            if (isCombinationCompleted(k, H)) {
                continue;
            }
            
            processed_++;
            
            try {
                double ARL = calculateARLParallel(k, H, config_.simulations, config_.n, 
                                                  config_.max_iter, config_.n_cores, chart);
                
                tempFile << std::fixed << std::setprecision(6) 
                         << k << "," << H << "," << ARL << "\n";
                tempFile.flush();
                
                ResultEntry entry;
                entry.k = k;
                entry.H = H;
                entry.ARL = ARL;
                entry.key = makeKey(k, H);
                results_.push_back(entry);
                completed_combinations_.insert(entry.key);
                
                double dev = std::abs(ARL - config_.target_ARL);
                if (dev < bestDev) {
                    bestDev = dev;
                    bestK = k;
                    bestH = H;
                    bestARL = ARL;
                    hasBest = true;
                    progress.setBestResult(bestK, bestH, bestARL, config_.target_ARL);
                    
                    if (config_.tolerance >= 0 && bestDev <= config_.tolerance) {
                        earlyStopped = true;
                        progress.setEarlyStop(true);
                        log("Early stopping: found solution within tolerance " + 
                            std::to_string(config_.tolerance) + 
                            " (deviation: " + std::to_string(bestDev) + ")");
                        
                        tempFile.close();
                        saveFinalResults();
                        
                        auto endTime = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
                        progress.finish("Early stop! Time: " + formatTime(elapsed));
                        
                        log("Early stopped after " + std::to_string(processed_) + " combinations");
                        log("Best result: k=" + std::to_string(bestK) + ", H=" + std::to_string(bestH) + 
                            ", ARL=" + std::to_string(bestARL) + ", deviation=" + std::to_string(bestDev));
                        
                        printTopResults(config_.top_n);
                        return;
                    }
                }
                
                updateCounter++;
                if (updateCounter >= UPDATE_INTERVAL || processed_ == remaining) {
                    updateCounter = 0;
                    std::string msg = "k=" + std::to_string(k).substr(0, 4) + 
                                     ", H=" + std::to_string(H).substr(0, 4);
                    progress.update(processed_, msg, ARL);
                }
                
                if (processed_ % config_.checkpoint_interval == 0) {
                    saveCheckpoint(k, H);
                }
                
            } catch (const std::exception& e) {
                logError(k, H, e.what());
            }
        }
    }
    
    tempFile.close();
    progress.update(processed_, "Finalizing...", -1);
    
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    
    progress.finish("Complete! Time: " + formatTime(elapsed));
    
    log("Calculation completed: " + std::to_string(processed_) + " combinations");
    log("Total time: " + formatTime(elapsed));
    if (hasBest) {
        log("Best result: k=" + std::to_string(bestK) + ", H=" + std::to_string(bestH) + 
            ", ARL=" + std::to_string(bestARL) + ", deviation=" + std::to_string(bestDev));
    }
    
    saveFinalResults();
    printTopResults(config_.top_n);
}

void Calculator::loadCompletedCombinations() {
    completed_combinations_.clear();
    results_.clear();
    
    if (!fileExists(config_.temp_file)) {
        return;
    }
    
    std::ifstream file(config_.temp_file);
    if (file.peek() == std::ifstream::traits_type::eof()) {
        file.close();
        deleteFile(config_.temp_file);
        return;
    }
    
    file.clear();
    file.seekg(0);
    
    std::string line;
    int count = 0;
    bool firstLine = true;
    
    completed_combinations_.reserve(10000);
    results_.reserve(10000);
    
    while (std::getline(file, line)) {
        if (firstLine) {
            firstLine = false;
            continue;
        }
        
        auto parts = split(trim(line), ',');
        if (parts.size() >= 3) {
            try {
                double k = std::stod(parts[0]);
                double H = std::stod(parts[1]);
                double ARL = std::stod(parts[2]);
                
                ResultEntry entry;
                entry.k = k;
                entry.H = H;
                entry.ARL = ARL;
                entry.key = makeKey(k, H);
                
                completed_combinations_.insert(entry.key);
                results_.push_back(entry);
                count++;
            } catch (...) {
                // skip invalid lines
            }
        }
    }
    file.close();
    
    if (count > 0) {
        log("Loaded " + std::to_string(count) + " completed combinations from temp file");
    } else {
        deleteFile(config_.temp_file);
    }
}

bool Calculator::isCombinationCompleted(double k, double H) const {
    uint64_t key = makeKey(k, H);
    return completed_combinations_.find(key) != completed_combinations_.end();
}

void Calculator::saveCheckpoint(double k, double H) {
    std::ofstream file(config_.checkpoint_file);
    if (file.is_open()) {
        file << std::fixed << std::setprecision(6) << k << "," << H << "\n";
        file.close();
    }
}

void Calculator::loadCheckpoint(double& k, double& H) {
    if (fileExists(config_.checkpoint_file)) {
        std::string content = readFile(config_.checkpoint_file);
        auto parts = split(trim(content), ',');
        if (parts.size() >= 2) {
            k = std::stod(parts[0]);
            H = std::stod(parts[1]);
        }
    }
}

void Calculator::saveFinalResults() {
    if (results_.empty()) {
        log("No results to save");
        return;
    }
    
    if (fileExists(config_.temp_file)) {
        std::ifstream src(config_.temp_file);
        std::ofstream dst(config_.final_file);
        if (src.is_open() && dst.is_open()) {
            dst << src.rdbuf();
            dst.close();
            src.close();
            log("Copied temp file to " + config_.final_file);
            saveBestPairs();
            return;
        }
    }
    
    std::ofstream file(config_.final_file);
    if (file.is_open()) {
        file << "k,H,ARL\n";
        for (const auto& r : results_) {
            file << std::fixed << std::setprecision(6) 
                 << r.k << "," << r.H << "," << r.ARL << "\n";
        }
        file.close();
        log("Saved " + std::to_string(results_.size()) + " results to " + config_.final_file);
    }
    
    saveBestPairs();
}

void Calculator::saveBestPairs() {
    if (results_.empty()) return;
    
    std::vector<std::tuple<double, double, double, double>> bestPairs;
    bestPairs.reserve(results_.size());
    
    for (const auto& r : results_) {
        double dev = std::abs(r.ARL - config_.target_ARL);
        bestPairs.push_back({r.k, r.H, r.ARL, dev});
    }
    
    int n = std::min(config_.top_n, static_cast<int>(bestPairs.size()));
    
    std::nth_element(bestPairs.begin(), 
                     bestPairs.begin() + n,
                     bestPairs.end(),
                     [](const auto& a, const auto& b) {
                         return std::get<3>(a) < std::get<3>(b);
                     });
    
    std::sort(bestPairs.begin(), bestPairs.begin() + n,
              [](const auto& a, const auto& b) {
                  return std::get<3>(a) < std::get<3>(b);
              });
    
    std::ofstream bestFile(config_.best_file);
    if (bestFile.is_open()) {
        bestFile << "k,H,ARL,Deviation\n";
        for (int i = 0; i < n; ++i) {
            const auto& r = bestPairs[i];
            bestFile << std::fixed << std::setprecision(6)
                     << std::get<0>(r) << "," 
                     << std::get<1>(r) << "," 
                     << std::get<2>(r) << "," 
                     << std::get<3>(r) << "\n";
        }
        bestFile.close();
    }
}

void Calculator::printTopResults(int n) const {
    if (results_.empty()) {
        std::cout << "\nNo results available.\n";
        return;
    }
    
    std::vector<std::tuple<double, double, double, double>> withDeviation;
    withDeviation.reserve(results_.size());
    
    for (const auto& r : results_) {
        double dev = std::abs(r.ARL - config_.target_ARL);
        withDeviation.push_back({r.k, r.H, r.ARL, dev});
    }
    
    int count = std::min(n, static_cast<int>(withDeviation.size()));
    
    std::nth_element(withDeviation.begin(), 
                     withDeviation.begin() + count,
                     withDeviation.end(),
                     [](const auto& a, const auto& b) {
                         return std::get<3>(a) < std::get<3>(b);
                     });
    
    std::sort(withDeviation.begin(), withDeviation.begin() + count,
              [](const auto& a, const auto& b) {
                  return std::get<3>(a) < std::get<3>(b);
              });
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "BEST PAIR (k, h):\n";
    std::cout << std::string(70, '=') << "\n";
    const auto& best = withDeviation[0];
    std::cout << "  k = " << std::get<0>(best) << "\n";
    std::cout << "  h = " << std::get<1>(best) << "\n";
    std::cout << "  ARL = " << std::get<2>(best) << " (deviation: " << std::get<3>(best) << ")\n";
    std::cout << std::string(70, '=') << "\n";
    
    std::cout << "\nTOP " << n << " BEST PAIRS:\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "  # |    k    |    h    |    ARL    | Deviation\n";
    std::cout << std::string(70, '-') << "\n";
    
    for (int i = 0; i < count; ++i) {
        const auto& r = withDeviation[i];
        printf(" %2d | %7.3f | %7.3f | %9.2f | %9.2f\n",
               i+1, std::get<0>(r), std::get<1>(r), std::get<2>(r), std::get<3>(r));
    }
    std::cout << std::string(70, '=') << "\n";
}

void Calculator::log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::ctime(&time);
    timestamp.pop_back();
    
    std::ofstream file(config_.log_file, std::ios::app);
    if (file.is_open()) {
        file << "[" << timestamp << "] " << message << "\n";
        file.close();
    }
}

void Calculator::logError(double k, double H, const std::string& error) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::ctime(&time);
    timestamp.pop_back();
    
    std::ofstream file(config_.error_file, std::ios::app);
    if (file.is_open()) {
        file << "[" << timestamp << "] k=" << k << ", H=" << H << ", error=" << error << "\n";
        file.close();
    }
}

void Calculator::resume(const Config& config) {
    config_ = config;
    run(config);
}

} // namespace cusum