// CopyBuildFolder.cpp : Defines the entry point for the custom action.
#include "pch.h"
#include <filesystem>
#include <sstream>
#include <windows.h>
#include <msi.h>
#include <msiquery.h>

// Copy directory recursively
void CopyDirectoryRecursively(const std::wstring& source, const std::wstring& destination) {
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
            const auto& path = entry.path();
            auto relativePathStr = std::filesystem::relative(path, source).wstring();
            std::wstring targetPath = destination + L"\\" + relativePathStr;

            if (std::filesystem::is_directory(path)) {
                std::filesystem::create_directories(targetPath);
                WcaLog(LOGMSG_STANDARD, "Created directory: %S", targetPath.c_str());
            }
            else if (std::filesystem::is_regular_file(path)) {
                std::filesystem::copy(path, targetPath, std::filesystem::copy_options::overwrite_existing);
                WcaLog(LOGMSG_STANDARD, "Copied file: %S to %S", path.c_str(), targetPath.c_str());
            }
            else {
                WcaLog(LOGMSG_STANDARD, "Skipping non-regular file: %S", path.c_str());
            }
        }
    }
    catch (const std::exception& e) {
        WcaLog(LOGMSG_STANDARD, "Exception in CopyDirectoryRecursively: %s", e.what());
    }
}

// Delete directory recursively
void DeleteDirectoryRecursively(const std::wstring& path) {
    try {
        std::filesystem::remove_all(path);
        WcaLog(LOGMSG_STANDARD, "Successfully deleted directory: %S", path.c_str());
    }
    catch (const std::exception& e) {
        WcaLog(LOGMSG_STANDARD, "Failed to delete directory: %S, Error: %s", path.c_str(), e.what());
    }
}

UINT __stdcall CopyBuildFolder(MSIHANDLE hInstall) {
    HRESULT hr = WcaInitialize(hInstall, "CopyBuildFolder");
    WCHAR szCustomActionData[2 * MAX_PATH];
    DWORD dwLen = sizeof(szCustomActionData) / sizeof(WCHAR);
    std::wistringstream dataStream;
    std::wstring sourcePath, targetPath, buildSourcePath;
    ExitOnFailure(hr, "Failed to initialize");

    hr = MsiGetProperty(hInstall, L"CustomActionData", szCustomActionData, &dwLen);
    ExitOnFailure(hr, "Failed to get CustomActionData");
    WcaLog(LOGMSG_STANDARD, "CustomActionData: %S", szCustomActionData);

    // Parse CustomActionData to get source and target paths
    dataStream = std::wistringstream(szCustomActionData);
    std::getline(dataStream, sourcePath, L';');
    std::getline(dataStream, targetPath, L';');

    WcaLog(LOGMSG_STANDARD, "Source path: %S", sourcePath.c_str());
    WcaLog(LOGMSG_STANDARD, "Target path: %S", targetPath.c_str());

    // Perform the copy operation
    try {
        CopyDirectoryRecursively(sourcePath, targetPath);
    }
    catch (const std::exception& e) {
        WcaLog(LOGMSG_STANDARD, "Exception occurred: %s", e.what());
        return ERROR_INSTALL_FAILURE;
    }

LExit:
    return WcaFinalize(hr);
}

UINT __stdcall DeleteBuildFolder(MSIHANDLE hInstall) {
    HRESULT hr = WcaInitialize(hInstall, "DeleteBuildFolder");
    WCHAR szInstallDir[MAX_PATH];
    DWORD dwLen = MAX_PATH;
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized DeleteBuildFolder.");

    // Get the installation directory from the MSI
    hr = MsiGetProperty(hInstall, L"CustomActionData", szInstallDir, &dwLen);
    ExitOnFailure(hr, "Failed to get installation directory");

    // Log the deletion
    WcaLog(LOGMSG_STANDARD, "Deleting build directory: %S", szInstallDir);

    // Delete the directory recursively
    DeleteDirectoryRecursively(szInstallDir);

LExit:
    hr = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    WcaLog(LOGMSG_STANDARD, "Finalizing DeleteBuildFolder with result: %d", hr);
    return WcaFinalize(hr);
}
