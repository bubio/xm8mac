#
# Update version number across all build systems.
#
# Usage: .\scripts\update_version.ps1 <major> <minor> <patch>
# Example: .\scripts\update_version.ps1 1 7 8
#

param(
    [Parameter(Mandatory)][int]$Major,
    [Parameter(Mandatory)][int]$Minor,
    [Parameter(Mandatory)][int]$Patch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($Major -lt 0 -or $Major -gt 9 -or $Minor -lt 0 -or $Minor -gt 9 -or $Patch -lt 0 -or $Patch -gt 9) {
    Write-Error "major, minor, patch must each be a single digit (0-9)"
    exit 1
}

$RootDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

$VerDot = "$Major.$Minor.$Patch"
$VerBCD = '0x{0:X2}{1:X}{2:X}' -f $Major, $Minor, $Patch
$VerAndroid = "$Major.$Minor$Patch"

Write-Host "Updating version to $VerDot (BCD: $VerBCD, Android: $VerAndroid)"

# Helper: read file, replace, write back (preserving encoding)
function Replace-InFile {
    param([string]$Path, [string]$Pattern, [string]$Replacement)
    $content = [System.IO.File]::ReadAllText($Path)
    $newContent = [regex]::Replace($content, $Pattern, $Replacement)
    if ($content -eq $newContent) {
        Write-Warning "No match for pattern in $Path"
    }
    [System.IO.File]::WriteAllText($Path, $newContent)
}

# 1. CMakeLists.txt
Replace-InFile "$RootDir\CMakeLists.txt" `
    '(?m)^(set\(PROJECT_VERSION )\d+\.\d+\.\d+\)' `
    "`${1}$VerDot)"

# 2. Source/UI/app.cpp
Replace-InFile "$RootDir\Source\UI\app.cpp" `
    '(?m)^(#define APP_VER\s+)0x[0-9A-Fa-f]+' `
    "`${1}$VerBCD"

# 3. Builder/Windows/XM8.rc — numeric versions
Replace-InFile "$RootDir\Builder\Windows\XM8.rc" `
    '(?m)^(FILEVERSION\s+)\d+,\d+,\d+,\d+' `
    "`${1}$Major,$Minor,$Patch,0"
Replace-InFile "$RootDir\Builder\Windows\XM8.rc" `
    '(?m)^(PRODUCTVERSION\s+)\d+,\d+,\d+,\d+' `
    "`${1}$Major,$Minor,$Patch,0"

# 3b. XM8.rc — string versions
Replace-InFile "$RootDir\Builder\Windows\XM8.rc" `
    '(VALUE "FileVersion",\s+")\d+\.\d+\.\d+\.\d+' `
    "`${1}$VerDot.0"
Replace-InFile "$RootDir\Builder\Windows\XM8.rc" `
    '(VALUE "ProductVersion",\s+")\d+\.\d+\.\d+\.\d+' `
    "`${1}$VerDot.0"

# 4. build.gradle
Replace-InFile "$RootDir\Builder\Android\app\build.gradle" `
    '(versionName ")\d+\.\d+"' `
    "`${1}$VerAndroid`""

# 5. AndroidManifest.xml
Replace-InFile "$RootDir\Builder\Android\app\src\main\AndroidManifest.xml" `
    '(android:versionName=")\d+\.\d+"' `
    "`${1}$VerAndroid`""

# 6. README.md
Replace-InFile "$RootDir\README.md" `
    '(releases/download/)\d+\.\d+\.\d+/' `
    "`${1}$VerDot/"

Write-Host "Done. Updated files:"
Write-Host "  CMakeLists.txt"
Write-Host "  Source\UI\app.cpp"
Write-Host "  Builder\Windows\XM8.rc"
Write-Host "  Builder\Android\app\build.gradle"
Write-Host "  Builder\Android\app\src\main\AndroidManifest.xml"
Write-Host "  README.md"
