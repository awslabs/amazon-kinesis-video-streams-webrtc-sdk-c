# IoT Device Certificates

This directory must contain the following certificates for your AWS IoT Thing:

- `certificate.pem` - The IoT device certificate in PEM format (`certificate.pem.crt`)
- `private.key` - The IoT device private key in PEM format (`private.pem.key`)
- `cacert.pem` - The AWS Root CA certificate (`AmazonRootCA1.pem`)

These certificates are required for the KVS WebRTC application to authenticate with AWS IoT Core and obtain credentials for KVS Signaling.

You can obtain these certificates by:

1. Creating an IoT Thing in AWS IoT Core
2. Creating and downloading the certificates during Thing creation
3. Copying the certificates to this directory with the exact filenames listed above

Make sure to keep your private key secure and never commit it to source control.

Please refer [this](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/blob/main/scripts/generate-iot-credential.sh) script for the certificate generation.
