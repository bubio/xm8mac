# SDL2 version
$version = "2.32.8"
$zipName = "SDL2-devel-$version-VC.zip"
$url = "https://github.com/libsdl-org/SDL/releases/download/release-$version/$zipName"

# Temporary directory
$tempDir = "SDL2_tmp"

# Destination directory
$targetDir = "SDL"
$includeDir = "$targetDir\include"
$libX86Dir = "$targetDir\lib\x86"
$libX64Dir = "$targetDir\lib\x64"
$libArm64Dir = "$targetDir\lib\arm64"

# 1. Download and extract SDL2 binary ZIP, then copy headers and libraries for x86/x64
Write-Host "Downloading SDL2 $version..."
Invoke-WebRequest $url -OutFile $zipName
Write-Host "Extracting $zipName..."
Expand-Archive $zipName -DestinationPath $tempDir -Force

# Write-Host "Creating target directories..."
New-Item -ItemType Directory -Path $includeDir -Force | Out-Null
New-Item -ItemType Directory -Path $libX86Dir -Force | Out-Null
New-Item -ItemType Directory -Path $libX64Dir -Force | Out-Null
New-Item -ItemType Directory -Path $libArm64Dir -Force | Out-Null

Write-Host "Copying include files..."
Copy-Item "$tempDir\SDL2-$version\include\*" $includeDir -Recurse -Force

Write-Host "Copying x86 library files..."
Copy-Item "$tempDir\SDL2-$version\lib\x86\*" $libX86Dir -Recurse -Force

Write-Host "Copying x64 library files..."
Copy-Item "$tempDir\SDL2-$version\lib\x64\*" $libX64Dir -Recurse -Force

Write-Host "Cleaning up binary extraction..."
Remove-Item $tempDir -Recurse -Force
Remove-Item $zipName -Force

# 2. Download and extract SDL2 source ZIP, build with CMake for ARM64, then copy headers and libraries
$sourceZipName = "SDL2-$version.zip"
$sourceUrl = "https://github.com/libsdl-org/SDL/releases/download/release-$version/$sourceZipName"
Write-Host "Downloading SDL2 source $version for ARM64..."
Invoke-WebRequest $sourceUrl -OutFile $sourceZipName

Write-Host "Extracting $sourceZipName..."
Expand-Archive $sourceZipName -DestinationPath $tempDir -Force

# Set source directory for CMake
$srcDir = "SDL2-$version"
$buildDir = "$tempDir\build"
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

Write-Host "Configuring build with CMake for ARM64..."
Push-Location $buildDir
Write-Host "Current directory:" (Get-Location)

cmake ..\$srcDir -A ARM64 -DSDL_LIBC=ON
Write-Host "Building SDL2 for ARM64..."
cmake --build . --config Release
Pop-Location

Write-Host "Copying ARM64 include files..."
Copy-Item "$tempDir\SDL2-$version\include\*" $includeDir -Recurse -Force

Write-Host "Copying ARM64 library files..."
Copy-Item "$buildDir\Release\SDL2.dll" $libArm64Dir -Force
Copy-Item "$buildDir\Release\SDL2.lib" $libArm64Dir -Force
Copy-Item "$buildDir\Release\SDL2main.lib" $libArm64Dir -Force

Write-Host "Cleaning up source extraction..."
Remove-Item $tempDir -Recurse -Force
Remove-Item $sourceZipName -Force

Write-Host "SDL2 setup completed for x86, x64, arm64."
