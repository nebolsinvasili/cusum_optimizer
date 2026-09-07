// ============================================================================
// Simulator.cpp — Монте-Карло симуляция двусторонней CUSUM-карты на знаковой
// (SN) и знаково-ранговой (SR) статистиках.
//
// Идея алгоритма:
//  1. На каждом шаге из n нормальных N(0,1) наблюдений строится статистика:
//       * SN (знаковая):       sn = 2*t - n, t = |{x_i >= 0}|;
//       * SR (знаково-ранговая): s = sum(sgn(x_i)*rank(|x_i|)), ранг по |x|
//                                (средний ранг при связях), диапазон ±M,
//                                M = n(n+1)/2.
//     При H0 (процесс в норме, E[x]=0) обе статистики имеют нулевое среднее.
//  2. Обновляются два кумулятивных счётчика (вверх и вниз):
//         C+ = max(0, C+ + stat - k)
//         C- = max(0, C- - stat - k)
//     Параметр k — «reference value» (допустимый сдвиг).
//  3. Сигнал о разладке — когда C+ >= H или C- >= H. H — «decision interval».
//  4. Число шагов до сигнала называется длиной серии (run length).
//     ARL = средняя длина серии по всем прогонам.
//
// Примечание по зернам (FIXME): глобальный RandomGenerator — синглтон, поэтому
// прогоны внутри одной партии последовательны; независимость между партиями
// обеспечивается разными зернами (см. calculateARLParallel).
// ============================================================================

#include "Simulator.hpp"
#include "Utils.hpp"
#include <cmath>
#include <omp.h>
#include <algorithm>
#include <numeric>
#include <utility>

namespace cusum {

namespace {

// Знаковая статистика подгруппы: sn = 2*t - n, t = число неотрицательных.
// Это сумма знаков sign(x_i) по подгруппе (Qiu, гл. 4.2.2.3, CUSUM-SN).
double statisticSN(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        double x = RandomGenerator::getInstance().normal(0.0, 1.0);
        if (x >= 0) t++;
    }
    return 2.0 * t - n;
}

// Знаково-ранговая статистика подгруппы: s = sum( sgn(x_i) * rank(|x_i|) ).
// Эквивалентна SR_t = 2*T_n^+ - n(n+1)/2, где T_n^+ — сумма рангов
// положительных наблюдений (Qiu, гл. 4.2.2.4, CUSUM-SR).
// Ранги считаются по |x| (средний ранг при точных совпадениях |x| — для
// непрерывных данных связи практически исключены). sgn(0) = 0.
double statisticSR(int n) {
    std::vector<std::pair<double, double>> obs;   // (|x|, x)
    obs.reserve(n);
    for (int i = 0; i < n; ++i) {
        double x = RandomGenerator::getInstance().normal(0.0, 1.0);
        obs.emplace_back(std::abs(x), x);
    }
    std::sort(obs.begin(), obs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    double sum = 0.0;
    int i = 0;
    while (i < n) {
        int j = i;
        while (j < n && obs[j].first == obs[i].first) ++j;   // группа связей
        double avgRank = (i + 1.0 + j) / 2.0;                // средний ранг группы
        for (int m = i; m < j; ++m) {
            double sign = obs[m].second > 0.0 ? 1.0 : (obs[m].second < 0.0 ? -1.0 : 0.0);
            sum += sign * avgRank;
        }
        i = j;
    }
    return sum;
}

// Один прогон серии: возвращает длину серии (шаг сигнала либо maxIter).
int runOneSeries(double k, double H, int n, int maxIter, ChartType chart) {
    double Ci_plus = 0.0;
    double Ci_minus = 0.0;
    
    for (int step = 1; step <= maxIter; ++step) {
        double stat = (chart == ChartType::SR) ? statisticSR(n) : statisticSN(n);
        
        Ci_plus = Ci_plus + stat - k;
        if (Ci_plus < 0) Ci_plus = 0.0;   // «подрезка» до нуля = сброс счётчика
        
        Ci_minus = Ci_minus - stat - k;
        if (Ci_minus < 0) Ci_minus = 0.0;
        
        if (Ci_plus >= H || Ci_minus >= H) {
            return step;   // карта дала сигнал — запоминаем длину серии
        }
    }
    return maxIter;
}

} // namespace

double simulateBatch(double k, double H, int simCount, int n, int maxIter, unsigned int seed,
                     ChartType chart) {
    // Один общий seed на всю партию: внутри партии прогоны идут последовательно.
    RandomGenerator::getInstance().setSeed(seed);
    
    long long totalRunLengths = 0;
    
    for (int iter = 0; iter < simCount; ++iter) {
        totalRunLengths += runOneSeries(k, H, n, maxIter, chart);
    }
    
    return static_cast<double>(totalRunLengths) / simCount;
}

std::tuple<double, std::vector<int>, std::vector<double>> 
simulateBatchWithDistribution(double k, double H, int simCount, int n, int maxIter, unsigned int seed,
                              ChartType chart) {
    RandomGenerator::getInstance().setSeed(seed);
    
    std::vector<int> runLengths;
    runLengths.reserve(simCount);
    long long totalRunLengths = 0;
    
    for (int iter = 0; iter < simCount; ++iter) {
        int runLength = runOneSeries(k, H, n, maxIter, chart);
        runLengths.push_back(runLength);
        totalRunLengths += runLength;
    }
    
    double ARL = static_cast<double>(totalRunLengths) / simCount;
    
    std::vector<double> stats;
    stats.push_back(ARL);
    
    std::vector<int> sorted = runLengths;
    std::sort(sorted.begin(), sorted.end());
    
    stats.push_back(calculateMean(sorted));
    stats.push_back(calculateStdDev(sorted, stats[1]));
    stats.push_back(calculatePercentile(sorted, 5.0));
    stats.push_back(calculatePercentile(sorted, 25.0));
    stats.push_back(calculatePercentile(sorted, 50.0));
    stats.push_back(calculatePercentile(sorted, 75.0));
    stats.push_back(calculatePercentile(sorted, 95.0));
    if (!sorted.empty()) {
        stats.push_back(static_cast<double>(sorted.front()));
        stats.push_back(static_cast<double>(sorted.back()));
    } else {
        stats.push_back(0.0);
        stats.push_back(0.0);
    }
    
    return std::make_tuple(ARL, runLengths, stats);
}

double calculateARLParallel(double k, double H, int simCount, int n, int maxIter, int numThreads,
                            ChartType chart) {
    // Декомпозиция: simCount прогонов делится на cores независимых партий.
    // Каждая партия берёт свой seed (детерминированно по индексу ядра и (k,H)).
    // Итоговый ARL — среднее с весами, равными размеру партии (корректно
    // при неточном делении, когда остаток распределяется по первым партиям).
    int cores = numThreads > 0 ? numThreads : omp_get_max_threads();
    int simPerCore = simCount / cores;
    int remainder = simCount % cores;
    
    std::vector<double> results(cores, 0.0);
    std::vector<int> counts(cores, 0);
    
    #pragma omp parallel for num_threads(cores)
    for (int i = 0; i < cores; ++i) {
        int sCount = simPerCore + (i < remainder ? 1 : 0);
        unsigned int seed = 42 + i * 1000 + static_cast<unsigned int>(k * 100) + static_cast<unsigned int>(H * 100);
        results[i] = simulateBatch(k, H, sCount, n, maxIter, seed, chart);
        counts[i] = sCount;
    }
    
    double totalSim = 0;
    double weightedSum = 0;
    for (int i = 0; i < cores; ++i) {
        weightedSum += results[i] * counts[i];
        totalSim += counts[i];
    }
    
    return weightedSum / totalSim;
}

std::tuple<double, std::vector<int>, std::vector<double>> 
calculateARLParallelWithDistribution(double k, double H, int simCount, int n, int maxIter, int numThreads,
                                     ChartType chart) {
    int cores = numThreads > 0 ? numThreads : omp_get_max_threads();
    int simPerCore = simCount / cores;
    int remainder = simCount % cores;
    
    std::vector<std::vector<int>> allRunLengths(cores);
    std::vector<double> arls(cores, 0.0);
    std::vector<int> counts(cores, 0);
    
    #pragma omp parallel for num_threads(cores)
    for (int i = 0; i < cores; ++i) {
        int sCount = simPerCore + (i < remainder ? 1 : 0);
        unsigned int seed = 42 + i * 1000 + static_cast<unsigned int>(k * 100) + static_cast<unsigned int>(H * 100);
        auto result = simulateBatchWithDistribution(k, H, sCount, n, maxIter, seed, chart);
        arls[i] = std::get<0>(result);
        allRunLengths[i] = std::get<1>(result);
        counts[i] = sCount;
    }
    
    std::vector<int> combinedRunLengths;
    int totalSim = 0;
    for (int i = 0; i < cores; ++i) {
        combinedRunLengths.insert(combinedRunLengths.end(), 
                                  allRunLengths[i].begin(), 
                                  allRunLengths[i].end());
        totalSim += counts[i];
    }
    
    double totalARL = 0;
    for (int i = 0; i < cores; ++i) {
        totalARL += arls[i] * counts[i];
    }
    totalARL /= totalSim;
    
    std::vector<double> stats;
    stats.push_back(totalARL);
    
    std::vector<int> sorted = combinedRunLengths;
    std::sort(sorted.begin(), sorted.end());
    
    stats.push_back(calculateMean(sorted));
    stats.push_back(calculateStdDev(sorted, stats[1]));
    stats.push_back(calculatePercentile(sorted, 5.0));
    stats.push_back(calculatePercentile(sorted, 25.0));
    stats.push_back(calculatePercentile(sorted, 50.0));
    stats.push_back(calculatePercentile(sorted, 75.0));
    stats.push_back(calculatePercentile(sorted, 95.0));
    if (!sorted.empty()) {
        stats.push_back(static_cast<double>(sorted.front()));
        stats.push_back(static_cast<double>(sorted.back()));
    } else {
        stats.push_back(0.0);
        stats.push_back(0.0);
    }
    
    return std::make_tuple(totalARL, combinedRunLengths, stats);
}

} // namespace cusum