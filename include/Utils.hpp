#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <map>

namespace cusum {

///
/// Сиглтон-обёртка над ГПСЧ (std::mt19937_64) с нормальным распределением.
/// Экземпляр создаётся thread_local: каждый поток владеет своим генератором,
/// setSeed затрагивает только текущий поток — это сохраняет воспроизводимость
/// периодических сидов и исключает гонки в параллельной симуляции.
///
class RandomGenerator {
public:
    /// Доступ к глобальному генератору (синглтон).
    static RandomGenerator& getInstance();
    /// Случайное число из нормального распределения N(mean, sigma^2).
    double normal(double mean = 0.0, double sigma = 1.0);
    /// Устанавливает зерно генератора (для воспроизводимости).
    void setSeed(unsigned int seed);

private:
    RandomGenerator();
    std::mt19937_64 rng_;
    std::normal_distribution<double> normal_dist_{0.0, 1.0};
};

/// Проверяет существование файла (открываемость на чтение).
bool fileExists(const std::string& filename);
/// Читает файл целиком в строку.
std::string readFile(const std::string& filename);
/// Записывает строку в файл (перезаписывая).
void writeFile(const std::string& filename, const std::string& content);
/// Удаляет файл, если он существует (иначе — no-op).
void deleteFile(const std::string& filename);

/// Разбивает строку по разделителю, обрезая пробелы у каждого токена.
std::vector<std::string> split(const std::string& str, char delimiter);
/// Удаляет ведущие/хвостовые пробелы, табы и переводы строк.
std::string trim(const std::string& str);
/// Форматирует секунды в "HH:MM:SS".
std::string formatTime(double seconds);

/// Округляет значение до заданного числа знаков после запятой.
double roundTo(double value, int decimals);
/// Сравнение двух чисел по модулю разности < eps.
bool isClose(double a, double b, double eps = 1e-6);

/// Среднее арифметическое (0 для пустого набора).
double calculateMean(const std::vector<int>& data);
/// Выборочное стандартное отклонение (n-1); 0 для выборок < 2 элементов.
double calculateStdDev(const std::vector<int>& data, double mean);
/// Линейная интерполяция перцентиля по отсортированным данным.
double calculatePercentile(const std::vector<int>& data, double percentile);
/// Гистограмма: bin = (value / binSize) * binSize -> частота.
std::map<int, int> calculateHistogram(const std::vector<int>& data, int binSize = 50);

} // namespace cusum