#pragma once

#include <vector>
#include <tuple>
#include <string>
#include "Config.hpp"

namespace cusum {

///
/// Одна запись результата: пара (k, H), ARL и отклонение от целевого значения.
///
struct Result {
    double k;          ///< Параметр сдвига CUSUM.
    double H;          ///< Порог принятия решения.
    double ARL;        ///< Средняя длина серии.
    double deviation;  ///< abs(ARL - target_ARL).
};

///
/// [ВНИМАНИЕ: класс не включён в сборку — Makefile его не компилирует.]
///
/// Управляет накоплением результатов и их сохранением в CSV.
/// Дублирует часть функциональности Calculator (saveFinalResults/saveBestPairs);
/// при дальнейшей разработке нужно либо интегрировать его в конвейер,
/// либо удалить (TODO, см. Roadmap в README).
///
class ResultManager {
public:
    /// Создаёт менеджер. target_ARL из конфига используется для отклонений.
    explicit ResultManager(const Config& config);

    /// Добавляет результат, вычисляя deviation = abs(ARL - target_ARL).
    void addResult(double k, double H, double ARL);
    /// Сохраняет все результаты в CSV с заголовком "k,H,ARL".
    void saveToFile(const std::string& filename) const;
    /// Загружает результаты из CSV (пропускает строку заголовка).
    void loadFromFile(const std::string& filename);
    /// Сохраняет итоговый файл (final_file) и лучшие пары (best_file).
    void saveFinalResults();
    /// Сохраняет топ-N пар по минимальному отклонению в best_file.
    void saveBestPairs(int n);

    /// Возвращает топ-N пар по минимальному отклонению (копии, сортировка не мутирует поля).
    std::vector<Result> getBestPairs(int n) const;
    /// Возвращает все накопленные результаты.
    std::vector<Result> getAllResults() const { return results_; }

    /// Число накопленных результатов.
    int size() const { return results_.size(); }
    /// Очищает накопленные результаты.
    void clear() { results_.clear(); }

private:
    Config config_;
    std::vector<Result> results_;
};

} // namespace cusum