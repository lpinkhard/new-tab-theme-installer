// CustomAction.cpp : Defines the entry point for the custom action.
#include "pch.h"
#include <iostream>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#include <filesystem>
#include <vector>

#pragma comment(lib, "shell32.lib")

// Helper function to set registry key values
bool setRegistryKeyValue(HKEY root, const std::wstring& subKey, const std::wstring& extensionPath) {
    HKEY hKey;
    if (RegOpenKeyEx(root, subKey.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        WcaLog(LOGMSG_STANDARD, "Failed to open registry key: %S", subKey.c_str());
        return false;
    }

    std::wstring currentValue(1024, L'\0');
    DWORD size = static_cast<DWORD>(currentValue.size() * sizeof(wchar_t));
    RegQueryValueEx(hKey, nullptr, nullptr, nullptr, reinterpret_cast<LPBYTE>(&currentValue[0]), &size);
    currentValue.resize(size / sizeof(wchar_t) - 1);

    // Add new argument if not already present
    if (currentValue.find(L"--load-extension") == std::wstring::npos) {
        std::wstring newValue = currentValue;
        size_t pos = newValue.find(L" %1");
        if (pos == std::wstring::npos) {
            pos = newValue.find(L" \"%1\"");
        }
        if (pos != std::wstring::npos) {
            newValue.insert(pos, L" --load-extension=\"" + extensionPath + L"\"");
            if (RegSetValueEx(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(newValue.c_str()), static_cast<DWORD>((newValue.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
                RegCloseKey(hKey);
                WcaLog(LOGMSG_STANDARD, "Failed to set registry key value: %S", subKey.c_str());
                return false;
            }
        }
    }

    RegCloseKey(hKey);
    return true;
}

// Helper function to restore registry key values
bool restoreRegistryKeyValue(HKEY root, const std::wstring& subKey, const std::wstring& extensionPath) {
    HKEY hKey;
    if (RegOpenKeyEx(root, subKey.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        WcaLog(LOGMSG_STANDARD, "Failed to open registry key: %S", subKey.c_str());
        return false;
    }

    std::wstring currentValue(1024, L'\0');
    DWORD size = static_cast<DWORD>(currentValue.size() * sizeof(wchar_t));
    RegQueryValueEx(hKey, nullptr, nullptr, nullptr, reinterpret_cast<LPBYTE>(&currentValue[0]), &size);
    currentValue.resize(size / sizeof(wchar_t) - 1);

    // Remove the argument if present
    std::wstring extensionArgument = L" --load-extension=\"" + extensionPath + L"\"";
    size_t pos = currentValue.find(extensionArgument);
    if (pos != std::wstring::npos) {
        currentValue.erase(pos, extensionArgument.length());
        if (RegSetValueEx(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(currentValue.c_str()), static_cast<DWORD>((currentValue.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            WcaLog(LOGMSG_STANDARD, "Failed to restore registry key value: %S", subKey.c_str());
            return false;
        }
    }

    RegCloseKey(hKey);
    return true;
}

// Function to find the Chrome or Edge shortcut path
std::wstring findShortcut(const std::wstring& browserName) {
    wchar_t* programsPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &programsPath);
    std::wstring shortcutPath = std::wstring(programsPath) + L"\\" + browserName + L".lnk";
    CoTaskMemFree(programsPath);

    // Check if the file exists
    if (std::filesystem::exists(shortcutPath)) {
        return shortcutPath;
    }
    else {
        WcaLog(LOGMSG_STANDARD, "%S shortcut not found at default location.", browserName.c_str());
        return L"";
    }
}

// Function to find the Chrome or Edge taskbar pin
std::wstring findPin(const std::wstring& browserName) {
    wchar_t* appDataPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath);
    std::wstring shortcutPath = std::wstring(appDataPath) + L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar\\" + browserName + L".lnk";
    CoTaskMemFree(appDataPath);

    // Check if the file exists
    if (std::filesystem::exists(shortcutPath)) {
        return shortcutPath;
    }
    else {
        WcaLog(LOGMSG_STANDARD, "%S pin not found at default location.", browserName.c_str());
        return L"";
    }
}

// Function to update the browser shortcut with a new argument
bool updateShortcut(const std::wstring& shortcutPath, const std::wstring& extensionPath) {
    CoInitialize(NULL);

    IShellLinkW* pShellLink = nullptr;
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&pShellLink);

    if (SUCCEEDED(hres)) {
        IPersistFile* pPersistFile = nullptr;

        // Load the shortcut
        hres = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
        if (SUCCEEDED(hres)) {
            hres = pPersistFile->Load(shortcutPath.c_str(), STGM_READWRITE);

            if (SUCCEEDED(hres)) {
                // Get the current target
                wchar_t targetPath[MAX_PATH];
                pShellLink->GetPath(targetPath, MAX_PATH, NULL, SLGP_UNCPRIORITY);

                // Update the arguments
                std::wstring fullCommand = L"--load-extension=\"" + extensionPath + L"\"";
                pShellLink->SetArguments(fullCommand.c_str());

                // Save the updated shortcut
                hres = pPersistFile->Save(shortcutPath.c_str(), TRUE);
                WcaLog(LOGMSG_STANDARD, "Shortcut updated: %S", shortcutPath.c_str());
            }
            pPersistFile->Release();
        }
        pShellLink->Release();
    }

    CoUninitialize();

    return SUCCEEDED(hres);
}

// Function to restore the browser shortcut to its original state
bool restoreShortcut(const std::wstring& shortcutPath) {
    CoInitialize(NULL);

    IShellLinkW* pShellLink = nullptr;
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&pShellLink);

    if (SUCCEEDED(hres)) {
        IPersistFile* pPersistFile = nullptr;

        // Load the shortcut
        hres = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
        if (SUCCEEDED(hres)) {
            hres = pPersistFile->Load(shortcutPath.c_str(), STGM_READWRITE);

            if (SUCCEEDED(hres)) {
                // Reset the arguments to an empty string
                pShellLink->SetArguments(L"");

                // Save the updated shortcut
                hres = pPersistFile->Save(shortcutPath.c_str(), TRUE);
                WcaLog(LOGMSG_STANDARD, "Shortcut restored: %S", shortcutPath.c_str());
            }
            pPersistFile->Release();
        }
        pShellLink->Release();
    }

    CoUninitialize();

    return SUCCEEDED(hres);
}

// Function to apply all changes
void applyAllChanges(const std::wstring& extensionPath) {
    std::wstring chromeShortcutPath = findShortcut(L"Google Chrome");
    std::wstring edgeShortcutPath = findShortcut(L"Microsoft Edge");
    std::wstring chromePinPath = findPin(L"Google Chrome");
    std::wstring edgePinPath = findPin(L"Microsoft Edge");

    if (!chromeShortcutPath.empty()) {
        updateShortcut(chromeShortcutPath, extensionPath);
    }

    if (!edgeShortcutPath.empty()) {
        updateShortcut(edgeShortcutPath, extensionPath);
    }

    if (!chromePinPath.empty()) {
        updateShortcut(chromePinPath, extensionPath);
    }

    if (!edgePinPath.empty()) {
        updateShortcut(edgePinPath, extensionPath);
    }

    // Chrome registry keys
    std::vector<std::wstring> chromeRegistryKeys = {
        L"ChromeHTML\\shell\\open\\command"
    };

    for (const auto& key : chromeRegistryKeys) {
        setRegistryKeyValue(HKEY_CLASSES_ROOT, key, extensionPath);
        WcaLog(LOGMSG_STANDARD, "Chrome registry key updated: %S", key.c_str());
    }

    // Edge registry keys
    std::vector<std::wstring> edgeRegistryKeys = {
        L"MSEdgeHTM\\shell\\open\\command",
        L"MSEdgeHTM\\shell\\runas\\command",
        L"MSEdgeMHT\\shell\\open\\command",
        L"MSEdgeMHT\\shell\\runas\\command",
        L"MSEdgePDF\\shell\\open\\command",
        L"MSEdgePDF\\shell\\runas\\command",
        L"microsoft-edge\\shell\\open\\command",
        L"VisioViewer.Viewer\\shell\\open\\command",
        L"http\\shell\\open\\command",
        L"https\\shell\\open\\command"
    };

    for (const auto& key : edgeRegistryKeys) {
        setRegistryKeyValue(HKEY_CLASSES_ROOT, key, extensionPath);
        WcaLog(LOGMSG_STANDARD, "Edge registry key updated: %S", key.c_str());
    }
}

// Function to restore all changes
void restoreAllChanges(const std::wstring& extensionPath) {
    std::wstring chromeShortcutPath = findShortcut(L"Google Chrome");
    std::wstring edgeShortcutPath = findShortcut(L"Microsoft Edge");
    std::wstring chromePinPath = findPin(L"Google Chrome");
    std::wstring edgePinPath = findPin(L"Microsoft Edge");

    if (!chromeShortcutPath.empty()) {
        restoreShortcut(chromeShortcutPath);
    }

    if (!edgeShortcutPath.empty()) {
        restoreShortcut(edgeShortcutPath);
    }

    if (!chromePinPath.empty()) {
        restoreShortcut(chromePinPath);
    }

    if (!edgePinPath.empty()) {
        restoreShortcut(edgePinPath);
    }

    // Chrome registry keys
    std::vector<std::wstring> chromeRegistryKeys = {
        L"ChromeHTML\\shell\\open\\command"
    };

    for (const auto& key : chromeRegistryKeys) {
        restoreRegistryKeyValue(HKEY_CLASSES_ROOT, key, extensionPath);
        WcaLog(LOGMSG_STANDARD, "Chrome registry key restored: %S", key.c_str());
    }

    // Edge registry keys
    std::vector<std::wstring> edgeRegistryKeys = {
        L"MSEdgeHTM\\shell\\open\\command",
        L"MSEdgeHTM\\shell\\runas\\command",
        L"MSEdgeMHT\\shell\\open\\command",
        L"MSEdgeMHT\\shell\\runas\\command",
        L"MSEdgePDF\\shell\\open\\command",
        L"MSEdgePDF\\shell\\runas\\command",
        L"microsoft-edge\\shell\\open\\command",
        L"VisioViewer.Viewer\\shell\\open\\command",
        L"http\\shell\\open\\command",
        L"https\\shell\\open\\command"
    };

    for (const auto& key : edgeRegistryKeys) {
        restoreRegistryKeyValue(HKEY_CLASSES_ROOT, key, extensionPath);
        WcaLog(LOGMSG_STANDARD, "Edge registry key restored: %S", key.c_str());
    }
}

UINT __stdcall InstallExtension(
    __in MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;
    std::wstring extensionPath;
    WCHAR szInstallDir[MAX_PATH];
    DWORD dwLen = MAX_PATH;

    hr = WcaInitialize(hInstall, "InstallExtension");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized InstallExtension.");

    // Get the installation directory from the MSI
    hr = MsiGetProperty(hInstall, L"CustomActionData", szInstallDir, &dwLen);
    ExitOnFailure(hr, "Failed to get installation directory");

    // Construct the path to the 'build' folder inside the installation directory
    extensionPath = std::wstring(szInstallDir) + L"\\build";

    WcaLog(LOGMSG_STANDARD, "Applying changes for extension path: %S", extensionPath.c_str());
    applyAllChanges(extensionPath);

LExit:
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    WcaLog(LOGMSG_STANDARD, "Finalizing InstallExtension with result: %d", er);
    return WcaFinalize(er);
}

UINT __stdcall UninstallExtension(
    __in MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;
    std::wstring extensionPath;
    WCHAR szInstallDir[MAX_PATH];
    DWORD dwLen = MAX_PATH;

    hr = WcaInitialize(hInstall, "UninstallExtension");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized UninstallExtension.");

    // Get the installation directory from the MSI
    hr = MsiGetProperty(hInstall, L"CustomActionData", szInstallDir, &dwLen);
    ExitOnFailure(hr, "Failed to get installation directory");

    // Construct the path to the 'build' folder inside the installation directory
    extensionPath = std::wstring(szInstallDir) + L"\\build";

    WcaLog(LOGMSG_STANDARD, "Restoring changes for extension path: %S", extensionPath.c_str());
    restoreAllChanges(extensionPath);

LExit:
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    WcaLog(LOGMSG_STANDARD, "Finalizing UninstallExtension with result: %d", er);
    return WcaFinalize(er);
}
