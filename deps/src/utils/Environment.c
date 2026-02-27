#include "Include_i.h"

BOOL isEnvVarEnabled(PCHAR envVarName)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL retBool = FALSE;

    // Null or empty envVarName to GETENV is undefined behavior.
    CHK_ERR(envVarName != NULL && envVarName[0] != '\0', STATUS_NULL_ARG, "Environment variable name is NULL or empty.");

    PCHAR envVarVal = GETENV(envVarName);

    // Case-insensitive comparisons.
    retBool = envVarVal != NULL && (STRCMPI(envVarVal, "1") == 0 || STRCMPI(envVarVal, "true") == 0 || STRCMPI(envVarVal, "on") == 0);

    CleanUp:
        CHK_LOG_ERR(retStatus);
    LEAVES();
    return retBool;
}