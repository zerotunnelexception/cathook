/*
 * logging.cpp
 *
 *  Created on: Oct 3, 2016
 *      Author: nullifiedcat
 */

#include <stdarg.h>
#include <string>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <settings/Bool.hpp>
#include "logging.hpp"
#include "helpers.hpp"
#include "MiscTemporary.hpp"
#include "hack.hpp"

static settings::Boolean log_to_console{ "hack.log-console", "true" };
static settings::Boolean enable_file_logging{ "hack.log-file", "true" };

static bool shut_down = false;
std::ofstream logging::handle;

#if ENABLE_LOGGING
void logging::Initialize()
{
    if (!*enable_file_logging)
        return;
        
    try {
        if (logging::handle.is_open())
            logging::handle.close();
            
        static passwd *pwd = getpwuid(getuid());
        if (!pwd) {
            fprintf(stderr, "Failed to get user info for logging\n");
            return;
        }
        
        std::string log_path = strfmt("/tmp/cathook-%s-%d.log", pwd->pw_name, getpid()).get();
        logging::handle.open(log_path, std::ios::out | std::ios::app);
        
        if (!logging::handle.is_open()) {
            fprintf(stderr, "Failed to open log file %s: %s\n", log_path.c_str(), strerror(errno));
            return;
        }
        
        // Write initial log entry to confirm file is working
        time_t current_time = time(nullptr);
        struct tm *time_info = localtime(&current_time);
        char timeString[10];
        strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);
        
        std::string init_msg = strfmt("[%s] Logging initialized\n", timeString).get();
        logging::handle << init_msg;
        logging::handle.flush();
        
    } catch (const std::exception& e) {
        fprintf(stderr, "Exception while initializing logging: %s\n", e.what());
    }
}
#endif

static inline void Log(const char *result, bool file_only)
{
#if ENABLE_LOGGING
    if (!*enable_file_logging)
        goto console_log;
        
    try {
        if (!logging::handle.is_open())
            logging::Initialize();
            
        if (logging::handle.is_open()) {
            time_t current_time = time(nullptr);
            struct tm *time_info = localtime(&current_time);
            char timeString[10];
            strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);

            std::string to_log = result;
            to_log = strfmt("[%s] ", timeString).get() + to_log + "\n";
            logging::handle << to_log;
            logging::handle.flush();
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Error writing to log: %s\n", e.what());
    }
    
console_log:
#if ENABLE_VISUALS
    if (!hack::shutdown && !file_only && *log_to_console)
    {
        try {
            if (g_ICvar) {
                g_ICvar->ConsoleColorPrintf(MENU_COLOR, "CAT: %s\n", result);
            } else {
                fprintf(stderr, "CAT: %s\n", result);
            }
        } catch (...) {
            fprintf(stderr, "Error writing to console\n");
        }
    }
#endif
#endif
}

void logging::Info(const char *fmt, ...)
{
#if ENABLE_LOGGING
    if (shut_down)
        return;
    // Argument list
    va_list list;
    va_start(list, fmt);
    // Allocate buffer
    char result[1024];
    // Fill buffer
    int size = vsnprintf(result, 1024, fmt, list);
    va_end(list);
    if (size < 0)
        return;
    Log(result, false);
#endif
}

void logging::File(const char *fmt, ...)
{
#if ENABLE_LOGGING
    if (shut_down)
        return;
    // Argument list
    va_list list;
    va_start(list, fmt);
    // Allocate buffer
    char result[512];
    // Fill buffer
    int size = vsnprintf(result, 512, fmt, list);
    va_end(list);
    if (size < 0)
        return;

    Log(result, true);
#endif
}

void logging::Shutdown()
{
#if ENABLE_LOGGING
    shut_down = true;
    if (logging::handle.is_open()) {
        logging::handle << "[Shutdown] Logging system shutting down\n";
        logging::handle.flush();
        logging::handle.close();
    }
#endif
}
