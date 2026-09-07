#include "ProgressBar.hpp"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace cusum {

ProgressBar::ProgressBar(int total, int width)
    : total_(total), width_(width), start_time_(std::chrono::steady_clock::now()) {}

ProgressBar::~ProgressBar() {
    finish();
}

void ProgressBar::update(int current, const std::string& message, double arl) {
    current_ = current;
    message_ = message;
    arl_value_ = arl;
    render();
}

void ProgressBar::increment(int step, const std::string& message, double arl) {
    current_ += step;
    message_ = message;
    arl_value_ = arl;
    render();
}

void ProgressBar::finish(const std::string& message) {
    current_ = total_;
    message_ = message;
    render();
    std::cout << std::endl;
    std::cout.flush();
}

void ProgressBar::setBestResult(double k, double H, double arl, double targetARL) {
    has_best_ = true;
    best_k_ = k;
    best_H_ = H;
    best_arl_ = arl;
    best_deviation_ = std::abs(arl - targetARL);
    target_arl_ = targetARL;
    render();
}

void ProgressBar::render() {
    double percent = std::min(100.0, (static_cast<double>(current_) / total_) * 100.0);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    
    std::string elapsedStr = formatTime(elapsed);
    std::string etaStr = "--:--:--";
    
    if (current_ > 0 && percent > 0) {
        double eta = (static_cast<double>(elapsed) / current_) * (total_ - current_);
        etaStr = formatTime(eta);
    }
    
    int filled = static_cast<int>(width_ * percent / 100.0);
    std::string bar = std::string(filled, '#') + std::string(width_ - filled, '-');
    
    std::ostringstream line;
    line << "Progress: |" << bar << "| " 
         << std::setw(5) << std::setprecision(1) << std::fixed << percent << "%  "
         << current_ << "/" << total_ << "  "
         << "Elapsed: " << elapsedStr << "  ETA: " << etaStr;
    
    if (arl_value_ >= 0 && !message_.empty()) {
        std::string msg = message_;
        if (msg.length() > 30) {
            msg = msg.substr(0, 27) + "...";
        }
        line << "  [" << msg << ", ARL: " 
             << std::setw(6) << std::setprecision(1) << std::fixed << arl_value_ << "]";
    } else if (arl_value_ >= 0) {
        line << "  [ARL: " << std::setw(6) << std::setprecision(1) << std::fixed << arl_value_ << "]";
    }
    
    if (has_best_) {
        line << " Best: k=" << std::setprecision(3) << std::fixed << best_k_
             << ", H=" << std::setprecision(3) << std::fixed << best_H_
             << ", ARL=" << std::setprecision(1) << std::fixed << best_arl_
             << " (dev=" << std::setprecision(1) << std::fixed << best_deviation_ << ")";
    }
    
    if (early_stopped_) {
        line << " [EARLY STOP]";
    }
    
    // Очистка строки: сначала '\r' + пробелы длиной прошлой строки, затем
    // позиционирование в начало. Это проще, чем запоминать количество символов
    // для точного перезатирания — но требует перерисовки всей строки.
    std::string output = line.str();
    
    std::cout << '\r' << std::string(last_line_length_, ' ') << '\r';
    std::cout << output;
    std::cout.flush();
    
    last_line_length_ = output.length();
}

std::string ProgressBar::formatTime(double seconds) {
    if (seconds < 0) seconds = 0;
    int hours = static_cast<int>(seconds / 3600);
    int minutes = static_cast<int>((seconds - hours * 3600) / 60);
    int secs = static_cast<int>(seconds - hours * 3600 - minutes * 60);
    
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setw(2) << minutes << ":"
       << std::setw(2) << secs;
    return ss.str();
}

} // namespace cusum