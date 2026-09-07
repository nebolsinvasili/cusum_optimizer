#pragma once

#include <vector>
#include <unordered_set>
#include <tuple>
#include <string>
#include <cstdint>
#include "Config.hpp"

namespace cusum {

/// Одна запись результата: пара параметров (k, H) и полученный ARL.
struct ResultEntry {
    double k;      ///< Параметр сдвига CUSUM.
    double H;      ///< Порог принятия решения.
    double ARL;    ///< Средняя длина серии по прогонам Монте-Карло.
    uint64_t key;  ///< Уникальный ключ пары (см. makeKey).
};

///
/// Оркестратор расчёта ARL.
///
/// Перебирает сетку (k, H), вызывает симулятор, ведёт чекпоинты и
/// сохраняет результаты в CSV. Поддерживает прерывание и продолжение расчёта.
///
class Calculator {
public:
    /// Создаёт калькулятор для заданной конфигурации.
    explicit Calculator(const Config& config);

    /// Запускает перебор всех пар (k, H) из конфигурации.
    /// В resume-режиме предварительно загружает завершённые комбинации.
    void run(const Config& config);
    /// Продолжает расчёт с контрольной точки (обёртка над run с resume=true).
    void resume(const Config& config);

private:
    /// Проверяет, была ли пара (k, H) уже посчитана (по ключу).
    bool isCombinationCompleted(double k, double H) const;
    /// Сохраняет контрольную точку: последнюю обработанную пару k,H.
    void saveCheckpoint(double k, double H);
    /// Загружает контрольную точку (пару k,H) из checkpoint_file.
    void loadCheckpoint(double& k, double& H);
    /// Читает завершённые комбинации из temp_file в completed_combinations_ и results_.
    void loadCompletedCombinations();
    /// Копирует temp_file в final_file и сохраняет лучшие пары.
    void saveFinalResults();
    /// Сохраняет топ-N пар по минимальному отклонению |ARL - target_ARL|.
    void saveBestPairs();
    /// Печатает топ-N лучших пар в stdout.
    void printTopResults(int n) const;
    /// Пишет построчную запись в log_file с временной меткой.
    void log(const std::string& message);
    /// Пишет ошибку расчёта пары (k, H) в error_file.
    void logError(double k, double H, const std::string& error);
    /// Строит uint64-ключ из (k, H): старшие 32 бита - k, младшие 32 бита - H.
    /// Смещение +10000 гарантирует неотрицательность хранения для k,H из [3.0, 7.0].
    uint64_t makeKey(double k, double H) const;

    Config config_;
    /// Множество ключей уже посчитанных пар (для resume / пропуска дубликатов).
    std::unordered_set<uint64_t> completed_combinations_;
    /// Все накопленные результаты за текущий запуск.
    std::vector<ResultEntry> results_;
    /// Счётчик обработанных комбинаций (для прогресса и чекпоинтов).
    int processed_ = 0;
};

} // namespace cusum