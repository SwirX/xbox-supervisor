#ifndef XENOLITH_TYPES_H
#define XENOLITH_TYPES_H

#include <stdint.h>

#define XENOLITH_API_VERSION 1

typedef enum {
    XL_OK = 0,
    XL_ERR_NOT_FOUND,
    XL_ERR_INVALID,
    XL_ERR_IO,
    XL_ERR_PERMISSION,
    XL_ERR_BUSY
} XlResult;

typedef enum {
    APP_STOPPED,
    APP_LOADING,
    APP_RUNNING,
    APP_PAUSED,
    APP_CRASHED
} AppState;

#endif
