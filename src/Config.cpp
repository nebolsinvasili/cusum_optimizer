// ============================================================================
// Config.cpp — загрузка конфигурации из JSON и командной строки.
//
// Логика приоритетов:
//  1. Поля по умолчанию (struct Config).
//  2. Загрузка JSON (--config) — перезаписывает только присутствующие ключи.
//  3. Отдельные CLI-опции — перезаписывают соответствующие поля.
//
// Особенности:
//  * n_cores <= 0 -> автоопределение по числу аппаратных потоков.
//  * Пустые k_values/H_values -> диапазон [3.0, 7.0] шагом 0.1 (один и тот же
//    для обеих осей). COUNT: 41 значение на ось, 41*41 = 1681 комбинаций.
// ============================================================================

#include "Config.hpp"
#include "Utils.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace cusum {

namespace {

// Приводит тип карты к каноническому виду: "SN" или "SR".
// Неизвестные значения -> предупреждение в stderr и "SN".
std::string normalizeChartType(const std::string& raw) {
    std::string upper = raw;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    if (upper == "SR") return "SR";
    if (upper != "SN") {
        std::cerr << "Warning: unknown chart type \"" << raw
                  << "\", using default \"SN\"" << std::endl;
    }
    return "SN";
}

} // namespace

Config Config::loadFromFile(const std::string& filename) {
    Config config;
    
    if (!fileExists(filename)) {
        std::cerr << "Warning: Config file not found: " << filename << std::endl;
        return config;
    }
    
    try {
        std::string content = readFile(filename);
        auto json = nlohmann::json::parse(content);
        
        if (json.contains("simulations")) config.simulations = json["simulations"];
        if (json.contains("n")) config.n = json["n"];
        if (json.contains("max_iter")) config.max_iter = json["max_iter"];
        if (json.contains("target_ARL")) config.target_ARL = json["target_ARL"];
        if (json.contains("n_cores")) config.n_cores = json["n_cores"];
        if (json.contains("checkpoint_interval")) config.checkpoint_interval = json["checkpoint_interval"];
        if (json.contains("top_n")) config.top_n = json["top_n"];
        if (json.contains("tolerance")) config.tolerance = json["tolerance"];
        if (json.contains("update_interval")) config.update_interval = json["update_interval"];
        
        if (json.contains("k_values")) {
            config.k_values = json["k_values"].get<std::vector<double>>();
        }
        if (json.contains("H_values")) {
            config.H_values = json["H_values"].get<std::vector<double>>();
        }
        if (json.contains("chart_type")) {
            std::string ct = json["chart_type"].get<std::string>();
            config.chart_type = normalizeChartType(ct);
        }
        
        if (json.contains("temp_file")) config.temp_file = json["temp_file"];
        if (json.contains("checkpoint_file")) config.checkpoint_file = json["checkpoint_file"];
        if (json.contains("final_file")) config.final_file = json["final_file"];
        if (json.contains("best_file")) config.best_file = json["best_file"];
        if (json.contains("log_file")) config.log_file = json["log_file"];
        if (json.contains("error_file")) config.error_file = json["error_file"];
        if (json.contains("resume")) config.resume = json["resume"];
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
    }
    
    if (config.n_cores <= 0) {
        config.n_cores = std::thread::hardware_concurrency();
        if (config.n_cores <= 0) config.n_cores = 4;
    }
    
    return config;
}

Config Config::loadFromArgs(int argc, char** argv) {
    Config config;
    config.resume = false;
    config.tolerance = -1.0;
    
    bool kExplicit = false;
    bool hExplicit = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--config" && i + 1 < argc) {
            Config fileConfig = loadFromFile(argv[++i]);
            config = fileConfig;
            config.resume = false;
            // Считаем сетку явно заданной, если в конфиге были ключи k/H_values.
            kExplicit = !config.k_values.empty();
            hExplicit = !config.H_values.empty();
        }
        else if (arg == "--simulations" && i + 1 < argc) {
            config.simulations = std::stoi(argv[++i]);
        }
        else if (arg == "--n" && i + 1 < argc) {
            config.n = std::stoi(argv[++i]);
        }
        else if (arg == "--max_iter" && i + 1 < argc) {
            config.max_iter = std::stoi(argv[++i]);
        }
        else if (arg == "--target_ARL" && i + 1 < argc) {
            config.target_ARL = std::stod(argv[++i]);
        }
        else if (arg == "--chart" && i + 1 < argc) {
            config.chart_type = normalizeChartType(argv[++i]);
        }
        else if (arg == "--cores" && i + 1 < argc) {
            config.n_cores = std::stoi(argv[++i]);
        }
        else if (arg == "--tolerance" && i + 1 < argc) {
            config.tolerance = std::stod(argv[++i]);
        }
        else if (arg == "--update_interval" && i + 1 < argc) {
            config.update_interval = std::stoi(argv[++i]);
        }
        else if (arg == "--k_start" && i + 1 < argc && i + 2 < argc) {
            double start = std::stod(argv[++i]);
            double end = std::stod(argv[++i]);
            double step = 0.1;
            // Эвристика: третий аргумент (step) считаем заданным, только если
            // он не начинается с '-' (иначе это следующая CLI-опция).
            // TODO: хрупко — отрицательный шаг (например --k_start 3 7 -0.1)
            // будет принят за опцию и не распознан; для отладки пригодится явная
            // опция --k_step.
            if (i + 1 < argc && argv[i+1][0] != '-') {
                step = std::stod(argv[++i]);
            }
            config.k_values.clear();
            kExplicit = true;
            for (double v = start; v <= end + 1e-9; v += step) {
                config.k_values.push_back(roundTo(v, 3));
            }
        }
        else if (arg == "--H_start" && i + 1 < argc && i + 2 < argc) {
            double start = std::stod(argv[++i]);
            double end = std::stod(argv[++i]);
            double step = 0.1;
            if (i + 1 < argc && argv[i+1][0] != '-') {
                step = std::stod(argv[++i]);
            }
            config.H_values.clear();
            hExplicit = true;
            for (double v = start; v <= end + 1e-9; v += step) {
                config.H_values.push_back(roundTo(v, 3));
            }
        }
        else if (arg == "--resume") {
            config.resume = true;
        }
        else if (arg == "--no-resume") {
            config.resume = false;
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: cusum [options]\n"
                      << "Options:\n"
                      << "  --config <file>        Load config from JSON file\n"
                      << "  --chart <SN|SR>        Chart type: sign (SN) or signed-rank (SR)\n"
                      << "  --simulations <N>      Number of simulations per pair\n"
                      << "  --n <N>                Subgroup size\n"
                      << "  --max_iter <N>         Maximum iterations per simulation\n"
                      << "  --target_ARL <value>   Target ARL value\n"
                      << "  --cores <N>            Number of CPU cores to use\n"
                      << "  --tolerance <value>    Stop when deviation <= tolerance\n"
                      << "  --update_interval <N>  Update progress every N iterations\n"
                      << "  --k_start <start> <end> [step]  k parameter range\n"
                      << "  --H_start <start> <end> [step]  H parameter range\n"
                      << "  --resume               Resume from checkpoint (default: off)\n"
                      << "  --no-resume            Start fresh (default)\n"
                      << "  --help                 Show this help\n";
            std::exit(0);
        }
    }
    
    if (config.k_values.empty()) {
        for (double k = 3.0; k <= 7.0 + 1e-9; k += 0.1) {
            config.k_values.push_back(roundTo(k, 3));
        }
    }
    if (config.H_values.empty()) {
        for (double H = 3.0; H <= 7.0 + 1e-9; H += 0.1) {
            config.H_values.push_back(roundTo(H, 3));
        }
    }
    
    if (config.n_cores <= 0) {
        config.n_cores = std::thread::hardware_concurrency();
        if (config.n_cores <= 0) config.n_cores = 4;
    }
    
    // Диапазон по умолчанию заточен под SN. Для SR (масштаб статистики
    // ~ n(n+1)/2) сетку нужно задавать явно через --k_start/--H_start.
    if (config.chart_type == "SR" && (!kExplicit || !hExplicit)) {
        std::cerr << "Warning: chart type SR uses the default SN grid [3.0, 7.0]. "
                  << "Set k/H ranges explicitly (--k_start, --H_start or config)."
                  << std::endl;
    }
    
    return config;
}

void Config::saveToFile(const std::string& filename) const {
    nlohmann::json json;
    json["simulations"] = simulations;
    json["n"] = n;
    json["max_iter"] = max_iter;
    json["target_ARL"] = target_ARL;
    json["n_cores"] = n_cores;
    json["checkpoint_interval"] = checkpoint_interval;
    json["top_n"] = top_n;
    json["tolerance"] = tolerance;
    json["update_interval"] = update_interval;
    json["k_values"] = k_values;
    json["H_values"] = H_values;
    json["chart_type"] = chart_type;
    json["temp_file"] = temp_file;
    json["checkpoint_file"] = checkpoint_file;
    json["final_file"] = final_file;
    json["best_file"] = best_file;
    json["log_file"] = log_file;
    json["error_file"] = error_file;
    json["resume"] = resume;
    
    writeFile(filename, json.dump(4));
}

void Config::print() const {
    std::cout << "Configuration:\n";
    std::cout << "  simulations: " << simulations << "\n";
    std::cout << "  n: " << n << "\n";
    std::cout << "  max_iter: " << max_iter << "\n";
    std::cout << "  target_ARL: " << target_ARL << "\n";
    std::cout << "  n_cores: " << n_cores << "\n";
    std::cout << "  tolerance: " << (tolerance < 0 ? "disabled" : std::to_string(tolerance)) << "\n";
    std::cout << "  update_interval: " << update_interval << "\n";
    std::cout << "  chart_type: " << chart_type << "\n";
    std::cout << "  k_values: [" << k_values.front() << ", ..., " << k_values.back() << "] (" << k_values.size() << ")\n";
    std::cout << "  H_values: [" << H_values.front() << ", ..., " << H_values.back() << "] (" << H_values.size() << ")\n";
    std::cout << "  resume: " << (resume ? "true (will continue from checkpoint)" : "false (fresh start)") << "\n";
}

} // namespace cusum