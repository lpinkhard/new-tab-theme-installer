import AWS from 'aws-sdk';
import { exec as execCallback } from 'child_process';
import fs from 'fs';
import os from 'os';
import path from 'path';
import { promisify } from 'util';

const s3 = new AWS.S3();
const execPromise = promisify(execCallback);

// Main handler function
export const handler = async (event) => {
    const tempDir = os.tmpdir();
    const buildDir = path.join(tempDir, 'build');
    const inputZipPath = path.join(tempDir, 'input.zip');
    const msiPath = path.join(tempDir, 'NewTabSetup.msi');
    const archivePath = path.join(tempDir, 'archive.7z');
    const configFilePath = path.join(tempDir, 'config.txt');
    const sfxModulePath = '/opt/bin/7zSD.sfx'; // Ensure this is the correct path to 7zSD.sfx
    const outputSfxPath = path.join(tempDir, 'output.exe');
    const certificatePath = path.join(tempDir, 'certificate.pfx');
    const signedOutputPath = path.join(tempDir, 'signed-output.exe');

    // Retrieve input details from the event
    const inputBucket = event.inputBucket;
    const inputKey = event.inputKey;
    const msiBucket = event.msiBucket;
    const msiKey = event.msiKey;
    const certBucket = event.certBucket;
    const certKey = event.certKey;
    const certPassword = event.certPassword;
    const outputBucket = event.outputBucket;
    const outputKey = event.outputKey;

    try {
        // Create build directory
        if (!fs.existsSync(buildDir)) {
            fs.mkdirSync(buildDir);
        }

        // Download the input zip file from S3
        await downloadFileFromS3(inputBucket, inputKey, inputZipPath);

        // Extract zip to build directory, preserving directory structure
        await execPromise(`/opt/bin/7za x ${inputZipPath} -o${buildDir} -y`);

        // Download the MSI file from S3
        await downloadFileFromS3(msiBucket, msiKey, msiPath);

        // Download the certificate file from S3
        await downloadFileFromS3(certBucket, certKey, certificatePath);

        // Create a 7z archive with the correct structure
        await createArchiveWithStructure(buildDir, msiPath, archivePath);

        // Create config file for the SFX archive
        const configContent = `
;!@Install@!UTF-8!
Title="New Tab Setup"
ExecuteFile="msiexec.exe"
ExecuteParameters="/i NewTabSetup.msi"
GUIMode="2"
;!@InstallEnd@!
`;
        fs.writeFileSync(configFilePath, configContent);

        // Concatenate the SFX module, config file, and 7z archive into an executable
        await execPromise(`cat ${sfxModulePath} ${configFilePath} ${archivePath} > ${outputSfxPath}`);

        // Sign the SFX executable using osslsigncode
        await signExecutable(outputSfxPath, signedOutputPath, certificatePath, certPassword);

        // Upload the signed SFX archive to S3
        await uploadFileToS3(outputBucket, outputKey, signedOutputPath);

        return {
            statusCode: 200,
            body: JSON.stringify({ message: 'SFX archive created, signed, and uploaded successfully!' })
        };
    } catch (error) {
        console.error('Error processing the request:', error);
        return {
            statusCode: 500,
            body: JSON.stringify({ error: error.message })
        };
    }
};

// Download a file from S3
async function downloadFileFromS3(bucket, key, destination) {
    const params = {
        Bucket: bucket,
        Key: key
    };

    const s3Object = await s3.getObject(params).promise();
    fs.writeFileSync(destination, s3Object.Body);
}

// Upload a file to S3
async function uploadFileToS3(bucket, key, filePath) {
    const fileStream = fs.createReadStream(filePath);
    const params = {
        Bucket: bucket,
        Key: key,
        Body: fileStream
    };
    await s3.upload(params).promise();
}

// Create a 7z archive with the specified structure
async function createArchiveWithStructure(buildDir, msiPath, archivePath) {
    // Create a temporary directory to hold the archive contents
    const tempDir = path.join(buildDir, 'temp');
    if (!fs.existsSync(tempDir)) {
        fs.mkdirSync(tempDir);
    }

    // Create a 'build' folder inside the temp directory
    const buildFolder = path.join(tempDir, 'build');
    if (!fs.existsSync(buildFolder)) {
        fs.mkdirSync(buildFolder);
    }

    // Move all files and directories to the 'build' folder, avoid moving 'temp' into itself
    const files = fs.readdirSync(buildDir);
    files.forEach(file => {
        const filePath = path.join(buildDir, file);
        const destinationPath = path.join(buildFolder, file);

        // Avoid moving the 'temp' directory into itself
        if (filePath !== tempDir) {
            fs.renameSync(filePath, destinationPath);
        }
    });

    // Move the MSI file to the root of the temp directory
    fs.copyFileSync(msiPath, path.join(tempDir, 'NewTabSetup.msi'));

    // Create the 7z archive from the temp directory, including subdirectories
    await execPromise(`/opt/bin/7za a ${archivePath} ${tempDir}/* -m0=lzma2 -mx=0 -r`);
}

// Sign an executable using osslsigncode
async function signExecutable(inputFile, outputFile, certFile, certPassword) {
    // Set the environment variable in the Lambda script
    process.env.OPENSSL_MODULES = '/opt/lib';

    const command = `/opt/bin/osslsigncode sign -pkcs12 ${certFile} -pass ${certPassword} -n "New Tab Setup" -i https://newtabthemebuilder.com/ -in ${inputFile} -out ${outputFile}`;
    await execPromise(command);
}
