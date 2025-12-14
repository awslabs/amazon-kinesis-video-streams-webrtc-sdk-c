#!/bin/bash
# You need to setup your aws cli first, because this script is based on aws cli.
# You can use this script to setup environment variables in the shell which samples run on.
# https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html

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

# Step 1: Create an IoT Thing Type and an IoT Thing
# The following example command creates a thing type $thingTypeName
aws --profile default  iot create-thing-type --thing-type-name $thingTypeName > iot-thing-type.json
# And this example command creates the $thingName thing of the $thingTypeName thing type:
aws --profile default  iot create-thing --thing-name $thingName --thing-type-name $thingTypeName > iot-thing.json

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
}' > iam-policy-document.json
# Create an IAM role.
aws --profile default  iam create-role --role-name $iotRoleName --assume-role-policy-document 'file://iam-policy-document.json' > iam-role.json

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
            "Resource": "arn:aws:kinesisvideo:*:*:channel/${credentials-iot:ThingName}/*"
        }
    ]
}' > iam-permission-document.json
# Next, you must attach a permissions policy to the IAM role you created above. 
aws --profile default iam put-role-policy --role-name $iotRoleName --policy-name $kvsPolicyName --policy-document 'file://iam-permission-document.json' 
# Next, create a Role Alias for your IAM Role
aws --profile default  iot create-role-alias --role-alias $iotRoleAlias --role-arn $(jq --raw-output '.Role.Arn' iam-role.json) --credential-duration-seconds 3600 > iot-role-alias.json

# You can use the following command to create the iot-policy-document.json document JSON:
cat > iot-policy-document.json <<EOF
{
   "Version":"2012-10-17",
   "Statement":[
      {
     "Effect":"Allow",
     "Action":[
        "iot:Connect"
     ],
     "Resource":"$(jq --raw-output '.roleAliasArn' iot-role-alias.json)"
 },
      {
     "Effect":"Allow",
     "Action":[
        "iot:AssumeRoleWithCertificate"
     ],
     "Resource":"$(jq --raw-output '.roleAliasArn' iot-role-alias.json)"
 }
   ]
}
EOF
# Now you can create the policy that will enable IoT to assume role with the certificate (once it is attached) using the role alias.
aws --profile default iot create-policy --policy-name $iotPolicyName --policy-document 'file://iot-policy-document.json'


