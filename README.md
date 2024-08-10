# New Tab Theme Installer

This project is a tool to install New Tab Themes using WiX (Windows Installer XML). It allows you to create and manage MSI packages, bundles, and other package types for your New Tab Themes.

## Getting Started

### Prerequisites

- .NET SDK version 6 or later
- WiX Toolset

### Installation

To install the WiX .NET tool, run the following command:

```bash
dotnet tool install --global wix
```

To verify the installation, run:

```bash
wix --version
```

### Updating WiX

To update your WiX .NET tool to the latest version, run:

```bash
dotnet tool update --global wix
```

To verify the update, run:

```bash
wix --version
```

## Building the Project

### Using the Command Line

You can build the project using the following command:

```bash
cd NewTabTheme
dotnet build
```

### Using Visual Studio

1. Open the existing Visual Studio solution (`NewTabTheme/NewTabTheme.sln`).
2. Restore the NuGet packages by right-clicking on the solution in the `Solution Explorer` and selecting `Restore NuGet Packages`.
3. Build the solution by selecting `Build` > `Build Solution` from the menu or pressing Ctrl+Shift+B.

### Generating the MSI File

The MSI file generated from the WiX build will be used as input for the AWS Lambda function. Place this MSI file into the designated S3 bucket as specified in the AWS Lambda configuration.

## AWS Lambda and S3 Configuration

This project uses AWS Lambda to handle the creation of a self-extracting executable (SFX) that installs New Tab Themes. The SFX is built from input files stored in S3, and the output is uploaded back to S3. This section provides instructions for setting up your AWS Lambda function and S3 buckets.

### Prerequisites

- AWS Account
- AWS CLI configured with your credentials
- Node.js and npm installed locally
- AWS Lambda Layer with 7-Zip and `osslsigncode` binaries

### S3 Configuration

1. **Create S3 Buckets:**

   You need to create three S3 buckets: 
   - **Input Bucket**: To store the input ZIP files.
   - **MSI Bucket**: To store the MSI files used in the installation.
   - **Output Bucket**: To store the signed SFX files after processing.

2. **Bucket Policy:**

   Ensure your S3 buckets have appropriate policies to allow access from your Lambda function. Below is an example policy that allows a specific Lambda function to read from the `input` and `msi` buckets and write to the `output` bucket:

   ```json
   {
       "Version": "2012-10-17",
       "Statement": [
           {
               "Effect": "Allow",
               "Principal": {
                   "Service": "lambda.amazonaws.com"
               },
               "Action": [
                   "s3:GetObject",
                   "s3:PutObject"
               ],
               "Resource": [
                   "arn:aws:s3:::your-input-bucket/*",
                   "arn:aws:s3:::your-msi-bucket/*",
                   "arn:aws:s3:::your-output-bucket/*"
               ]
           }
       ]
   }
   ```

### AWS Lambda Configuration

1. **Create a Lambda Function:**

   - Go to the AWS Lambda console and click on `Create function`.
   - Select `Author from scratch`.
   - Enter a function name, e.g., `NewTabThemeInstaller`.
   - Choose `Node.js 20` as the runtime.
   - Set up the execution role with permissions to access the S3 buckets.

2. **Set Up Environment Variables:**

   - **`OPENSSL_MODULES`**: Set this to `/opt/lib` to ensure `osslsigncode` can find the OpenSSL modules.
   - **Other variables**: You can define variables such as bucket names and keys if needed for easier configuration.

3. **Configure the Lambda Layer:**

   - A pre-packaged `layer.zip` is included in the repository under `LambdaLayer/layer.zip` for convenience. This layer includes the necessary binaries for `7z`, `osslsigncode`, and `7zSD.sfx`.
   - Ensure the layer is built for the `x86_64` architecture and Node.js 20 runtime.
   - Add this layer to your Lambda function.

4. **Include 7zSD.sfx in the Layer:**

   - Download the `7zSD.sfx` from the [LZMA SDK](https://www.7-zip.org/sdk.html).
   - Copy `7zSD.sfx` to the `bin` directory of your Lambda Layer.

5. **Deploy the Lambda Function:**

   Use the AWS CLI or AWS Console to deploy your Lambda function. Ensure that your deployment package includes all necessary code and dependencies.

6. **Set Up Triggers (Optional):**

   You can configure S3 triggers to automatically invoke your Lambda function when a new file is uploaded to the input bucket. This can automate the process of creating the SFX installer when new themes are uploaded.

### Changing the Icon

To change the icon of the SFX installer, you can use [Resource Hacker](http://www.angusj.com/resourcehacker/):

1. Open `7zSD.sfx` with Resource Hacker.
2. Replace the icon resource with your desired icon.
3. Save the modified `7zSD.sfx` and include it in your Lambda Layer.

### Running the Lambda Function

Once everything is set up, you can invoke the Lambda function manually via the AWS Console, AWS CLI, or by using an S3 trigger as mentioned above.

- **Example Invocation via AWS CLI:**

   ```bash
   aws lambda invoke --function-name NewTabThemeInstaller --payload '{"inputBucket":"your-input-bucket","inputKey":"path/to/input.zip","msiBucket":"your-msi-bucket","msiKey":"path/to/msi/NewTabSetup.msi","certBucket":"your-cert-bucket","certKey":"path/to/certificate.pfx","certPassword":"your-cert-password","outputBucket":"your-output-bucket","outputKey":"path/to/output/output.exe"}' output.json
   ```

- **Example Payload for Lambda Invocation:**

   ```json
   {
       "inputBucket": "your-input-bucket",
       "inputKey": "path/to/input.zip",
       "msiBucket": "your-msi-bucket",
       "msiKey": "path/to/msi/NewTabSetup.msi",
       "certBucket": "your-cert-bucket",
       "certKey": "path/to/certificate.pfx",
       "certPassword": "your-cert-password",
       "outputBucket": "your-output-bucket",
       "outputKey": "path/to/output/output.exe"
   }
   ```

### Lambda Execution Role

Ensure your Lambda execution role has the necessary permissions to access the S3 buckets and log the execution. Here’s an example IAM policy for the execution role:

```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "logs:CreateLogGroup",
                "logs:CreateLogStream",
                "logs:PutLogEvents"
            ],
            "Resource": "arn:aws:logs:*:*:*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "s3:GetObject",
                "s3:PutObject"
            ],
            "Resource": [
                "arn:aws:s3:::your-input-bucket/*",
                "arn:aws:s3:::your-msi-bucket/*",
                "arn:aws:s3:::your-cert-bucket/*",
                "arn:aws:s3:::your-output-bucket/*"
            ]
        }
    ]
}
```

### Installing 7-Zip for Lambda

To use `7z` in AWS Lambda, you need to install the `p7zip` package on your local machine or a compatible build environment. Once installed, copy the `7za` binary to your Lambda Layer:

1. **Install p7zip:**

   On a Linux environment, you can use the package manager to install `p7zip`:

   ```bash
   sudo apt-get install p7zip-full
   ```

2. **Copy the 7za Binary:**

   Locate the `7za` binary (often found in `/usr/libexec/7za`) and copy it to the `bin` directory in your Lambda Layer.

### Building and Installing osslsigncode

To use `osslsigncode` in AWS Lambda, it must be statically linked to OpenSSL. Here’s how to set it up:

1. **Modify CMakeLists.txt:**

   Adjust the `CMakeLists.txt` file for `osslsigncode` to ensure it links statically to OpenSSL. Set the OpenSSL library path to `/usr/local/openssl/lib64` and ensure static linking.

   ```cmake
   target_link_libraries(osslsigncode PRIVATE /usr/local/openssl/lib64/libssl.a /usr/local/openssl/lib64/libcrypto.a -ldl -lpthread)
   ```

2. **Build OpenSSL Statically:**

   Download and compile OpenSSL with static libraries:

   ```bash
   ./config no-shared --prefix=/usr

/local/openssl
   make
   make install
   ```

3. **Build osslsigncode with CMake:**

   Download `osslsigncode` source and build it using CMake:

   ```bash
   git clone https://github.com/mtrojnar/osslsigncode.git
   cd osslsigncode
   mkdir build
   cd build
   cmake .. -DOPENSSL_ROOT_DIR=/usr/local/openssl
   make
   ```

4. **Copy Binary and Dependencies:**

   - Copy the `osslsigncode` binary to the `bin` directory in your Lambda Layer.
   - Copy the `legacy.so` module (from OpenSSL) to the `lib` directory in your Lambda Layer.

This setup should enable you to efficiently manage and execute the creation of New Tab Theme installers using AWS services, integrating AWS Lambda and S3 for storage and processing. Ensure that the necessary IAM roles and policies are correctly configured to avoid permission issues during execution.
