// Copyright 2026 . All rights reserved.
// Author: PwaOS Team
// Email: daqingzd@pwaos.com
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// desktop_app.c - Implementation of desktop methods using PwaOS SDK
// Implements basic desktop methods similar to Gnome: window manager, app manager, and etc.

#include "runtime_client_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#if defined(_WIN32)
#include <windows.h>
#define PATH_SEPARATOR '\\'
#else
#include <unistd.h>
#include <time.h>
#define PATH_SEPARATOR '/'
#endif

const void* getStructInChain(const void* base, pwaosStructureType type) {
    const pwaosBaseInStructure* b = (const pwaosBaseInStructure*)base;
    while (b) {
        if (b->type == type) {
            return (const void*)b;
        }
        b = (const pwaosBaseInStructure*)b->next;
    }
    return NULL;
}

// Helper function to sleep for milliseconds
static void sleep_ms(int milliseconds) {
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

// Global app state for callbacks
typedef struct {
    PWAOS_APP_HANDLE appHandle;
    int running;
    int page_loaded;
} DesktopState;

static DesktopState g_desktop_state = {INVALID_PWAOS_HANDLE, 1, 0};

// Print error message
static void print_error(const char* function_name, pwaosResult result) {
    char err_str[PWAOS_MAX_RESULT_NAME_SIZE];
    pwaos_resultToString(result, err_str);
    printf("[ERROR] %s failed: %s (code: %d)\n", function_name, err_str, result);
}

// Print usage information
static void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS] [URL]\n\n", program_name);
    printf("PwaOS Desktop - A desktop application using the PwaOS SDK\n");
    printf("Default: http://wwww.pwaos.net/webapps/desktop/index.html\n\n");
    printf("Options:\n");
    printf("  -h, --help           Show this help message and exit\n");
    printf("  -v, --version        Show version information and exit\n");
    printf("  -u, --user-agent UA  Set custom user agent string\n");
    printf("  -p, --proxy  TYPE    Set proxy type: system, direct, manual\n");
    printf("  -f, --fps    FPS     Set target frame rate (default: 60)\n");
    printf("  -d, --density DENS   Set display density/DPI (default: 1.0)\n");
    printf("  --remote-debugging-port PORT   Set devtools port number\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s\n", program_name);
    printf("  %s https://www.google.com\n", program_name);
    printf("  %s --fps 144 http://wwww.pwaos.net/webapps/desktop/index.html\n", program_name);
    printf("  %s --user-agent \"Mozilla/5.0\" --proxy system\n", program_name);
    printf("\n");
    printf("Report bugs to: daqingzd@pwaos.com\n");
}

// Print version information
static void print_version() {
    printf("PwaOS Desktop App v1.0.0\n");
    printf("Using PwaOS Runtime SDK v0.2.1\n");
}

// Parse command line arguments
typedef struct {
    const char* url;
    char user_agent[512];
    char proxy_type[32];
    int fps;
    float density;
    int show_help;
    int show_version;
} DesktopArgs;

static void init_args(DesktopArgs* args);  // forward — defined below

static void init_args(DesktopArgs* args) {
    args->url = "https://www.pwaos.net/webapps/desktop/index.html";
    args->user_agent[0] = '\0';
    args->proxy_type[0] = '\0';
    args->fps = 60;
    args->density = 1.0f;
    args->show_help = 0;
    args->show_version = 0;
}

static int parse_args(DesktopArgs* args, int argc, char** argv) {
    init_args(args);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args->show_help = 1;
            return 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            args->show_version = 1;
            return 1;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--user-agent") == 0) {
            if (i + 1 < argc) {
                strncpy(args->user_agent, argv[++i], sizeof(args->user_agent) - 1);
                args->user_agent[sizeof(args->user_agent) - 1] = '\0';
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--proxy") == 0) {
            if (i + 1 < argc) {
                strncpy(args->proxy_type, argv[++i], sizeof(args->proxy_type) - 1);
                args->proxy_type[sizeof(args->proxy_type) - 1] = '\0';
            }
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fps") == 0) {
            if (i + 1 < argc) {
                args->fps = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--density") == 0) {
            if (i + 1 < argc) {
                args->density = atof(argv[++i]);
            }
        } else if (argv[i][0] != '-') {
            // Assume it's a URL
            args->url = argv[i];
        }
    }
    return 0;
}

/**
 * Desktop implementation using PwaOS SDK
 */
int main(int argc, char** argv) {
    DesktopArgs args;

    // Parse command line arguments first
    if (parse_args(&args, argc, argv)) {
        // Early return was requested (--help or --version)
        if (args.show_help) {
            print_usage(argv[0]);
            return 0;
        }
        if (args.show_version) {
            print_version();
            return 0;
        }
    }

    printf("PwaOS Desktop App starting...\n");
    printf("URL: %s | FPS: %d\n",
           args.url, args.fps);

    // Set runtime executable path
#if defined(_WIN32)
    pwaos_setRuntimeExecutablePath(PWAOS_SDK_DIR"\\runtime");
#else
    pwaos_setRuntimeExecutablePath(PWAOS_SDK_DIR"/runtime");
#endif

    // Initialize
    pwaosInitializeInfo init_info = {
        .type = PWAOS_TYPE_INITIALIZE_INFO,
        .next = NULL,
        .argc = argc,
        .argv = argv,
        .use_dedicated_thread = false,
    };

    pwaosResult result = pwaos_init(&init_info);
    if (result != PWAOS_RESULT_SUCCESS) {
        print_error("pwaos_init", result);
        return -1;
    }
    printf("[OK] Runtime initialized\n");

    // Create instance
    pwaosInstanceCreateInfo create_info = {
        .type = PWAOS_TYPE_INSTANCE_CREATE_INFO,
        .next = NULL,
        .runtimeType = runtimeTypePwa,
        .serverUrl = "http://localhost",
        .dataPath = "",
    };

    // Set data path
    if (argc > 0 && access(argv[0], F_OK) == 0) {
        char exe_path[PWAOS_MAX_FILE_PATH_LEN];
#if defined(_WIN32)
        _fullpath(exe_path, argv[0], PWAOS_MAX_FILE_PATH_LEN);
#else
        realpath(argv[0], exe_path);
#endif
        const char* last_sep = strrchr(exe_path, PATH_SEPARATOR);
        if (last_sep) {
            int dir_len = last_sep - exe_path;
            create_info.dataPath[0] = '\0';
            strncat(create_info.dataPath, exe_path, dir_len);
            strcat(create_info.dataPath, "/data");
        }
    }

    PWAOS_INSTANCE_HANDLE instance_handle = INVALID_PWAOS_HANDLE;
    result = pwaos_createInstance(&create_info, &instance_handle);
    if (result != PWAOS_RESULT_SUCCESS) {
        print_error("pwaos_createInstance", result);
        pwaos_shutDown();
        return -1;
    }
    printf("[OK] Instance created\n");

    // Wait for instance to be ready
    int instance_ready = 0;
    while (!instance_ready && g_desktop_state.running) {
        pwaosEventDataBuffer buffer;
        if (PWAOS_RESULT_SUCCESS == pwaosInstance_pollEvent(instance_handle, &buffer)) {
            if (buffer.type == PWAOS_TYPE_EVENT_DATA_INSTANCE_STATUS_CHANGED) {
                const pwaosEventDataInstanceStatusChanged* event =
                    (const pwaosEventDataInstanceStatusChanged*)&buffer;
                if (event->instance == instance_handle) {
                    if (event->status == instanceStatusWorking) {
                        instance_ready = 1;
                    } else if (event->status == instanceStatusStopping ||
                               event->status < 0) {
                        printf("[WARN] Instance status: %d\n", event->status);
                    }
                }
            }
        } else {
            sleep_ms(10);
        }
    }

    if (!instance_ready) {
        printf("[ERROR] Instance did not become ready\n");
        pwaos_destroyInstance(instance_handle);
        pwaos_shutDown();
        return -1;
    }
    printf("[OK] Instance is working\n");

    // Create desktop app with appTypeDesktop
    pwaosAppCreateInfo app_info = {
        .type = PWAOS_TYPE_PWA_CREATE_INFO,
        .next = NULL,
        .appType = appTypeDesktop,  // Run as desktop pwa app
        .appUrl = args.url,
        .width = 0,
        .height = 0
    };
    strcpy(app_info.appUrl, args.url);

    // Setup custom user agent (from args or default)
    pwaosUserAgentInfo ua_info = {
        .type = PWAOS_TYPE_USER_AGENT_INFO,
        .next = NULL,
    };
    if (args.user_agent[0] != '\0') {
        strncpy(ua_info.userAgent, args.user_agent, MAX_URI_LEN - 1);
        ua_info.userAgent[MAX_URI_LEN - 1] = '\0';
    } else {
        snprintf(ua_info.userAgent, MAX_URI_LEN,
                 "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                 "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    }
    app_info.next = &ua_info;

    // Setup proxy
    pwaosNetworkProxyInfo proxy_info = {
        .type = PWAOS_TYPE_NETWORK_PROXY_INFO,
        .next = NULL,
        .proxyType = PWAOS_NETWORK_PROXY_TYPE_SYSTEM,
    };

    // Set proxy type from args (default: SYSTEM)
    if (args.proxy_type[0] != '\0') {
        if (strcmp(args.proxy_type, "system") == 0) {
            proxy_info.proxyType = PWAOS_NETWORK_PROXY_TYPE_SYSTEM;
        } else if (strcmp(args.proxy_type, "direct") == 0) {
            proxy_info.proxyType = PWAOS_NETWORK_PROXY_TYPE_DIRECT;
        } else if (strcmp(args.proxy_type, "auto") == 0) {
            proxy_info.proxyType = PWAOS_NETWORK_PROXY_TYPE_AUTO_DETECT;
        }
    }
    ua_info.next = &proxy_info;

    PWAOS_APP_HANDLE app_handle = INVALID_PWAOS_HANDLE;
    result = pwaosInstance_createPwa(instance_handle, &app_info, &app_handle);
    if (result != PWAOS_RESULT_SUCCESS) {
        print_error("pwaosInstance_createPwa", result);
        pwaos_destroyInstance(instance_handle);
        pwaos_shutDown();
        return -1;
    }
    g_desktop_state.appHandle = app_handle;
    printf("[OK] Desktop app created with URL: %s\n", app_info.appUrl);

    // =========================================================
    // Desktop Methods Implementation
    // =========================================================

    // 1. Get/Set User Agent (direct SDK call, for comparison)
    printf("\n--- 1. User Agent Test ---\n");
    {
        pwaosUserAgentInfo ua_info = {
            .type = PWAOS_TYPE_USER_AGENT_INFO,
            .next = NULL,
        };

        // Set custom user agent (from args or default)
        if (args.user_agent[0] != '\0') {
            strncpy(ua_info.userAgent, args.user_agent, MAX_URI_LEN - 1);
            ua_info.userAgent[MAX_URI_LEN - 1] = '\0';
        } else {
            snprintf(ua_info.userAgent, MAX_URI_LEN, 
                     "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        }
        result = pwaosApp_setUserAgent(app_handle, &ua_info);
        if (result == PWAOS_RESULT_SUCCESS) {
            printf("[OK] User agent set\n");
        } else {
            printf("[INFO] setUserAgent: %d\n", result);
        }

        // Get user agent
        memset(ua_info.userAgent, 0, sizeof(ua_info.userAgent));
        result = pwaosApp_getUserAgent(app_handle, &ua_info);
        if (result == PWAOS_RESULT_SUCCESS) {
            printf("[INFO] Current user agent: %s\n", ua_info.userAgent);
        }
    }

    // 5. FPS Control
    printf("\n--- 5. FPS Control Test ---\n");
    {
        pwaosAppFpsInfo fps_info = {
            .type = PWAOS_TYPE_APP_FPS_INFO,
            .next = NULL,
            .fps = (float)args.fps
        };
        result = pwaosApp_setFPS(app_handle, &fps_info);
        if (result == PWAOS_RESULT_SUCCESS) {
            printf("[OK] FPS set to: %.1f\n", fps_info.fps);
        } else {
            printf("[INFO] setFPS: %d\n", result);
        }
    }

    // 6. Network Proxy Settings
    printf("\n--- 6. Network Proxy Test ---\n");
    {
        pwaosNetworkProxyInfo proxy_info = {
            .type = PWAOS_TYPE_NETWORK_PROXY_INFO,
            .next = NULL,
            .proxyType = PWAOS_NETWORK_PROXY_TYPE_SYSTEM,
        };

        // Set proxy type from args (default: SYSTEM)
        if (args.proxy_type[0] != '\0') {
            if (strcmp(args.proxy_type, "system") == 0) {
                proxy_info.proxyType = PWAOS_NETWORK_PROXY_TYPE_SYSTEM;
            } else if (strcmp(args.proxy_type, "direct") == 0) {
                proxy_info.proxyType = PWAOS_NETWORK_PROXY_TYPE_DIRECT;
            } else if (strcmp(args.proxy_type, "auto") == 0) {
                proxy_info.proxyType = PWAOS_NETWORK_PROXY_TYPE_AUTO_DETECT;
            }
        }

        // Set system proxy (use system settings)
        result = pwaosApp_setNetworkProxy(app_handle, &proxy_info);
        if (result == PWAOS_RESULT_SUCCESS) {
            printf("[OK] Network proxy set to SYSTEM\n");
        } else {
            printf("[INFO] setNetworkProxy: %d\n", result);
        }

        // Get current proxy settings
        result = pwaosApp_getNetworkProxy(app_handle, &proxy_info);
        if (result == PWAOS_RESULT_SUCCESS) {
            printf("[INFO] Current proxy type: %d\n", proxy_info.proxyType);
        }
    }

    // 7. Density Control (for responsive design testing)
    printf("\n--- 7. Display Density Test ---\n");
    {
        float density = args.density;
        result = pwaosApp_setDensity(app_handle, density);
        if (result == PWAOS_RESULT_SUCCESS) {
            printf("[OK] Display density set to: %.2f\n", density);
        } else {
            printf("[INFO] setDensity: %d\n", result);
        }

        float out_density = 0.0f;
        result = pwaosApp_getDensity(app_handle, &out_density);
        if (result == PWAOS_RESULT_SUCCESS) {
            printf("[INFO] Current display density: %.2f\n", out_density);
        }
    }

    // 8. DevTools URI (for debugging)
    printf("\n--- 8. DevTools URI Test ---\n");
    {
        pwaosUriInfo uri_info = {
            .type = PWAOS_TYPE_DEVTOOLS_URI_INFO,
            .next = NULL,
        };
        result = pwaosApp_getDevToolsUri(app_handle, &uri_info);
        if (result == PWAOS_RESULT_SUCCESS && strlen(uri_info.url) > 0) {
            printf("[OK] DevTools URI: %s\n", uri_info.url);
        } else {
            printf("[INFO] DevTools not available or not yet ready\n");
        }
    }

    // =========================================================
    // Main event loop - wait for app to close
    // =========================================================
    printf("\n--- Main Event Loop ---\n");
    printf("Desktop is running. Waiting for events...\n");
    printf("Press Ctrl+C to exit or close the Desktop window.\n");

    while (g_desktop_state.running) {
        pwaosEventDataBuffer buffer;
        if (PWAOS_RESULT_SUCCESS == pwaosInstance_pollEvent(instance_handle, &buffer)) {
            if (buffer.type == PWAOS_TYPE_EVENT_DATA_INSTANCE_STATUS_CHANGED) {
                const pwaosEventDataInstanceStatusChanged* event =
                    (const pwaosEventDataInstanceStatusChanged*)&buffer;
                if (event->instance == instance_handle) {
                    if (event->status == instanceStatusStopping ||
                        event->status < 0) {
                        printf("[EXIT_INSTANCE] Instance status changed: %d\n", event->status);
                        g_desktop_state.running = 0;
                        break;
                    }
                }
            } else if (buffer.type == PWAOS_TYPE_EVENT_DATA_APP_STATUS_CHANGED) {
                const pwaosEventDataAppStatusChanged* event =
                    (const pwaosEventDataAppStatusChanged*)&buffer;
                if (event->app == app_handle) {
                    printf("[APP_STATUS] status=%d\n", event->status);
                    if (event->status == appStatusStopping) {
                        printf("[EXIT_APP] App received appStatusStopping, exiting\n");
                        const pwaosAppStoppingInfoExt* info = (const pwaosAppStoppingInfoExt*)getStructInChain(event, PWAOS_TYPE_APP_STOPPING_INFO_EXT);
                        if (info && (info->stoppingReason == PWAOS_APP_STOPPING_REASON_NETWORK || info->stoppingReason == PWAOS_APP_STOPPING_REASON_CRASH))
                          continue;

                        g_desktop_state.running = 0;
                        break;
                    } else if (event->status == appStatusReady) {
                        printf("[INFO] Page loaded and ready\n");
                        g_desktop_state.page_loaded = 1;
                    }
                }
            }
        } else {
            sleep_ms(10);
        }
    }

    // Cleanup
    printf("\n--- Cleanup ---\n");
    pwaosInstance_destroyPwa(app_handle);
    printf("[OK] App destroyed\n");

    pwaos_destroyInstance(instance_handle);
    printf("[OK] Instance destroyed\n");

    pwaos_shutDown();
    printf("[OK] Runtime shutdown\n");

    printf("PwaOS Desktop App exited\n");
    return 0;
}
