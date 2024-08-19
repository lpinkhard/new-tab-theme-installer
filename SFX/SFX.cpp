#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <Msi.h>

// Function prototypes
bool ExtractZipWithShell(const std::wstring& zipPath, const std::wstring& outputPath);
bool ExecuteMSI(const std::wstring& msiPath);
void ShowError(LPCWSTR message);

// Helper function to find the ZIP signature manually
std::vector<char>::iterator FindZipSignature(std::vector<char>& data, const char* signature, size_t sigSize) {
    for (auto it = data.begin(); it != data.end() - sigSize + 1; ++it) {
        if (std::equal(signature, signature + sigSize, it)) {
            return it;
        }
    }
    return data.end();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);

    // Open the executable file
    std::ifstream exeFile(exePath, std::ios::binary | std::ios::ate);
    if (!exeFile) {
        ShowError(L"Failed to open the executable file!");
        return 1;
    }

    // Get the size of the executable
    std::streamsize exeSize = exeFile.tellg();
    exeFile.seekg(0, std::ios::beg);

    // Read the entire executable into memory
    std::vector<char> exeData(exeSize);
    if (!exeFile.read(exeData.data(), exeSize)) {
        ShowError(L"Failed to read the executable file!");
        return 1;
    }

    exeFile.close();

    // Find the ZIP signature (PK\x03\x04) manually
    const char zipSignature[] = { 'P', 'K', 0x03, 0x04 };
    auto zipStart = FindZipSignature(exeData, zipSignature, sizeof(zipSignature));

    if (zipStart == exeData.end()) {
        ShowError(L"No ZIP file found at the end of the executable!");
        return 1;
    }

    // Save the ZIP data to a temporary file
    wchar_t tempPath[MAX_PATH];
    GetTempPath(MAX_PATH, tempPath);
    std::wstring zipPath = std::wstring(tempPath) + L"extracted.zip";

    std::ofstream zipFile(zipPath, std::ios::binary);
    zipFile.write(&(*zipStart), std::distance(zipStart, exeData.end()));
    zipFile.close();

    // Define the extraction path (temporary directory)
    std::wstring extractionPath = std::wstring(tempPath);

    if (!ExtractZipWithShell(zipPath, extractionPath)) {
        ShowError(L"Failed to extract the ZIP file!");
        return 1;
    }

    // Execute the NewTabSetup.msi file
    std::wstring msiPath = extractionPath + L"NewTabSetup.msi";
    if (!ExecuteMSI(msiPath)) {
        ShowError(L"Failed to execute the MSI file!");
        return 1;
    }

    // Cleanup: Delete the temporary ZIP file
    DeleteFile(zipPath.c_str());

    return 0;
}

bool ExtractZipWithShell(const std::wstring& zipPath, const std::wstring& outputPath) {
    IShellDispatch* pShellDisp = NULL;
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        return false;
    }

    hr = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void**)&pShellDisp);
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    BSTR bstrZipPath = SysAllocString(zipPath.c_str());
    VARIANT vtZipFile;
    VariantInit(&vtZipFile);
    vtZipFile.vt = VT_BSTR;
    vtZipFile.bstrVal = bstrZipPath;

    VARIANT vtOutPath;
    VariantInit(&vtOutPath);
    vtOutPath.vt = VT_BSTR;
    BSTR bstrOutPath = SysAllocString(outputPath.c_str());
    vtOutPath.bstrVal = bstrOutPath;

    Folder* pZipFolder = NULL;
    hr = pShellDisp->NameSpace(vtZipFile, &pZipFolder);
    if (FAILED(hr) || pZipFolder == NULL) {
        pShellDisp->Release();
        CoUninitialize();
        return false;
    }

    Folder* pOutFolder = NULL;
    hr = pShellDisp->NameSpace(vtOutPath, &pOutFolder);
    if (FAILED(hr) || pOutFolder == NULL) {
        pZipFolder->Release();
        pShellDisp->Release();
        CoUninitialize();
        return false;
    }

    FolderItems* pItems = NULL;
    hr = pZipFolder->Items(&pItems);
    if (FAILED(hr) || pItems == NULL) {
        pOutFolder->Release();
        pZipFolder->Release();
        pShellDisp->Release();
        CoUninitialize();
        return false;
    }

    // Wrap the FolderItems in a VARIANT
    VARIANT vtItems;
    VariantInit(&vtItems);
    vtItems.vt = VT_DISPATCH;
    vtItems.pdispVal = pItems;

    VARIANT vtOptions;
    VariantInit(&vtOptions);
    vtOptions.vt = VT_I4;
    vtOptions.lVal = FOF_NO_UI; // Or use 0 if you don't want to suppress the UI

    hr = pOutFolder->CopyHere(vtItems, vtOptions);
    if (FAILED(hr)) {
        pItems->Release();
        pOutFolder->Release();
        pZipFolder->Release();
        pShellDisp->Release();
        CoUninitialize();
        return false;
    }

    // Cleanup
    VariantClear(&vtItems);
    pItems->Release();
    pOutFolder->Release();
    pZipFolder->Release();
    pShellDisp->Release();
    CoUninitialize();

    return true;
}

bool ExecuteMSI(const std::wstring& msiPath) {
    // The command line can be left as NULL if no special options are needed
    LPCWSTR commandLine = NULL;

    // Call the MsiInstallProduct function to install the MSI package
    UINT result = MsiInstallProduct(msiPath.c_str(), commandLine);

    // Check the result to see if the installation succeeded
    return (result == ERROR_SUCCESS);
}

void ShowError(LPCWSTR message) {
    MessageBox(NULL, message, L"Error", MB_ICONERROR);
}
