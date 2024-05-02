#include <aws/core/Aws.h>
#include "c_wrapper.h"

extern "C" {
AwsSdkHandle aws_sdk_init() {
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    // Return some handle or pointer to indicate success
    return NULL;
}
}
