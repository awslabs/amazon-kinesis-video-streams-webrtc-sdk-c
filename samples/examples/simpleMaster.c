#include "sampleSignaling.h"
#include "samplePeer.h"

INT32 main(INT32 argc, CHAR* argv[]) {
    AppCtx appCtx;
    initializeLibrary(&appCtx);
    initializeAppCtx(&appCtx, "test", "us-west-2");
    deinitializeLibrary();
}
