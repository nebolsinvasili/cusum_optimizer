#include "bridge.h"
#include "Config.hpp"
#include "Calculator.hpp"
#include "Simulator.hpp"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

static cusum::ChartType to_chart(CusumChartType c) {
    return (c == CUSUM_SR) ? cusum::ChartType::SR : cusum::ChartType::SN;
}

extern "C" {

CusumConfig cusum_config_create(void) {
    auto* cfg = new(std::nothrow) cusum::Config();
    return static_cast<CusumConfig>(cfg);
}

void cusum_config_destroy(CusumConfig cfg) {
    if (cfg) {
        delete static_cast<cusum::Config*>(cfg);
    }
}

int cusum_config_from_json(CusumConfig cfg, const char* json_str) {
    if (!cfg || !json_str) return -1;
    const char* tmp = "/tmp/cusum_config_tmp.json";
    FILE* f = std::fopen(tmp, "w");
    if (!f) return -1;
    std::fputs(json_str, f);
    std::fclose(f);
    try {
        *static_cast<cusum::Config*>(cfg) = cusum::Config::loadFromFile(tmp);
        std::remove(tmp);
        return 0;
    } catch (...) {
        std::remove(tmp);
        return -1;
    }
}

int cusum_config_set_simulations(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->simulations = value;
    return 0;
}

int cusum_config_set_n(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->n = value;
    return 0;
}

int cusum_config_set_max_iter(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->max_iter = value;
    return 0;
}

int cusum_config_set_target_arl(CusumConfig cfg, double value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->target_ARL = value;
    return 0;
}

int cusum_config_set_n_cores(CusumConfig cfg, int value) {
    if (!cfg) return -1;
    static_cast<cusum::Config*>(cfg)->n_cores = value;
    return 0;
}

int cusum_config_set_tolerance(CusumConfig cfg, double value) {
    if (!cfg) return -1;
    static_cast<cusum::Config*>(cfg)->tolerance = value;
    return 0;
}

int cusum_config_set_chart_type(CusumConfig cfg, CusumChartType chart) {
    if (!cfg) return -1;
    static_cast<cusum::Config*>(cfg)->chart_type = (chart == CUSUM_SR) ? "SR" : "SN";
    return 0;
}

int cusum_config_set_k_range(CusumConfig cfg, double start, double end, double step) {
    if (!cfg || step <= 0 || start > end) return -1;
    auto* c = static_cast<cusum::Config*>(cfg);
    c->k_values.clear();
    for (double v = start; v <= end + step * 0.5; v += step) {
        c->k_values.push_back(v);
    }
    return 0;
}

int cusum_config_set_h_range(CusumConfig cfg, double start, double end, double step) {
    if (!cfg || step <= 0 || start > end) return -1;
    auto* c = static_cast<cusum::Config*>(cfg);
    c->H_values.clear();
    for (double v = start; v <= end + step * 0.5; v += step) {
        c->H_values.push_back(v);
    }
    return 0;
}

int cusum_config_set_top_n(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->top_n = value;
    return 0;
}

int cusum_config_set_temp_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->temp_file = path;
    return 0;
}

int cusum_config_set_checkpoint_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->checkpoint_file = path;
    return 0;
}

int cusum_config_set_final_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->final_file = path;
    return 0;
}

int cusum_config_set_best_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->best_file = path;
    return 0;
}

int cusum_config_set_log_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->log_file = path;
    return 0;
}

int cusum_config_set_error_file(CusumConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<cusum::Config*>(cfg)->error_file = path;
    return 0;
}

int cusum_config_set_checkpoint_interval(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->checkpoint_interval = value;
    return 0;
}

int cusum_config_set_update_interval(CusumConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<cusum::Config*>(cfg)->update_interval = value;
    return 0;
}

double cusum_calculate_arl(CusumConfig cfg, double k, double H, CusumChartType chart) {
    if (!cfg) return -1.0;
    auto* c = static_cast<cusum::Config*>(cfg);
    return cusum::calculateARLParallel(k, H, c->simulations, c->n,
                                       c->max_iter, c->n_cores, to_chart(chart));
}

double cusum_calculate_arl_dist(CusumConfig cfg, double k, double H,
                                CusumChartType chart, double* stats_out) {
    if (!cfg || !stats_out) return -1.0;
    auto* c = static_cast<cusum::Config*>(cfg);
    auto [arl, lengths, stats] = cusum::calculateARLParallelWithDistribution(
        k, H, c->simulations, c->n, c->max_iter, c->n_cores, to_chart(chart));
    int count = std::min(static_cast<int>(stats.size()), 10);
    for (int i = 0; i < count; ++i) stats_out[i] = stats[i];
    return arl;
}

CusumResultArray cusum_run_grid(CusumConfig cfg) {
    CusumResultArray empty = {nullptr, 0};
    if (!cfg) return empty;

    auto* c = static_cast<cusum::Config*>(cfg);
    c->resume = false;

    cusum::Calculator calculator(*c);
    calculator.run(*c);

    const auto& results = calculator.getResults();
    if (results.empty()) return empty;

    CusumResultArray arr;
    arr.count = static_cast<int>(results.size());
    arr.results = static_cast<CusumResult*>(std::malloc(arr.count * sizeof(CusumResult)));
    if (!arr.results) return empty;

    for (int i = 0; i < arr.count; ++i) {
        arr.results[i].k = results[i].k;
        arr.results[i].H = results[i].H;
        arr.results[i].ARL = results[i].ARL;
        arr.results[i].deviation = std::abs(results[i].ARL - c->target_ARL);
    }
    return arr;
}

void cusum_free_results(CusumResultArray arr) {
    if (arr.results) {
        std::free(arr.results);
    }
}

} // extern "C"