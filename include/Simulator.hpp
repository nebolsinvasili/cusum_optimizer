#pragma once

#include <vector>
#include <random>
#include <tuple>

namespace cusum {

/// Тип контрольной карты: знаковая (SN) или знаково-ранговая (SR).
enum class ChartType {
    SN,  ///< Знаковая статистика sn = 2*t - n.
    SR   ///< Знаково-ранговая статистика s = sum( sgn(x_i) * rank(|x_i|) ).
};

///
/// Последовательная симуляция партии прогонов двусторонней CUSUM-карты.
///
/// На каждом шаге из n нормальных N(0,1) наблюдений строится статистика:
///   * SN: sn = 2*t - n (t - число неотрицательных наблюдений);
///   * SR: s = sum(sgn(x_i) * rank(|x_i|)), ранг по абсолютному значению
///     (средний ранг при связях), диапазон +-(n(n+1)/2).
/// Обновляются счётчики C+ = max(0, C+ + stat - k) и C- = max(0, C- - stat - k);
/// сигнал — при C+ >= H или C- >= H. Возвращает среднюю длину серии (ARL).
///
/// \param k        Параметр сдвига (reference value).
/// \param H        Порог принятия решения (decision interval).
/// \param simCount Число прогонов Монте-Карло.
/// \param n        Размер подгруппы.
/// \param maxIter  Обрыв серии без сигнала.
/// \param seed     Зерно ГПСЧ (используется через глобальный RandomGenerator).
/// \param chart    Тип карты (SN по умолчанию).
/// \return         Средняя длина серии (ARL).
double simulateBatch(double k, double H, int simCount, int n, int maxIter, unsigned int seed,
                     ChartType chart = ChartType::SN);

///
/// Параллельная симуляция ARL: simCount разбивается на numThreads независимых
/// партий с разными зернами, результаты усредняются с весами по размеру партии.
///
/// \param numThreads Число ядер; <= 0 означает omp_get_max_threads().
/// \param chart      Тип карты (SN по умолчанию).
/// \return          Средневзвешенный ARL по всем партиям.
double calculateARLParallel(double k, double H,
                           int simCount, int n, int maxIter,
                           int numThreads, ChartType chart = ChartType::SN);

///
/// Как simulateBatch, но дополнительно возвращает длины всех серий
/// и статистику их распределения (mean, stddev, перцентили 5/25/50/75/95,
/// минимум, максимум).
///
/// \param[out] 1-й элемент tuple: ARL.
/// \param[out] 2-й элемент tuple: вектор длин серий (по одной на прогон).
/// \param[out] 3-й элемент tuple: [ARL, mean, stddev, p5, p25, p50, p75, p95, min, max].
std::tuple<double, std::vector<int>, std::vector<double>>
simulateBatchWithDistribution(double k, double H, int simCount, int n, int maxIter, unsigned int seed,
                              ChartType chart = ChartType::SN);

///
/// Параллельная версия simulateBatchWithDistribution: объединяет длины серий
/// всех партий и считает статистику по объединённой выборке.
std::tuple<double, std::vector<int>, std::vector<double>>
calculateARLParallelWithDistribution(double k, double H,
                                     int simCount, int n, int maxIter,
                                     int numThreads, ChartType chart = ChartType::SN);

} // namespace cusum