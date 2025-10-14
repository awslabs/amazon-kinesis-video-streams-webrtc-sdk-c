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
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic pop
#endif

/////////////////////////////////////////////////////
/// Nullable Macros
/////////////////////////////////////////////////////

/*! \addtogroup NullableMacroUtilities
 * @brief Use this to set the value to NULL. If value field is set after calling this, it is ignored
 * @{
 */
#define NULLABLE_SET_EMPTY(a)                                                                                                                        \
    do {                                                                                                                                             \
        (a).isNull = TRUE;                                                                                                                           \
    } while (FALSE)

/**
 * @brief Used to check if the value is NULL. If yes, the value field should not be populated
 */
#define NULLABLE_CHECK_EMPTY(a) ((a).isNull == TRUE)

/**
 * @brief Used to set a value. The macro sets the isNull flag to FALSE and sets the passed in value
 */
#define NULLABLE_SET_VALUE(a, val)                                                                                                                   \
    do {                                                                                                                                             \
        (a).isNull = FALSE;                                                                                                                          \
        (a).value = val;                                                                                                                             \
    } while (FALSE)
/*!@} */

/////////////////////////////////////////////////////
/// Nullable data type Related structures
/////////////////////////////////////////////////////

/*! \addtogroup NullableStructures
 * @brief Custom data type to allow setting BOOL data type to NULL since C does
 * not support setting basic data types to NULL
 * @{
 */
typedef struct {
    BOOL isNull; //!< If this value is set, the value field will be ignored
    BOOL value;  //!< This value is used only if isNull is not set. Can be set to TRUE/FALSE
} NullableBool;

/**
 * @brief Custom data type to allow setting UINT8 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull; //!< If this value is set, the value field will be ignored
    UINT8 value; //!< This value is used only if isNull is not set. Can be set to a unsigned 8 bit value
} NullableUint8;

/**
 * @brief Custom data type to allow setting INT8 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull; //!< If this value is set, the value field will be ignored
    INT8 value;  //!< This value is used only if isNull is not set. Can be set to a signed 8 bit value
} NullableInt8;

/**
 * @brief Custom data type to allow setting UINT16 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull;  //!< If this value is set, the value field will be ignored
    UINT16 value; //!< This value is used only if isNull is not set. Can be set to a unsigned 16 bit value
} NullableUint16;

/**
 * @brief Custom data type to allow setting INT16 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull; //!< If this value is set, the value field will be ignored
    INT16 value; //!< This value is used only if isNull is not set. Can be set to a signed 16 bit value
} NullableInt16;

/**
 * @brief Custom data type to allow setting UINT32 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull;  //!< If this value is set, the value field will be ignored
    UINT32 value; //!< This value is used only if isNull is not set. Can be set to a unsigned 32 bit value
} NullableUint32;

/**
 * @brief Custom data type to allow setting INT32 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull; //!< If this value is set, the value field will be ignored
    INT32 value; //!< This value is used only if isNull is not set. Can be set to a signed 32 bit value
} NullableInt32;

/**
 * @brief Custom data type to allow setting UINT64 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull;  //!< If this value is set, the value field will be ignored
    UINT64 value; //!< This value is used only if isNull is not set. Can be set to a unsigned 64 bit value
} NullableUint64;

/**
 * @brief Custom data type to allow setting INT64 data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull; //!< If this value is set, the value field will be ignored
    INT64 value; //!< This value is used only if isNull is not set. Can be set to a signed 64 bit value
} NullableInt64;

/**
 * @brief Custom data type to allow setting FLOAT data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull; //!< If this value is set, the value field will be ignored
    FLOAT value; //!< This value is used only if isNull is not set. Can be set to a float value
} NullableFloat;

/**
 * @brief Custom data type to allow setting DOUBLE data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull;  //!< If this value is set, the value field will be ignored
    DOUBLE value; //!< This value is used only if isNull is not set. Can be set to a double value
} NullableDouble;

/**
 * @brief Custom data type to allow setting LONG DOUBLE data type to NULL since C does
 * not support setting basic data types to NULL
 */
typedef struct {
    BOOL isNull;   //!< If this value is set, the value field will be ignored
    LDOUBLE value; //!< This value is used only if isNull is not set. Can be set to a long double value
} NullableLongDouble;
/*!@} */

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_NULLABLEDEFS__ */
