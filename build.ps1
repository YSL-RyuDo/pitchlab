param([string]$BuildDirectory = (Join-Path ([System.IO.Path]::GetTempPath()) 'PitchLab-build'))
$ErrorActionPreference = 'Stop'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakePath = $cmakeCommand.Source
} else {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'Install Visual Studio 2022 C++ desktop development and CMake first.' }
    $vsPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmakePath = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path -LiteralPath $cmakePath)) { throw 'CMake 3.24 or newer is required.' }
}
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
$dependencyDirectory = Join-Path $BuildDirectory 'dependencies'
$juceDirectory = Join-Path $dependencyDirectory 'JUCE'
$rubberbandDirectory = Join-Path $dependencyDirectory 'rubberband'
$juceArchive = Join-Path $PSScriptRoot 'ThirdParty\JUCE-8.0.15-source.zip'
$rubberbandArchive = Join-Path $PSScriptRoot 'ThirdParty\RubberBand-4.0.0-source.zip'
if ((Test-Path -LiteralPath $juceArchive) -and -not (Test-Path -LiteralPath (Join-Path $juceDirectory 'CMakeLists.txt'))) {
    Expand-Archive -LiteralPath $juceArchive -DestinationPath $juceDirectory
}
if ((Test-Path -LiteralPath $rubberbandArchive) -and -not (Test-Path -LiteralPath (Join-Path $rubberbandDirectory 'single\RubberBandSingle.cpp'))) {
    Expand-Archive -LiteralPath $rubberbandArchive -DestinationPath $rubberbandDirectory
}
$juceSource = if (Test-Path -LiteralPath (Join-Path $juceDirectory 'CMakeLists.txt')) { $juceDirectory } else { '' }
$rubberbandSource = if (Test-Path -LiteralPath (Join-Path $rubberbandDirectory 'single\RubberBandSingle.cpp')) { $rubberbandDirectory } else { '' }
& $cmakePath -S $PSScriptRoot -B $BuildDirectory -G 'Visual Studio 17 2022' -A x64 "-DFETCHCONTENT_SOURCE_DIR_JUCE=$juceSource" "-DFETCHCONTENT_SOURCE_DIR_RUBBERBAND=$rubberbandSource"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmakePath --build $BuildDirectory --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
$builtApp = Join-Path $BuildDirectory 'PitchLab_artefacts\Release\PitchLab.exe'
Copy-Item -LiteralPath $builtApp -Destination (Join-Path $PSScriptRoot 'PitchLab.exe') -Force
$testDirectory = Join-Path $BuildDirectory 'verification'
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
try {
    $testProcess = Start-Process -FilePath $builtApp -ArgumentList @('--self-test',('"' + $testDirectory + '"')) -WindowStyle Hidden -Wait -PassThru
} catch {
    throw "Compilation succeeded and PitchLab.exe was copied, but Windows could not run the verification executable. Verification was NOT completed. $($_.Exception.Message)"
}
if ($testProcess.ExitCode -ne 0) { throw "Audio verification failed. Read $testDirectory\integration-test.txt" }
Copy-Item -LiteralPath (Join-Path $testDirectory 'integration-test.txt') -Destination (Join-Path $PSScriptRoot 'TEST-RESULTS.txt') -Force
Write-Host 'Build and audio verification passed. Launch PitchLab.exe.'
