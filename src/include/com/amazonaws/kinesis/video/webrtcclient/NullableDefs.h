/*
 * Header file to define nullable values for common data types
 */

#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_NULLABLEDEFS__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_NULLABLEDEFS__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic pop

#define NULLABLE_SET_EMPTY(a)                                                                                                                        \
    do {                                                                                                                                             \
        (a).isNull = TRUE;                                                                                                                           \
    } while (FALSE)

#define NULLABLE_CHECK_EMPTY(a) ((a).isNull == TRUE)

#define NULLABLE_SET_VALUE(a, val)                                                                                                                   \
    do {                                                                                                                                             \
        (a).isNull = FALSE;                                                                                                                          \
        (a).value = val;                                                                                                                             \
    } while (FALSE)

typedef struct {
    BOOL isNull;
    BOOL value;
} NullableBool;

typedef struct {
    BOOL isNull;
    UINT8 value;
} NullableUint8;

typedef struct {
    BOOL isNull;
    INT8 value;
} NullableInt8;

typedef struct {
    BOOL isNull;
    UINT16 value;
} NullableUint16;

typedef struct {
    BOOL isNull;
    INT16 value;
} NullableInt16;

typedef struct {
    BOOL isNull;
    UINT32 value;
} NullableUint32;

typedef struct {
    BOOL isNull;
    INT32 value;
} NullableInt32;

typedef struct {
    BOOL isNull;
    UINT64 value;
} NullableUint64;

typedef struct {
    BOOL isNull;
    INT64 value;
} NullableInt64;

typedef struct {
    BOOL isNull;
    FLOAT value;
} NullableFloat;

typedef struct {
    BOOL isNull;
    DOUBLE value;
} NullableDouble;

typedef struct {
    BOOL isNull;
    LDOUBLE value;
} NullableLongDouble;

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_NULLABLEDEFS__ */
