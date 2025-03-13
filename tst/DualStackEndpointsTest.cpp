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
    ChannelInfo channelInfo;
    memset(&channelInfo, 0, sizeof(channelInfo));
    PChannelInfo pChannelInfo;
    CHAR originalCustomControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN] = "https://kinesisvideo.us-west-2.api.aws";
    CHAR validatedCustomControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN]  =  {0};

    // Make a deep copy of originalCustomControlPlaneUrl into validatedCustomControlPlaneUrl to save the value for later comparison.
    strncpy(validatedCustomControlPlaneUrl, originalCustomControlPlaneUrl, MAX_CONTROL_PLANE_URI_CHAR_LEN);
    validatedCustomControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN - 1] = '\0';
    
    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    // pChannelName must be set to make the createValidateChannelInfo call.
    channelInfo.pChannelName = "TestChannelName";

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    // Validate the ChanelInfo's Control Plane URL against the originalCustomControlPlaneUrl.
    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) originalCustomControlPlaneUrl);

    freeChannelInfo(&pChannelInfo);
}

TEST_F(DualStackEndpointsTest, customControlPlaneEndpointEdgeCases)
{
    STATUS retStatus;
    ChannelInfo channelInfo;
    memset(&channelInfo, 0, sizeof(channelInfo));
    PChannelInfo pChannelInfo;
    
    // TODO: Verify why we use MAX_URI_CHAR_LEN instead of MAX_CONTROL_PLANE_URI_CHAR_LEN for the custom URL.
    // MAX_URI_CHAR_LEN does not include the null terminator, so add 1.
    CHAR originalCustomControlPlaneUrl[MAX_URI_CHAR_LEN + 1] =  {0};
    CHAR validatedCustomControlPlaneUrl[MAX_URI_CHAR_LEN + 1] =  {0};
    CHAR expectedSdkGeneratedUri[MAX_CONTROL_PLANE_URI_CHAR_LEN] = {0};

    // If createValidateChannelInfo needs to generate the URL, it will use DEFAULT_AWS_REGION since
    // we are not setting a region on ChannelInfo.
    SNPRINTF(expectedSdkGeneratedUri, sizeof(expectedSdkGeneratedUri), "%s%s.%s%s",
             CONTROL_PLANE_URI_PREFIX, KINESIS_VIDEO_SERVICE_NAME, DEFAULT_AWS_REGION, CONTROL_PLANE_URI_POSTFIX);
    
    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    // pChannelName must be set to make the createValidateChannelInfo call.
    channelInfo.pChannelName = "TestChannelName";


    /* MAX URL ARRAY LENGTH (expect the custom URL be used) */

    // Fill array full of non-null values, accounting for null terminator.
    memset(originalCustomControlPlaneUrl, 'X', sizeof(originalCustomControlPlaneUrl));
    originalCustomControlPlaneUrl[sizeof(originalCustomControlPlaneUrl) - 1] = '\0';

    // Make a deep copy of originalCustomControlPlaneUrl into validatedCustomControlPlaneUrl to save the value for
    //      later comparison in case createValidateChannelInfo modified validatedCustomControlPlaneUrl.
    strncpy(validatedCustomControlPlaneUrl, originalCustomControlPlaneUrl, sizeof(validatedCustomControlPlaneUrl));
    validatedCustomControlPlaneUrl[sizeof(validatedCustomControlPlaneUrl) - 1] = '\0';

    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    // Validate the ChanelInfo's Control Plane URL against originalCustomControlPlaneUrl in case createValidateChannelInfo
    //      modified validatedCustomControlPlaneUrl.
    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) originalCustomControlPlaneUrl);
    freeChannelInfo(&pChannelInfo);


    /* EMPTY URL ARRAY (expect SDK to construct a proper URL) */

    // Fill array with null values.
    memset(validatedCustomControlPlaneUrl, 0, sizeof(validatedCustomControlPlaneUrl));

    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    // Validate the ChanelInfo's Control Plane URL should be the default one the SDK generated.
    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) expectedSdkGeneratedUri);
    freeChannelInfo(&pChannelInfo);


    /* NULL URL ARRAY (expect SDK to construct a proper URL) */

    channelInfo.pControlPlaneUrl = (PCHAR) NULL;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));
    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) expectedSdkGeneratedUri);
    freeChannelInfo(&pChannelInfo);
}


TEST_F(DualStackEndpointsTest, customControlPlaneMaxEndpointLengthFailingCase)
{
    STATUS retStatus;
    ChannelInfo channelInfo;
    memset(&channelInfo, 0, sizeof(channelInfo));
    PChannelInfo pChannelInfo;

    
    /* Exceed MAX URL ARRAY LENGTH (expect SDK to error out) */

    // MAX_URI_CHAR_LEN does not include the null terminator, so add 2.
    CHAR customControlPlaneUrl[MAX_URI_CHAR_LEN + 2] =  {0};

    // pChannelName must be set to make the createValidateChannelInfo call.
    channelInfo.pChannelName = "TestChannelName";

    // Fill array full of non-null values, accounting for null terminator.
    memset(customControlPlaneUrl, 'X', sizeof(customControlPlaneUrl));
    customControlPlaneUrl[sizeof(customControlPlaneUrl) - 1] = '\0';

    channelInfo.pControlPlaneUrl = (PCHAR) customControlPlaneUrl;

    EXPECT_EQ(STATUS_SIGNALING_INVALID_CPL_LENGTH, createValidateChannelInfo(&channelInfo, &pChannelInfo));
    freeChannelInfo(&pChannelInfo);
}


} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com