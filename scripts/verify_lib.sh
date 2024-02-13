#!/bin/bash

# Path to the library file
LIB_FILE="$1"
# Expected architecture identifier (e.g., "ARM64", "ARM32", "x86_64", etc.)
ARCH_EXPECTED="$2"

# Associative array mapping human-readable architecture names to file command patterns
declare -A ARCH_PATTERNS
ARCH_PATTERNS=(
    ["ARM64"]="ARM aarch64"
    ["ARM32"]="ARM,"
    ["x86_64"]="x86-64"
    # Add more architectures here as needed
)

# Determine the linkage type based on file extension
if [[ $LIB_FILE == *.so ]]; then
    LINKAGE_TYPE="dynamic"
elif [[ $LIB_FILE == *.a ]]; then
    LINKAGE_TYPE="static"
else
    echo "Unsupported file extension for $LIB_FILE."
    exit 1
fi

# Use the file command to check the file type
FILE_OUTPUT=$(file $LIB_FILE)

# Function to print verification result and exit appropriately
function verify_result() {
    if $1; then
        echo "Verification passed: $LIB_FILE matches criteria for ${ARCH_PATTERNS[$ARCH_EXPECTED]} and $LINKAGE_TYPE linkage."
    else
        echo "Verification failed: $LIB_FILE does not match criteria for ${ARCH_PATTERNS[$ARCH_EXPECTED]} and $LINKAGE_TYPE linkage."
        exit 1 # Exit with an error status
    fi
}

# Validate expected architecture argument
if [[ -z "${ARCH_PATTERNS[$ARCH_EXPECTED]}" ]]; then
    echo "Unsupported or undefined architecture: $ARCH_EXPECTED"
    exit 1
fi

# Check for expected architecture in the output
ARCH_MATCH=$(echo "$FILE_OUTPUT" | grep -q "${ARCH_PATTERNS[$ARCH_EXPECTED]}"; echo $?)

# Initialize LINK_MATCH based on determined linkage type
LINK_MATCH=1 # Default to fail

if [ "$LINKAGE_TYPE" == "static" ]; then
    # Extract and check the first object file if static
    FIRST_OBJ=$(ar -t $LIB_FILE | head -n 1)
    if [ -n "$FIRST_OBJ" ]; then
        ar -x $LIB_FILE $FIRST_OBJ
        OBJ_FILE_OUTPUT=$(file $FIRST_OBJ)
        LINK_MATCH=$(echo "$OBJ_FILE_OUTPUT" | grep -Eq "relocatable"; echo $?)
        # Clean up extracted file
        rm -f $FIRST_OBJ
    fi
elif [ "$LINKAGE_TYPE" == "dynamic" ]; then
    # Check for "dynamically linked" in the output for dynamic libraries
    LINK_MATCH=$(echo "$FILE_OUTPUT" | grep -q "dynamically linked"; echo $?)
fi

# Verify both expected architecture and determined linkage type
if [ $ARCH_MATCH -eq 0 ] && [ $LINK_MATCH -eq 0 ]; then
    verify_result true
else
    verify_result false
fi
