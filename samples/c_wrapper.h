#ifdef __cplusplus
extern "C" {
#endif

// Declare C-compatible interface
typedef void* AwsSdkHandle;

AwsSdkHandle aws_sdk_init();
void aws_sdk_cleanup(AwsSdkHandle handle);
void aws_sdk_some_operation(AwsSdkHandle handle);

#ifdef __cplusplus
}
#endif
