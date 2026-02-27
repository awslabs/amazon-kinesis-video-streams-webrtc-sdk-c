#include "Include_i.h"

VOID customAssert(INT64 condition, const CHAR* fileName, INT64 lineNumber, const CHAR* functionName)
{
    if (!condition) {
        fprintf(stderr, "Assertion failed in file %s, function %s, line %lld \n", fileName, functionName, lineNumber);
        abort(); // C function call
    }
}
