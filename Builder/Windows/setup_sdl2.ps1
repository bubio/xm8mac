# SDL2 のバージョン
$version = "2.32.8"
$zipName = "SDL2-devel-$version-VC.zip"
$url = "https://github.com/libsdl-org/SDL/releases/download/release-$version/$zipName"

# 一時ディレクトリ
$tempDir = "SDL2_tmp"

# コピー先
$targetDir = "SDL"
$includeDir = "$targetDir\include"
$libX86Dir = "$targetDir\lib\x86"
$libX64Dir = "$targetDir\lib\x64"

Write-Host "Downloading SDL2 $version..."
Invoke-WebRequest $url -OutFile $zipName

Write-Host "Extracting $zipName..."
Expand-Archive $zipName -DestinationPath $tempDir -Force

Write-Host "Creating target directories..."
New-Item -ItemType Directory -Path $includeDir -Force | Out-Null
New-Item -ItemType Directory -Path $libX86Dir -Force | Out-Null
New-Item -ItemType Directory -Path $libX64Dir -Force | Out-Null

Write-Host "Copying include files..."
Copy-Item "$tempDir\SDL2-$version\include\*" $includeDir -Recurse -Force

Write-Host "Copying x86 library files..."
Copy-Item "$tempDir\SDL2-$version\lib\x86\*" $libX86Dir -Recurse -Force

Write-Host "Copying x64 library files..."
Copy-Item "$tempDir\SDL2-$version\lib\x64\*" $libX64Dir -Recurse -Force

Write-Host "Cleaning up..."
Remove-Item $tempDir -Recurse -Force
Remove-Item $zipName -Force

Write-Host "SDL2 setup completed."