#ifndef CUSUM_BRIDGE_H
#define CUSUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CUSUM_SN = 0, CUSUM_SR = 1 } CusumChartType;

typedef struct {
    double k;
    double H;
    double ARL;
    double deviation;
} CusumResult;

typedef struct {
    CusumResult* results;
    int count;
} CusumResultArray;

/* Opaque handle to Config */
typedef void* CusumConfig;

/* Opaque handle to Calculator */
typedef void* CusumCalculator;

/* Config lifecycle */
CusumConfig cusum_config_create(void);
void cusum_config_destroy(CusumConfig cfg);
int  cusum_config_from_json(CusumConfig cfg, const char* json_str);

/* Config field setters (return 0 on success, -1 on bad input) */
int cusum_config_set_simulations(CusumConfig cfg, int value);
int cusum_config_set_n(CusumConfig cfg, int value);
int cusum_config_set_max_iter(CusumConfig cfg, int value);
int cusum_config_set_target_arl(CusumConfig cfg, double value);
int cusum_config_set_n_cores(CusumConfig cfg, int value);
int cusum_config_set_tolerance(CusumConfig cfg, double value);
int cusum_config_set_chart_type(CusumConfig cfg, CusumChartType chart);
int cusum_config_set_k_range(CusumConfig cfg, double start, double end, double step);
int cusum_config_set_h_range(CusumConfig cfg, double start, double end, double step);
int cusum_config_set_top_n(CusumConfig cfg, int value);
int cusum_config_set_temp_file(CusumConfig cfg, const char* path);
int cusum_config_set_checkpoint_file(CusumConfig cfg, const char* path);
int cusum_config_set_final_file(CusumConfig cfg, const char* path);
int cusum_config_set_best_file(CusumConfig cfg, const char* path);
int cusum_config_set_log_file(CusumConfig cfg, const char* path);
int cusum_config_set_error_file(CusumConfig cfg, const char* path);
int cusum_config_set_checkpoint_interval(CusumConfig cfg, int value);
int cusum_config_set_update_interval(CusumConfig cfg, int value);

/* Single-pair ARL calculation */
double cusum_calculate_arl(CusumConfig cfg, double k, double H, CusumChartType chart);

/* Single-pair ARL with distribution stats.
   stats_out must point to array of at least 10 doubles.
   Returns ARL, fills stats_out with: [ARL, mean, stddev, p5, p25, p50, p75, p95, min, max]. */
double cusum_calculate_arl_dist(CusumConfig cfg, double k, double H,
                                CusumChartType chart, double* stats_out);

/* Full grid search — returns array of results. Caller must free with cusum_free_results. */
CusumResultArray cusum_run_grid(CusumConfig cfg);

/* Free result array returned by cusum_run_grid. */
void cusum_free_results(CusumResultArray arr);

#ifdef __cplusplus
}
#endif

#endif /* CUSUM_BRIDGE_H */