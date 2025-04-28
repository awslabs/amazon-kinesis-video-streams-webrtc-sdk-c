#ifndef __FLASH_WRAPPER_H__
#define __FLASH_WRAPPER_H__

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// // Maximum size for certificate buffers
// #define MAX_CERT_SIZE (4096)

/**
 * @brief Initialize flash wrapper
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_init(void);

/**
 * @brief Deinitialize flash wrapper
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_deinit(void);

/**
 * @brief Read data from flash
 *
 * @param path File path
 * @param buf Buffer to store data
 * @param size Size to read
 * @param offset Offset to read from
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_read(const char* path, void* buf, size_t size, size_t offset);

/**
 * @brief Write data to flash
 *
 * @param path File path
 * @param buf Buffer containing data
 * @param size Size to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_write(const char* path, const void* buf, size_t size);

/**
 * @brief Check if file exists
 *
 * @param path File path
 * @param exists Set to true if file exists
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_exists(const char* path, bool* exists);

/**
 * @brief Get file size
 *
 * @param path File path
 * @param size Pointer to store file size
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_get_size(const char* path, size_t* size);

/**
 * @brief Get file status
 *
 * @param path File path
 * @param st File status
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_stat(const char* path, struct stat* st);

// /**
//  * @brief Read certificate from flash into a buffer
//  *
//  * @param cert_path Path to certificate file
//  * @param cert_buf Buffer to store certificate data
//  * @param cert_len Size of certificate buffer
//  * @param bytes_read Actual bytes read
//  * @return esp_err_t ESP_OK on success
//  */
// esp_err_t flash_wrapper_read_cert(const char* cert_path, uint8_t* cert_buf, size_t cert_len, size_t* bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_WRAPPER_H__ */
