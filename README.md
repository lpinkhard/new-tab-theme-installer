# New Tab Theme Installer

This project is a tool to install new tab themes using WiX (Windows Installer XML). It allows you to create and manage MSI packages, bundles, and other package types for your new tab themes.

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
dotnet build
```

### Using Visual Studio

1. Open the existing Visual Studio solution (`NewTabTheme.sln`).
2. Restore the NuGet packages by right-clicking on the solution in the `Solution Explorer` and selecting `Restore NuGet Packages`.
3. Build the solution by selecting `Build` > `Build Solution` from the menu or pressing Ctrl+Shift+B.
