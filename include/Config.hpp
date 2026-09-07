#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace cusum {

///
/// Конфигурация запуска расчёта ARL.
///
/// Все поля имеют значения по умолчанию и могут быть перезаписаны
/// из JSON-файла (loadFromFile) или аргументов командной строки (loadFromArgs).
/// Схема полей идентична схеме JSON-конфига (см. docs/API.md).
///
struct Config {
    // --- Simulation parameters -------------------------------------------
    /// Число прогонов Монте-Карло для одной пары (k, H).
    int simulations = 5000;
    /// Размер подгруппы: число наблюдений, из которых строится статистика SN.
    int n = 14;
    /// Максимальная длина серии: если сигнала нет за max_iter шагов — серия обрывается.
    int max_iter = 5000;
    /// Целевой ARL, к которому подбирается пара (k, H).
    double target_ARL = 370.0;
    /// Число ядер CPU для OpenMP. Значение <= 0 означает автоопределение.
    int n_cores = 0;
    /// Сохранять контрольную точку каждые N обработанных комбинаций.
    int checkpoint_interval = 5;
    /// Сколько лучших пар выводить и сохранять в best_arl_pairs.csv.
    int top_n = 10;

    // --- Tolerance for early stopping ------------------------------------
    /// Ранняя остановка при abs(ARL - target_ARL) <= tolerance. Отрицательное — отключено.
    double tolerance = -1.0;

    // --- Progress bar update interval ------------------------------------
    /// Перерисовывать прогресс-бар каждые N обработанных комбинаций.
    int update_interval = 5;

    // --- Parameter ranges ------------------------------------------------
    /// Тип контрольной карты: "SN" (знаковая) или "SR" (знаково-ранговая).
    std::string chart_type = "SN";
    /// Сетка значений параметра сдвига k (сортируется перед перебором).
    std::vector<double> k_values;
    /// Сетка значений порога принятия решения H (сортируется перед перебором).
    std::vector<double> H_values;

    // --- File paths --------------------------------------------------------
    /// Временный файл результатов (дописывается во время расчёта).
    std::string temp_file = "arl_calculation_temp.csv";
    /// Файл контрольной точки (последняя обработанная пара k,H).
    std::string checkpoint_file = "arl_checkpoint.txt";
    /// Итоговый CSV со всеми парами.
    std::string final_file = "arl_results_final.csv";
    /// CSV с топ-N парами и отклонением от целевого ARL.
    std::string best_file = "best_arl_pairs.csv";
    /// Журнал запусков.
    std::string log_file = "arl_calculation.log";
    /// Журнал ошибок по отдельным парам.
    std::string error_file = "errors.log";

    // --- Resume -------------------------------------------------------------
    /// Продолжить прерванный расчёт (true) либо начать заново (false).
    bool resume = false;

    /// Загружает конфигурацию из JSON-файла. Недостающие ключи получают
    /// значения по умолчанию. При ошибке парсинга печатает сообщение в stderr.
    static Config loadFromFile(const std::string& filename);

    /// Разбирает аргументы командной строки (--config, --simulations, --k_start и т.д.).
    /// См. docs/API.md раздел "CLI-интерфейс".
    static Config loadFromArgs(int argc, char** argv);

    /// Сохраняет конфигурацию в JSON-файл (отступ 4 пробела).
    void saveToFile(const std::string& filename) const;

    /// Выводит текущую конфигурацию в stdout (для диагностики).
    void print() const;
};

} // namespace cusum