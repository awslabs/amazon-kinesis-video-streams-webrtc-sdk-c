#!/bin/bash
# You need to setup your aws cli first, because this script is based on aws cli.
# You can use this script to setup environment variables in the shell which samples run on.
# https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html
thingName="webrtc_iot_thing"
thingTypeName="webrtc_iot_thing_type"
iotPolicyName="webrtc_iot_policy"
kvsPolicyName="webrtc_policy"
iotRoleName="webrtc_iot_role"
iotRoleAlias="webrtc_iot_role_alias"
cacert="cacert.pem"
iotCert="webrtc_iot_certifcate.pem"
iotPublicKey="webrtc_iot_public.key"
iotPrivateKey="webrtc_iot_private.key"
tmpPath="iot-tmp"
certPath="iot-cert"

mkdir -p $tmpPath
mkdir -p $certPath

# Step 1: Create an IoT Thing Type and an IoT Thing
# The following example command creates a thing type $thingTypeName
aws --profile default  iot create-thing-type --thing-type-name $thingTypeName > $tmpPath/iot-thing-type.json
# And this example command creates the $thingName thing of the $thingTypeName thing type:
aws --profile default  iot create-thing --thing-name $thingName --thing-type-name $thingTypeName > $tmpPath/iot-thing.json

# Step 2: Create an IAM Role to be Assumed by IoT
# You can use the following trust policy JSON for the iam-policy-document.json:
echo '{
   "Version":"2012-10-17",
   "Statement":[
    {
     "Effect":"Allow",
     "Principal":{
        "Service":"credentials.iot.amazonaws.com"
     },
     "Action":"sts:AssumeRole"
    }
   ]
}' > $tmpPath/iam-policy-document.json
# Create an IAM role.
aws --profile default  iam create-role --role-name $iotRoleName --assume-role-policy-document file://$tmpPath/iam-policy-document.json > $tmpPath/iam-role.json

# You can use the following IAM policy JSON for the iam-permission-document.json:
echo '{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "kinesisvideo:DescribeSignalingChannel",
                "kinesisvideo:CreateSignalingChannel",
                "kinesisvideo:DeleteSignalingChannel",
                "kinesisvideo:GetSignalingChannelEndpoint",
                "kinesisvideo:GetIceServerConfig",
                "kinesisvideo:ConnectAsMaster",
                "kinesisvideo:ConnectAsViewer"
            ],
            "Resource": "arn:aws:kinesisvideo:*:*:channel/*/*"
        }
    ]
}' > $tmpPath/iam-permission-document.json
# Next, you must attach a permissions policy to the IAM role you created above. 
aws --profile default iam put-role-policy --role-name $iotRoleName --policy-name $kvsPolicyName --policy-document file://$tmpPath/iam-permission-document.json > $tmpPath/iot-policy.json
# Next, create a Role Alias for your IAM Role
aws --profile default  iot create-role-alias --role-alias $iotRoleAlias --role-arn $(jq --raw-output '.Role.Arn' $tmpPath/iam-role.json) --credential-duration-seconds 3600 > $tmpPath/iot-role-alias.json

# You can use the following command to create the iot-policy-document.json document JSON:
cat > $tmpPath/iot-policy-document.json <<EOF
{
   "Version":"2012-10-17",
   "Statement":[
      {
	 "Effect":"Allow",
	 "Action":[
	    "iot:Connect"
	 ],
	 "Resource":"$(jq --raw-output '.roleAliasArn' $tmpPath/iot-role-alias.json)"
 },
      {
	 "Effect":"Allow",
	 "Action":[
	    "iot:AssumeRoleWithCertificate"
	 ],
	 "Resource":"$(jq --raw-output '.roleAliasArn' $tmpPath/iot-role-alias.json)"
 }
   ]
}
EOF
# Now you can create the policy that will enable IoT to assume role with the certificate (once it is attached) using the role alias.
aws --profile default iot create-policy --policy-name $iotPolicyName --policy-document file://$tmpPath/iot-policy-document.json

# Step 3: Create and Configure the X.509 Certificate
# Create the certificate to which you must attach the policy for IoT that you created above.
aws --profile default  iot create-keys-and-certificate --set-as-active --certificate-pem-outfile $certPath/$iotCert --public-key-outfile $certPath/$iotPublicKey --private-key-outfile $certPath/$iotPrivateKey > $tmpPath/certificate
# Attach the policy for IoT (KvsCameraIoTPolicy created above) to this certificate.
aws --profile default  iot attach-policy --policy-name $iotPolicyName --target $(jq --raw-output '.certificateArn' $tmpPath/certificate)
# Attach your IoT thing (kvs_example_camera_stream) to the certificate you just created:
aws --profile default  iot attach-thing-principal --thing-name $thingName --principal $(jq --raw-output '.certificateArn' $tmpPath/certificate)
# In order to authorize requests through the IoT credentials provider, you need the IoT credentials endpoint which is unique to your AWS account ID. You can use the following command to get the IoT credentials endpoint.
aws --profile default  iot describe-endpoint --endpoint-type iot:CredentialProvider --output text > $tmpPath/iot-credential-provider.txt
# In addition to the X.509 cerficiate created above, you must also have a CA certificate to establish trust with the back-end service through TLS. You can get the CA certificate using the following command:
curl --silent 'https://www.amazontrust.com/repository/SFSRootCAG2.pem' --output $certPath/$cacert


export AWS_IOT_CORE_CREDENTIAL_ENDPOINT=$(cat $tmpPath/iot-credential-provider.txt)
export AWS_KVS_CACERT_PATH=$(pwd)"/"$certPath"/"$cacert
export AWS_IOT_CORE_CERT=$(pwd)"/"$certPath"/"$iotCert
export AWS_IOT_CORE_PRIVATE_KEY=$(pwd)"/"$certPath"/"$iotPrivateKey
export AWS_IOT_CORE_ROLE_ALIAS=$iotRoleAlias
export AWS_IOT_CORE_THING_NAME=$thingName
        


