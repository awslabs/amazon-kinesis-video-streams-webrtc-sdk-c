#include "Include_i.h"

/**
 * Read a file from the given full/relative filePath into the memory area pointed to by pBuffer.
 * Specifying NULL in pBuffer will return the size of the file.
 *
 * Parameters:
 *     filePath - file path to read from
 *     binMode  - TRUE to read file stream as binary; FALSE to read as a normal text file
 *     pBuffer  - buffer to write contents of the file to. If NULL return the size in pSize.
 *     pSize    - destination PUINT64 to store the size of the file when pBuffer is NULL;
 */
STATUS readFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, PUINT64 pSize)
{
    ENTERS();
    UINT64 fileLen;
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;

    CHK(filePath != NULL && pSize != NULL, STATUS_NULL_ARG);

    DLOGV("Opening file: %s", filePath);
    fp = FOPEN(filePath, binMode ? "rb" : "r");

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    // Get the size of the file

    // Use Windows-specific _fseeki64 and _ftelli64 as the traditional fseek and ftell are non-compliant on systems that do not provide
    // the same guarantees as POSIX. On these systems, setting the file position indicator to the
    // end of the file using fseek() is not guaranteed to work for a binary stream, and consequently,
    // the amount of memory allocated may be incorrect, leading to a potential vulnerability.
    FSEEK(fp, 0, SEEK_END);
    fileLen = FTELL(fp);

    if (pBuffer == NULL) {
        // requested the length - set and early return
        *pSize = fileLen;
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Validate the buffer size
    CHK(fileLen <= *pSize, STATUS_BUFFER_TOO_SMALL);

    // Read the file into memory buffer
    FSEEK(fp, 0, SEEK_SET);

    // fread would either return 1, i.e, the number of objects we've requested it to read
    // or it would run into end-of-file / error.
    // fread does not distinguish between end-of-file and error,
    // and callers must use feof and ferror to determine which occurred.
    if (FREAD(pBuffer, (SIZE_T) fileLen, 1, fp) != 1) {
        CHK(FEOF(fp), STATUS_READ_FILE_FAILED);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    LEAVES();
    return retStatus;
}

/**
 * Read a section of the file from the given full/relative filePath into the memory area pointed to by pBuffer.
 *
 * NOTE: The buffer should be large enough to read the section.
 *
 * Parameters:
 *     filePath - file path to read from
 *     binMode  - TRUE to read file stream as binary; FALSE to read as a normal text file
 *     pBuffer  - buffer to write contents of the file to. Non-null
 *     offset   - Offset into the file to start reading from.
 *     readSize - The number of bytes to read from the file.
 */
STATUS readFileSegment(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 readSize)
{
    ENTERS();
    UINT64 fileLen;
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;
    INT32 result = 0;

    CHK(filePath != NULL && pBuffer != NULL && readSize != 0, STATUS_NULL_ARG);

    DLOGV("Opening file: %s", filePath);
    fp = FOPEN(filePath, binMode ? "rb" : "r");

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    // Get the size of the file

    // Use Windows-specific _fseeki64 and _ftelli64 as the traditional fseek and ftell are non-compliant on systems that do not provide
    // the same guarantees as POSIX. On these systems, setting the file position indicator to the
    // end of the file using fseek() is not guaranteed to work for a binary stream, and consequently,
    // the amount of memory allocated may be incorrect, leading to a potential vulnerability.
    FSEEK(fp, 0, SEEK_END);
    fileLen = FTELL(fp);

    // Check if we are trying to read past the end of the file
    CHK(offset + readSize <= fileLen, STATUS_READ_FILE_FAILED);

    // Set the offset and read the file content
    result = FSEEK(fp, (UINT32) offset, SEEK_SET);

    CHK(result == 0, STATUS_READ_FILE_FAILED);

    // fread would either return 1, i.e, the number of objects we've requested it to read
    // or it would run into end-of-file / error.
    // fread does not distinguish between end-of-file and error,
    // and callers must use feof and ferror to determine which occurred.
    if (FREAD(pBuffer, (SIZE_T) readSize, 1, fp) != 1) {
        CHK(FEOF(fp), STATUS_READ_FILE_FAILED);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    LEAVES();
    return retStatus;
}

/**
 * Write contents pointed to by pBuffer to the given filePath. Logs will be printed within this function.
 * WARNING: This must NOT be called by the file logger.
 *
 * Parameters:
 *     filePath - file path to write to
 *     binMode  - TRUE to read file stream as binary; FALSE to read as a normal text file
 *     append   - TRUE to append; FALSE to overwrite
 *     pBuffer  - memory location whose contents should be written to the file
 *     size     - number of bytes that should be written to the file
 */
STATUS writeFileWithLogging(PCHAR filePath, BOOL binMode, BOOL append, PBYTE pBuffer, UINT64 size)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;

    CHK(filePath != NULL && pBuffer != NULL, STATUS_NULL_ARG);

    DLOGV("Opening file: %s", filePath);
    fp = FOPEN(filePath, binMode ? (append ? "ab" : "wb") : (append ? "a" : "w"));

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    // Write the buffer to the file
    CHK(FWRITE(pBuffer, (SIZE_T) size, 1, fp) == 1, STATUS_WRITE_TO_FILE_FAILED);

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    LEAVES();
    return retStatus;
}

/**
 * Write contents pointed to by pBuffer to the given filePath.
 * WARNING: Logs must NOT be printed within this function.
 * Parameters:
 *     filePath - file path to write to
 *     binMode  - TRUE to read file stream as binary; FALSE to read as a normal text file
 *     append   - TRUE to append; FALSE to overwrite
 *     pBuffer  - memory location whose contents should be written to the file
 *     size     - number of bytes that should be written to the file
 */
STATUS writeFile(PCHAR filePath, BOOL binMode, BOOL append, PBYTE pBuffer, UINT64 size)
{
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;

    CHK(filePath != NULL && pBuffer != NULL, STATUS_NULL_ARG);

    fp = FOPEN(filePath, binMode ? (append ? "ab" : "wb") : (append ? "a" : "w"));

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    // Write the buffer to the file
    CHK(FWRITE(pBuffer, (SIZE_T) size, 1, fp) == 1, STATUS_WRITE_TO_FILE_FAILED);

CleanUp:

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    return retStatus;
}

/**
 * Write contents pointed to by pBuffer to the given filePath.
 *
 * Parameters:
 *     filePath - file path to write to
 *     binMode  - TRUE to read file stream as binary; FALSE to read as a normal text file
 *     pBuffer  - memory location whose contents should be written to the file
 *     offset   - Offset to start writing from
 *     size     - number of bytes that should be written to the file
 */
STATUS updateFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 size)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;
    UINT32 i;
    PBYTE pCurPtr;

    CHK(filePath != NULL && pBuffer != NULL, STATUS_NULL_ARG);

    DLOGV("Opening file: %s", filePath);
    fp = FOPEN(filePath, binMode ? "rb+" : "r+");

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    CHK(0 == FSEEK(fp, (UINT32) offset, SEEK_SET), STATUS_INVALID_OPERATION);

    for (i = 0, pCurPtr = pBuffer + offset; i < size; i++, pCurPtr++) {
        CHK(EOF != FPUTC(*pCurPtr, fp), STATUS_WRITE_TO_FILE_FAILED);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    LEAVES();
    return retStatus;
}

/**
 * Gets the file length of the given filePath.
 *
 * Parameters:
 *     filePath - file path whose file length should be computed
 *     pLength  - Returns the size of the file in bytes
 *
 * Returns:
 *     STATUS of the operation
 */
STATUS getFileLength(PCHAR filePath, PUINT64 pLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK_STATUS(readFile(filePath, TRUE, NULL, pLength));

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

/**
 * Sets the file length of the given filePath.
 *
 * Parameters:
 *     filePath - file path whose file length should be computed
 *     length  - Sets the size of the file in bytes
 *
 * Returns:
 *     STATUS of the operation
 */
STATUS setFileLength(PCHAR filePath, UINT64 length)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 retVal, errCode, fileDesc;

    CHK(filePath != NULL, STATUS_NULL_ARG);

#if defined __WINDOWS_BUILD__
    fileDesc = _open(filePath, _O_BINARY | _O_RANDOM | _O_RDWR, 0);

    if (fileDesc != -1) {
        retVal = _chsize_s(fileDesc, length);

        if (retVal != 0) {
            retVal = -1;
            errCode = errno;
        }

        _close(fileDesc);
    } else {
        retVal = -1;
        errCode = errno;
    }

#else
    UNUSED_PARAM(fileDesc);
    retVal = truncate(filePath, length);
    errCode = errno;
#endif

    if (retVal == -1) {
        switch (errCode) {
            case EACCES:
                retStatus = STATUS_DIRECTORY_ACCESS_DENIED;
                break;

            case ENOENT:
                retStatus = STATUS_DIRECTORY_MISSING_PATH;
                break;

            case EINVAL:
                retStatus = STATUS_INVALID_ARG_LEN;
                break;

            case EISDIR:
            case EBADF:
                retStatus = STATUS_INVALID_ARG;
                break;

            case ENOSPC:
                retStatus = STATUS_NOT_ENOUGH_MEMORY;
                break;

            default:
                retStatus = STATUS_INVALID_OPERATION;
        }
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

/**
 * Checks if the file or directory exists with a given full or relative path
 *
 * Parameters:
 *      filePath - file path to check
 *      pExists - TRUE if the file exists
 */
STATUS fileExists(PCHAR filePath, PBOOL pExists)
{
    ENTERS();
    if (filePath == NULL || pExists == NULL) {
        LEAVES();
        return STATUS_NULL_ARG;
    }

    struct GLOBAL_STAT st;
    INT32 result = FSTAT(filePath, &st);
    *pExists = (result == 0);

    LEAVES();
    return STATUS_SUCCESS;
}

/**
 * Creates/overwrites a new file with a given size
 *
 * Parameters:
 *      filePath - file path to check
 *      size - The size of the newly created file
 */
STATUS createFile(PCHAR filePath, UINT64 size)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;

    CHK(filePath != NULL, STATUS_NULL_ARG);

    DLOGD("Creating file: %s", filePath);
    fp = FOPEN(filePath, "w+b");
    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    if (size != 0) {
        CHK(0 == FSEEK(fp, (UINT32) (size - 1), SEEK_SET), STATUS_INVALID_OPERATION);
        CHK(0 == FPUTC(0, fp), STATUS_INVALID_OPERATION);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    LEAVES();
    return retStatus;
}
