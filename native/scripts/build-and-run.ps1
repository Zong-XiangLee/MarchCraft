[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-Checked {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,

        [Parameter(Mandatory)]
        [string[]] $ArgumentList,

        [Parameter(Mandatory)]
        [string] $FailureMessage
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "$FailureMessage (exit code $LASTEXITCODE)."
    }
}

$worktree = if ([string]::IsNullOrWhiteSpace($env:CODEX_WORKTREE_PATH)) {
    [System.IO.Path]::GetFullPath((Get-Location).Path)
} else {
    [System.IO.Path]::GetFullPath($env:CODEX_WORKTREE_PATH)
}

$sharedSourceTree = if (-not [string]::IsNullOrWhiteSpace($env:CODEX_SOURCE_TREE_PATH)) {
    [System.IO.Path]::GetFullPath($env:CODEX_SOURCE_TREE_PATH)
} else {
    $commonGitDirectory = (& git -C $worktree rev-parse --path-format=absolute --git-common-dir).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commonGitDirectory)) {
        throw "Unable to locate the shared MarchCraft checkout from $worktree."
    }
    [System.IO.Path]::GetFullPath((Split-Path -Parent $commonGitDirectory))
}

$sharedNative = Join-Path $sharedSourceTree "native"
$source = Join-Path $worktree "native"
$build = Join-Path $source "build-worktree-mingw"

$compilerBin = Join-Path $sharedNative ".qt\Tools\mingw1310_64\bin"
$qtRoot = Join-Path $sharedNative ".qt\6.8.3\mingw_64"
$qtBin = Join-Path $qtRoot "bin"

$cmakeCandidates = @(
    (Join-Path $sharedNative ".tools\bin\cmake.exe"),
    (Join-Path $sharedNative ".tools\cmake\data\bin\cmake.exe"),
    "C:\Program Files\CMake\bin\cmake.exe"
)
$cmake = $cmakeCandidates | Where-Object {
    Test-Path -LiteralPath $_ -PathType Leaf
} | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($cmake)) {
    throw "CMake was not found. Checked: $($cmakeCandidates -join ', ')"
}
$cCompiler = Join-Path $compilerBin "gcc.exe"
$cxxCompiler = Join-Path $compilerBin "g++.exe"
$makeProgram = Join-Path $compilerBin "mingw32-make.exe"
$winDeployQt = Join-Path $qtBin "windeployqt.exe"
$executable = Join-Path $build "marchcraft.exe"

$requiredFiles = @(
    $cmake,
    $cCompiler,
    $cxxCompiler,
    $makeProgram,
    $winDeployQt,
    (Join-Path $qtBin "qmake.exe"),
    (Join-Path $source "CMakeLists.txt")
)
foreach ($requiredFile in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required build file was not found: $requiredFile"
    }
}

$env:PATH = "$compilerBin;$qtBin;$env:PATH"

Write-Host "Shared toolchain checkout: $sharedSourceTree"
Write-Host "Current worktree: $worktree"
Write-Host "Configuring MarchCraft in $build"
Invoke-Checked -FilePath $cmake -ArgumentList @(
    "-S", $source,
    "-B", $build,
    "-G", "MinGW Makefiles",
    "-DCMAKE_C_COMPILER=$cCompiler",
    "-DCMAKE_CXX_COMPILER=$cxxCompiler",
    "-DCMAKE_MAKE_PROGRAM=$makeProgram",
    "-DCMAKE_PREFIX_PATH=$qtRoot",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DMARCHCRAFT_SYNC_PRODUCTION_PREVIEW=OFF"
) -FailureMessage "MarchCraft configuration failed"

Write-Host "Building MarchCraft"
Invoke-Checked -FilePath $cmake -ArgumentList @(
    "--build", $build,
    "--target", "marchcraft",
    "--parallel", "2"
) -FailureMessage "MarchCraft compilation failed"

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Compilation completed without producing $executable"
}

Write-Host "Deploying Qt runtime dependencies"
Invoke-Checked -FilePath $winDeployQt -ArgumentList @(
    "--release",
    "--compiler-runtime",
    "--qmldir", (Join-Path $source "qml"),
    $executable
) -FailureMessage "Qt deployment failed"

Write-Host "Launching MarchCraft"
$process = Start-Process -FilePath $executable -WorkingDirectory $build -PassThru
Write-Host "MarchCraft launched successfully (PID $($process.Id))."
