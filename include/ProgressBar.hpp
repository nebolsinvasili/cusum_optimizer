#pragma once

#include <string>
#include <chrono>

namespace cusum {

///
/// Текстовый прогресс-бар для CLI.
///
/// Рисуется в одной строке стандартного вывода через '\r', отображает
/// процент, счётчик, затраченное время, ETA, текущий ARL и найденный
/// лучший результат. Пример использования:
///
///     ProgressBar bar(total);
///     bar.update(processed, "k=3.0, H=4.0", arl);
///     bar.setBestResult(bestK, bestH, bestARL, targetARL);
///     bar.finish("Complete!");
///
class ProgressBar {
public:
    /// Создаёт бар: total — всего шагов, width — ширина полосы из символов '#'.
    ProgressBar(int total, int width = 50);
    /// Автоматически завершает бар при разрушении объекта.
    ~ProgressBar();

    /// Перерисовывает бар с позицией current, подписью message и значением ARL.
    /// arl < 0 скрывает поле ARL.
    void update(int current, const std::string& message = "", double arl = -1.0);
    /// То же, что update, но current увеличивается на step.
    void increment(int step = 1, const std::string& message = "", double arl = -1.0);
    /// Завершает бар, выводя финальное сообщение (по умолчанию "Complete").
    void finish(const std::string& message = "Complete");
    /// Отображает текущий лучший результат: пару (k, H), ARL и отклонение.
    void setBestResult(double k, double H, double arl, double targetARL);
    /// Отмечает раннюю остановку (добавляет метку "[EARLY STOP]").
    void setEarlyStop(bool stopped) { early_stopped_ = stopped; }

private:
    /// Печатает текущее состояние бара в одной строке.
    void render();
    /// Секунды -> "HH:MM:SS".
    std::string formatTime(double seconds);

    int total_;
    int width_;
    int current_ = 0;
    std::chrono::steady_clock::time_point start_time_;
    std::string message_;
    double arl_value_ = -1.0;
    int last_line_length_ = 0;

    bool has_best_ = false;
    double best_k_ = 0.0;
    double best_H_ = 0.0;
    double best_arl_ = 0.0;
    double best_deviation_ = 0.0;
    double target_arl_ = 370.0;
    bool early_stopped_ = false;
};

} // namespace cusum