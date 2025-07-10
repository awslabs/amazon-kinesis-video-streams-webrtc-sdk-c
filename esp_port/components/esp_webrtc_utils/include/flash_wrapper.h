#ifndef __FLASH_WRAPPER_H__
#define __FLASH_WRAPPER_H__

#include <esp_err.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <nvs.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * This function can read from both regular files and NVS storage.
 * For NVS storage, use the path format: /nvs/<partition>/<namespace>/<key>
 * Examples:
 *   - Regular file: "/spiffs/certs/certificate.pem"
 *   - NVS key: "/nvs/nvs/rmaker/cert"
 *   - Factory NVS key: "/nvs/fctry/rmaker/cert"
 *
 * @param path File path or NVS path
 * @param buf Buffer to store data
 * @param size Size to read
 * @param offset Offset to read from (ignored for NVS paths)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_read(const char* path, void* buf, size_t size, size_t offset);

/**
 * @brief Write data to flash
 *
 * This function can write to both regular files and NVS storage.
 * For NVS storage, use the path format: /nvs/<partition>/<namespace>/<key>
 * Examples:
 *   - Regular file: "/spiffs/certs/certificate.pem"
 *   - NVS key: "/nvs/nvs/rmaker/cert"
 *   - Factory NVS key: "/nvs/fctry/rmaker/cert"
 *
 * @param path File path or NVS path
 * @param buf Buffer containing data
 * @param size Size to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_write(const char* path, const void* buf, size_t size);

/**
 * @brief Check if file exists
 *
 * This function can check both regular files and NVS keys.
 * For NVS keys, use the path format: /nvs/<partition>/<namespace>/<key>
 *
 * @param path File path or NVS path
 * @param exists Set to true if file exists
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_exists(const char* path, bool* exists);

/**
 * @brief Get file size
 *
 * This function can get the size of both regular files and NVS blobs.
 * For NVS blobs, use the path format: /nvs/<partition>/<namespace>/<key>
 *
 * @param path File path or NVS path
 * @param size Pointer to store file size
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_get_size(const char* path, size_t* size);

/**
 * @brief Get file status
 *
 * This function can get status for both regular files and NVS blobs.
 * For NVS blobs, use the path format: /nvs/<partition>/<namespace>/<key>
 * For NVS blobs, only the st_size field will be set.
 *
 * @param path File path or NVS path
 * @param st File status
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_stat(const char* path, struct stat* st);

/**
 * @brief Read certificate from flash into a buffer
 *
 * This function can read certificates from both regular files and NVS storage.
 * For NVS storage, use the path format: /nvs/<partition>/<namespace>/<key>
 *
 * @param cert_path Path to certificate file or NVS path
 * @param cert_buf Buffer to store certificate data
 * @param cert_len Size of certificate buffer
 * @param bytes_read Actual bytes read
 * @return esp_err_t ESP_OK on success
 */
esp_err_t flash_wrapper_read_cert(const char* cert_path, uint8_t* cert_buf, size_t cert_len, size_t* bytes_read);

/**
 * @brief NVS wrapper functions for direct NVS access
 */
esp_err_t flash_wrapper_nvs_read(const char* ns, const char* key, nvs_type_t type, void* value, size_t* value_size);
esp_err_t flash_wrapper_nvs_write(const char* ns, const char* key, nvs_type_t type, const void* value, size_t value_size);
esp_err_t flash_wrapper_nvs_erase(const char* ns, const char* key);
esp_err_t flash_wrapper_nvs_exists(const char* ns, const char* key, bool* exists);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_WRAPPER_H__ */
