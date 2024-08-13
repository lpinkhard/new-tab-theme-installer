import { exec as execCallback } from 'child_process';
import fs from 'fs';
import os from 'os';
import path from 'path';
import { promisify } from 'util';
import yargs from 'yargs';

const execPromise = promisify(execCallback);

// Determine the correct executable for 7-Zip based on the platform
function get7zExecutable() {
    if (process.platform === 'win32') {
        const programFiles = process.env['ProgramFiles'] || 'C:\\Program Files';
        const sevenZipPath = path.join(programFiles, '7-Zip', '7z.exe');
        return fs.existsSync(sevenZipPath) ? `"${sevenZipPath}"` : '7z';
    }
    return '7za';
}

// Determine the correct path for signtool based on the platform
function getSignToolExecutable() {
    if (process.platform === 'win32') {
        const programFilesX86 = process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)';
        const signToolPath = path.join(programFilesX86, 'Microsoft SDKs', 'ClickOnce', 'SignTool', 'signtool.exe');
        return fs.existsSync(signToolPath) ? `"${signToolPath}"` : 'signtool';
    }
    throw new Error('signtool is only available on Windows');
}

// Helper function to clear a directory
function clearDirectory(directoryPath) {
    if (fs.existsSync(directoryPath)) {
        fs.readdirSync(directoryPath).forEach(file => {
            const currentPath = path.join(directoryPath, file);
            try {
                if (fs.lstatSync(currentPath).isDirectory()) {
                    clearDirectory(currentPath);
                    fs.rmdirSync(currentPath);
                } else {
                    fs.unlinkSync(currentPath);
                }
            } catch (err) {
                console.error(`Failed to delete file: ${currentPath}, error: ${err.message}`);
            }
        });
    }
}

// Concatenate files to create an SFX executable
function concatenateFiles(sfxModulePath, configFilePath, archivePath, outputSfxPath) {
    try {
        const sfxModule = fs.readFileSync(sfxModulePath);
        const configFile = fs.readFileSync(configFilePath);
        const archive = fs.readFileSync(archivePath);

        const outputStream = fs.createWriteStream(outputSfxPath);
        outputStream.write(sfxModule);
        outputStream.write(configFile);
        outputStream.write(archive);
        outputStream.end();
    } catch (err) {
        console.error(`Error during file concatenation: ${err.message}`);
    }
}

// Main function to process files
async function processFiles(options) {
    const uniqueTempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'newtabtheme-'));
    const buildDir = path.join(uniqueTempDir, 'build');
    const inputZipPath = options.inputFile;
    const msiPath = options.msiFile;
    const archivePath = path.join(uniqueTempDir, 'archive.7z');
    const configFilePath = path.join(uniqueTempDir, 'config.txt');
    const sfxModulePath = options.sfxModulePath;
    const outputSfxPath = options.outputFile;
    const certificatePath = options.certFile;
    const signedOutputPath = path.join(uniqueTempDir, 'signed-output.exe');
    const sevenZip = get7zExecutable();
    const signTool = getSignToolExecutable();

    try {
        // Create build directory
        if (!fs.existsSync(buildDir)) {
            fs.mkdirSync(buildDir);
        }

        // Extract zip to build directory, preserving directory structure
        await execPromise(`${sevenZip} x "${inputZipPath}" -o"${buildDir}" -y`);

        // Create a 7z archive with the correct structure
        await createArchiveWithStructure(buildDir, msiPath, archivePath, sevenZip);

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
        concatenateFiles(sfxModulePath, configFilePath, archivePath, outputSfxPath);

        // Sign the SFX executable
        if (process.platform === 'win32') {
            await signExecutableWithSignTool(outputSfxPath, certificatePath, options.certPassword, signTool);
        } else {
            await signExecutableWithOsslSigncode(outputSfxPath, signedOutputPath, certificatePath, options.certPassword);
        }

        console.log('SFX archive created and signed successfully!');
    } catch (error) {
        console.error('Error processing the request:', error);
    } finally {
        // Clean up temporary directory
        clearDirectory(uniqueTempDir);
    }
}

// Create a 7z archive with the specified structure
async function createArchiveWithStructure(buildDir, msiPath, archivePath, sevenZip) {
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

    // Move all files and directories to the 'build' folder
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
    await execPromise(`${sevenZip} a "${archivePath}" "${tempDir}\\*" -m0=lzma2 -mx=0 -r`);
}

// Sign an executable using osslsigncode (non-Windows)
async function signExecutableWithOsslSigncode(inputFile, outputFile, certFile, certPassword) {
    const command = `osslsigncode sign -pkcs12 "${certFile}" -pass "${certPassword}" -n "New Tab Setup" -i https://newtabthemebuilder.com/ -in "${inputFile}" -out "${outputFile}"`;
    await execPromise(command);
}

// Sign an executable using signtool (Windows)
async function signExecutableWithSignTool(inputFile, certFile, certPassword, signTool) {
    const command = `${signTool} sign /f "${certFile}" /p "${certPassword}" /tr http://timestamp.digicert.com /td sha256 /fd sha256 "${inputFile}"`;
    await execPromise(command, { shell: 'cmd.exe' });
}

// Command-line arguments handling
const argv = yargs(process.argv.slice(2))
    .option('inputFile', {
        alias: 'i',
        description: 'Path to the input ZIP file',
        type: 'string',
        default: 'input.zip'
    })
    .option('msiFile', {
        alias: 'm',
        description: 'Path to the MSI file',
        type: 'string',
        default: 'NewTabSetup.msi'
    })
    .option('certFile', {
        alias: 'c',
        description: 'Path to the certificate PFX file',
        type: 'string',
        default: 'Certificate.pfx'
    })
    .option('certPassword', {
        alias: 'p',
        description: 'Password for the certificate PFX file',
        type: 'string',
        default: 'password'
    })
    .option('outputFile', {
        alias: 'o',
        description: 'Path to the output executable file',
        type: 'string',
        default: 'NewTabSetup.exe'
    })
    .option('sfxModulePath', {
        alias: 's',
        description: 'Path to the 7zSD.sfx file',
        type: 'string',
        default: '7zSD.sfx'
    })
    .help()
    .alias('help', 'h')
    .argv;

// Run the process
processFiles(argv);
