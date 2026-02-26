#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class CustomEndpointTest : public WebRtcClientTestBase {
};

TEST_F(CustomEndpointTest, customControlPlaneEndpointBasicCase)
{
    STATUS retStatus;
    ChannelInfo channelInfo;
    MEMSET(&channelInfo, 0, SIZEOF(channelInfo));
    PChannelInfo pChannelInfo = nullptr;
    CHAR originalCustomControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN] = "https://kinesisvideo.us-west-2.api.aws";
    CHAR validatedCustomControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN] = {0};

    // Make a deep copy of originalCustomControlPlaneUrl into validatedCustomControlPlaneUrl to save the value for later comparison.
    STRNCPY(validatedCustomControlPlaneUrl, originalCustomControlPlaneUrl, MAX_CONTROL_PLANE_URI_CHAR_LEN);
    validatedCustomControlPlaneUrl[MAX_CONTROL_PLANE_URI_CHAR_LEN - 1] = '\0';
    
    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    // pChannelName must be set to make the createValidateChannelInfo call.
    channelInfo.pChannelName = (PCHAR) "TestChannelName";

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));
    EXPECT_NE(nullptr, pChannelInfo);

    // Validate the ChannelInfo's Control Plane URL against the originalCustomControlPlaneUrl.
    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) originalCustomControlPlaneUrl);

    freeChannelInfo(&pChannelInfo);
}

TEST_F(CustomEndpointTest, customControlPlaneEndpointEdgeCases)
{
    STATUS retStatus;
    ChannelInfo channelInfo;
    MEMSET(&channelInfo, 0, SIZEOF(channelInfo));
    PChannelInfo pChannelInfo = nullptr;
    
    // TODO: Verify why we use MAX_URI_CHAR_LEN instead of MAX_CONTROL_PLANE_URI_CHAR_LEN for the custom URL.
    // MAX_URI_CHAR_LEN does not include the null terminator, so add 1.
    CHAR originalCustomControlPlaneUrl[MAX_URI_CHAR_LEN + 1] =  {0};
    CHAR validatedCustomControlPlaneUrl[MAX_URI_CHAR_LEN + 1] =  {0};
    CHAR expectedSdkGeneratedUri[MAX_CONTROL_PLANE_URI_CHAR_LEN] = {0};

    // If createValidateChannelInfo needs to generate the URL, it will use DEFAULT_AWS_REGION since
    // we are not setting a region on ChannelInfo.
    SNPRINTF(expectedSdkGeneratedUri, SIZEOF(expectedSdkGeneratedUri), "%s%s.%s%s",
             CONTROL_PLANE_URI_PREFIX, KINESIS_VIDEO_SERVICE_NAME, DEFAULT_AWS_REGION, CONTROL_PLANE_URI_POSTFIX);
    
    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    // pChannelName must be set to make the createValidateChannelInfo call.
    channelInfo.pChannelName = (PCHAR) "TestChannelName";


    /* MAX URL ARRAY LENGTH (expect the custom URL be used) */

    // Fill array full of non-null values, accounting for null terminator.
    MEMSET(originalCustomControlPlaneUrl, 'X', SIZEOF(originalCustomControlPlaneUrl));
    originalCustomControlPlaneUrl[SIZEOF(originalCustomControlPlaneUrl) - 1] = '\0';

    // Make a deep copy of originalCustomControlPlaneUrl into validatedCustomControlPlaneUrl to save the value for
    //      later comparison in case createValidateChannelInfo modified validatedCustomControlPlaneUrl.
    STRNCPY(validatedCustomControlPlaneUrl, originalCustomControlPlaneUrl, SIZEOF(validatedCustomControlPlaneUrl));
    validatedCustomControlPlaneUrl[SIZEOF(validatedCustomControlPlaneUrl) - 1] = '\0';

    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));
    EXPECT_NE(nullptr, pChannelInfo);

    // Validate the ChannelInfo's Control Plane URL against originalCustomControlPlaneUrl in case createValidateChannelInfo
    //      modified validatedCustomControlPlaneUrl.
    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) originalCustomControlPlaneUrl);
    freeChannelInfo(&pChannelInfo);


    /* EMPTY URL ARRAY (expect SDK to construct a proper URL) */

    // Fill array with null values.
    MEMSET(validatedCustomControlPlaneUrl, 0, SIZEOF(validatedCustomControlPlaneUrl));

    channelInfo.pControlPlaneUrl = (PCHAR) validatedCustomControlPlaneUrl;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));
    EXPECT_NE(nullptr, pChannelInfo);

    // Validate the ChannelInfo's Control Plane URL should be the default one the SDK generated.
    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) expectedSdkGeneratedUri);
    freeChannelInfo(&pChannelInfo);


    /* NULL URL ARRAY (expect SDK to construct a proper URL) */

    channelInfo.pControlPlaneUrl = nullptr;

    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&channelInfo, &pChannelInfo));
    EXPECT_NE(nullptr, pChannelInfo);

    EXPECT_STREQ(pChannelInfo->pControlPlaneUrl, (PCHAR) expectedSdkGeneratedUri);
    freeChannelInfo(&pChannelInfo);
}


TEST_F(CustomEndpointTest, customControlPlaneEndpointTooLong)
{
    STATUS retStatus;
    ChannelInfo channelInfo;
    MEMSET(&channelInfo, 0, SIZEOF(channelInfo));
    PChannelInfo pChannelInfo = nullptr;

    
    /* Exceed MAX URL ARRAY LENGTH (expect SDK to error out) */

    // MAX_URI_CHAR_LEN does not include the null terminator, so add 2.
    CHAR customControlPlaneUrl[MAX_URI_CHAR_LEN + 2] =  {0};

    // pChannelName must be set to make the createValidateChannelInfo call.
    channelInfo.pChannelName = (PCHAR) "TestChannelName";

    // Fill array full of non-null values, accounting for null terminator.
    MEMSET(customControlPlaneUrl, 'X', SIZEOF(customControlPlaneUrl));
    customControlPlaneUrl[SIZEOF(customControlPlaneUrl) - 1] = '\0';

    channelInfo.pControlPlaneUrl = (PCHAR) customControlPlaneUrl;

    EXPECT_EQ(STATUS_SIGNALING_INVALID_CPL_LENGTH, createValidateChannelInfo(&channelInfo, &pChannelInfo));
    freeChannelInfo(&pChannelInfo);
}


} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
