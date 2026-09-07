// ============================================================================
// main.cpp — точка входа приложения.
//
// Логика запуска:
//  * Без аргументов: загрузка config/default_config.json (если есть),
//    иначе — конфигурация по умолчанию.
//  * С аргументами: парсинг CLI (Config::loadFromArgs) с опцией --config.
//  * Режим resume: продолжение с temp/checkpoint файлами.
//  * Обычный режим: удаление старых temp/checkpoint файлов (fresh start).
// ============================================================================

#include "Config.hpp"
#include "Calculator.hpp"
#include "Utils.hpp"
#include <iostream>
#include <string>

using namespace cusum;

int main(int argc, char** argv) {
    std::cout << "=== CUSUM ARL Calculator ===" << std::endl;
    std::cout << "Version 1.0.0" << std::endl;
    std::cout << std::endl;
    
    Config config;
    if (argc > 1) {
        config = Config::loadFromArgs(argc, argv);
    } else {
        if (fileExists("config/default_config.json")) {
            config = Config::loadFromFile("config/default_config.json");
            config.resume = false;   // по умолчанию всегда начинаем заново
        }
    }
    
    config.print();
    std::cout << std::endl;
    
    if (config.resume) {
        if (!fileExists(config.temp_file)) {
            // Resume без temp-файла бессмысленен: нечего продолжать.
            std::cout << "Warning: resume is enabled but temp file not found. Starting fresh." << std::endl;
            config.resume = false;
        } else {
            std::cout << "Resume mode: continuing from checkpoint" << std::endl;
        }
    } else {
        // Fresh start: удаляем артефакты предыдущего запуска, чтобы
        // temp-файл и checkpoint не «догнали» результаты нового расчёта.
        if (fileExists(config.temp_file)) {
            deleteFile(config.temp_file);
            std::cout << "Fresh start: deleted old temp file" << std::endl;
        }
        if (fileExists(config.checkpoint_file)) {
            deleteFile(config.checkpoint_file);
            std::cout << "Fresh start: deleted old checkpoint file" << std::endl;
        }
        std::cout << "Fresh start mode: starting new calculation" << std::endl;
    }
    std::cout << std::endl;
    
    Calculator calculator(config);
    calculator.run(config);
    
    // TODO: выходной код всегда 0. Стоит возвращать ненулевой код
    // при ошибках (конфиг не найден, temp-файл не открылся) — см. Roadmap.
    return 0;
}