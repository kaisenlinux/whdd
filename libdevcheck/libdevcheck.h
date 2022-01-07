#ifndef LIBDEVCHECK_H
#define LIBDEVCHECK_H

#include <time.h>

#include "version.h"
#define WHDD_ABOUT "WHDD - disk drives diagnostic tool\n" \
    "Revision " WHDD_VERSION "\n" \
    "License: GNU GPLv3\n" \


#include "objects_def.h"
#include "device.h"
#include "procedure.h"
#include "log.h"
#include "render.h"

extern clockid_t DC_BEST_CLOCK;

// this should not be accessed from applications, for internal usage only
struct dc_ctx {
    DC_Procedure *procedure_list;
    int nb_procedures;
    DC_Renderer *renderer_list;
    enum DC_LogLevel log_level;
    void (*log_func)(void *priv, enum DC_LogLevel level, const char* fmt, va_list vl);
    void *logger_priv;
};

extern DC_Ctx *dc_ctx_global;

struct dc_dev_list {
    DC_Dev *arr;
    int arr_size;
};

/**
 * This func initializes underlying libraries,
 * internal structures connectivity object, and so on.
 * Call this once at app beginning
 * And store returned pointer for further use of lib.
 */
int dc_init(void);
void dc_finish(void);

/**
 * Return array of testable block devices
 */
DC_DevList *dc_dev_list(void);
void dc_dev_list_free(DC_DevList *list);
int dc_dev_list_size(DC_DevList *list);
DC_Dev *dc_dev_list_get_entry(DC_DevList *list, int index);

#endif // LIBDEVCHECK_H
