#!/bin/bash

prefix=$1
thingName="${prefix}_thing"
thingTypeName="${prefix}_thing_type"
iotPolicyName="${prefix}_policy"
kvsPolicyName="${prefix}_policy"
iotRoleName="${prefix}_role"
iotRoleAlias="${prefix}_role_alias"
iotCert="${prefix}_certificate.pem"
iotPublicKey="${prefix}_public.key"
iotPrivateKey="${prefix}_private.key"

# Create the certificate to which you must attach the policy for IoT that you created above.
aws --profile default  iot create-keys-and-certificate --set-as-active --certificate-pem-outfile $iotCert --public-key-outfile $iotPublicKey --private-key-outfile $iotPrivateKey > certificate
# Attach the policy for IoT (KvsCameraIoTPolicy created above) to this certificate.
aws --profile default  iot attach-policy --policy-name $iotPolicyName --target $(jq --raw-output '.certificateArn' certificate)
# Attach your IoT thing (kvs_example_camera_stream) to the certificate you just created:
aws --profile default  iot attach-thing-principal --thing-name $thingName --principal $(jq --raw-output '.certificateArn' certificate)
# In order to authorize requests through the IoT credentials provider, you need the IoT credentials endpoint which is unique to your AWS account ID. You can use the following command to get the IoT credentials endpoint.
aws --profile default  iot describe-endpoint --endpoint-type iot:CredentialProvider --output text > iot-credential-provider.txt
# In addition to the X.509 cerficiate created above, you must also have a CA certificate to establish trust with the back-end service through TLS. You can get the CA certificate using the following command:
curl --silent 'https://www.amazontrust.com/repository/SFSRootCAG2.pem' --output cacert.pem