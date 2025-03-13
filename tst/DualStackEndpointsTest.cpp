#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DualStackEndpointsTest : public WebRtcClientTestBase {
};

TEST_F(DualStackEndpointsTest, customControlPlaneEndpointBasicCase)
{
    STATUS retStatus;
    ChannelInfo originalChannelInfo;
    memset(&originalChannelInfo, 0, sizeof(originalChannelInfo));

    ChannelInfo validatedChannelInfo;
    memset(&validatedChannelInfo, 0, sizeof(validatedChannelInfo));
    PChannelInfo pValidatedChannelInfo = &validatedChannelInfo;

    CHAR originalControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN] = "https://kinesisvideo.us-west-2.api.aws";
    CHAR validatedControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN]  =  {0};

    // pChannelName must be set to make the createValidateChannelInfo call.
    originalChannelInfo.pChannelName = "TestChannelName";

    // Make a deep copy of originalControlPlaneUrl into validatedControlPlaneUrl.
    strncpy(validatedControlPlaneUrl, originalControlPlaneUrl, MAX_CONTROL_PLANE_URI_CHAR_LEN);
    validatedControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN - 1] = '\0';

    originalChannelInfo.pControlPlaneUrl = (PCHAR) validatedControlPlaneUrl;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&originalChannelInfo, &pValidatedChannelInfo));

    // Validate the ChanelInfo's Control Plane URL against originalControlPlaneUrl in case createValidateChannelInfo
    //      modified validatedControlPlaneUrl.
    EXPECT_STREQ(pValidatedChannelInfo->pControlPlaneUrl, (PCHAR) originalControlPlaneUrl);
}

TEST_F(DualStackEndpointsTest, customControlPlaneEndpointEdgeCases)
{
    STATUS retStatus;
    ChannelInfo originalChannelInfo;
    memset(&originalChannelInfo, 0, sizeof(originalChannelInfo));

    ChannelInfo validatedChannelInfo;
    memset(&validatedChannelInfo, 0, sizeof(validatedChannelInfo));
    PChannelInfo pValidatedChannelInfo = &validatedChannelInfo;

    // TODO: Verify why we use MAX_URI_CHAR_LEN instead of MAX_CONTROL_PLANE_URI_CHAR_LEN for the custom URL.
    // MAX_URI_CHAR_LEN does not include the null terminator, so add 1.
    CHAR originalControlPlaneUrl[MAX_URI_CHAR_LEN + 1] =  {0};
    CHAR validatedControlPlaneUrl[MAX_URI_CHAR_LEN + 1] =  {0};

    CHAR expectedSdkGeneratedUri[MAX_CONTROL_PLANE_URI_CHAR_LEN] = {0};

    // createValidateChannelInfo will use DEFAULT_AWS_REGION to generate a URL if needed
    // since we are not setting a region on originalChannelInfo.
    SNPRINTF(expectedSdkGeneratedUri, sizeof(expectedSdkGeneratedUri), "%s%s.%s%s",
             CONTROL_PLANE_URI_PREFIX, KINESIS_VIDEO_SERVICE_NAME, DEFAULT_AWS_REGION, CONTROL_PLANE_URI_POSTFIX);
    
    // pChannelName must be set to make the createValidateChannelInfo call.
    originalChannelInfo.pChannelName = "TestChannelName";


    /* MAX URL ARRAY LENGTH (expect the custom URL be used) */

    // Fill array full of non-null values, accounting for null terminator.
    memset(originalControlPlaneUrl, 'X', sizeof(originalControlPlaneUrl));
    originalControlPlaneUrl[sizeof(originalControlPlaneUrl) - 1] = '\0';

    // Make a deep copy of originalControlPlaneUrl into validatedControlPlaneUrl.
    strncpy(validatedControlPlaneUrl, originalControlPlaneUrl, sizeof(validatedControlPlaneUrl));
    validatedControlPlaneUrl[sizeof(validatedControlPlaneUrl) - 1] = '\0';

    originalChannelInfo.pControlPlaneUrl = (PCHAR) validatedControlPlaneUrl;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&originalChannelInfo, &pValidatedChannelInfo));

    // Validate the ChanelInfo's Control Plane URL against originalControlPlaneUrl in case createValidateChannelInfo
    //      modified validatedControlPlaneUrl.
    EXPECT_STREQ(pValidatedChannelInfo->pControlPlaneUrl, (PCHAR) originalControlPlaneUrl);


    /* EMPTY URL ARRAY (expect SDK to construct a proper URL) */

    // Fill array with null values.
    memset(validatedControlPlaneUrl, 0, sizeof(validatedControlPlaneUrl));

    originalChannelInfo.pControlPlaneUrl = (PCHAR) validatedControlPlaneUrl;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&originalChannelInfo, &pValidatedChannelInfo));
    EXPECT_STREQ(pValidatedChannelInfo->pControlPlaneUrl, (PCHAR) expectedSdkGeneratedUri);


    /* NULL URL ARRAY (expect SDK to construct a proper URL) */

    originalChannelInfo.pControlPlaneUrl = (PCHAR) NULL;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&originalChannelInfo, &pValidatedChannelInfo));
    EXPECT_STREQ(pValidatedChannelInfo->pControlPlaneUrl, (PCHAR) expectedSdkGeneratedUri);
}


TEST_F(DualStackEndpointsTest, customControlPlaneMaxEndpointLengthFailingCase)
{
    STATUS retStatus;
    ChannelInfo originalChannelInfo;
    memset(&originalChannelInfo, 0, sizeof(originalChannelInfo));

    ChannelInfo validatedChannelInfo;
    memset(&validatedChannelInfo, 0, sizeof(validatedChannelInfo));
    PChannelInfo pValidatedChannelInfo = &validatedChannelInfo;

    /* Exceed MAX URL ARRAY LENGTH (expect SDK to error out) */

    // MAX_URI_CHAR_LEN does not include the null terminator, so add 2.
    CHAR originalControlPlaneUrl[MAX_URI_CHAR_LEN + 2] =  {0};

    // pChannelName must be set to make the createValidateChannelInfo call.
    originalChannelInfo.pChannelName = "TestChannelName";

    // Fill array full of non-null values, accounting for null terminator.
    memset(originalControlPlaneUrl, 'X', sizeof(originalControlPlaneUrl));
    originalControlPlaneUrl[sizeof(originalControlPlaneUrl) - 1] = '\0';

    originalChannelInfo.pControlPlaneUrl = (PCHAR) originalControlPlaneUrl;

    EXPECT_EQ(STATUS_SIGNALING_INVALID_CPL_LENGTH, createValidateChannelInfo(&originalChannelInfo, &pValidatedChannelInfo));
}


} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com