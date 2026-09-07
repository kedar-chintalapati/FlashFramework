param(
    [ValidateRange(1000, 100000000)]
    [int]$Iterations = 500000,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
$compiler = "C:\msys64\ucrt64\bin\g++.exe"
$gprof = "C:\msys64\ucrt64\bin\gprof.exe"

if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "Required compiler was not found: $compiler"
}
if (-not (Test-Path -LiteralPath $gprof -PathType Leaf)) {
    throw "Required gprof executable was not found: $gprof"
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "Required tool was not found on PATH: cmake"
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw "Required tool was not found on PATH: ninja"
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Required tool was not found on PATH: git"
}

$toolchain = Join-Path $repository "build\conan\release\conan_toolchain.cmake"
if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
    throw "Release Conan toolchain was not found: $toolchain"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
$buildPath = Join-Path $repository "build\profile-$stamp"
if ([IO.Directory]::Exists($buildPath)) {
    throw "Generated build directory already exists: $buildPath"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repository "build\benchmark-results\profile-$stamp"
}
$resultPath = [IO.Path]::GetFullPath($OutputDirectory)
if ([IO.Directory]::Exists($resultPath)) {
    throw "Profile result directory already exists: $resultPath"
}
[IO.Directory]::CreateDirectory($resultPath) | Out-Null

$configureArguments = @(
    "-S", $repository,
    "-B", $buildPath,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_CXX_COMPILER=$compiler",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DFLASH_BUILD_TESTS=OFF",
    "-DFLASH_BUILD_EXAMPLES=OFF",
    "-DFLASH_BUILD_BENCHMARKS=ON",
    "-DFLASH_NATIVE_OPTIMIZATION=ON",
    "-DFLASH_PROFILE=ON"
)
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) {
    throw "Profile benchmark configure failed"
}

& cmake --build $buildPath --target flash_benchmark_core --parallel 2
if ($LASTEXITCODE -ne 0) {
    throw "Profile benchmark build failed"
}

$benchmark = Join-Path $buildPath "benchmarks\flash_benchmark_core.exe"
if (-not (Test-Path -LiteralPath $benchmark -PathType Leaf)) {
    throw "Built benchmark executable was not found: $benchmark"
}

$jsonPath = Join-Path $resultPath "core.jsonl"
$gmonPath = Join-Path $resultPath "gmon.out"
$gprofPath = Join-Path $resultPath "gprof.txt"
$metadataPath = Join-Path $resultPath "metadata.json"

Push-Location $resultPath
try {
    & $benchmark ([string]$Iterations) | Out-File -LiteralPath $jsonPath -Encoding ascii
    if ($LASTEXITCODE -ne 0) {
        throw "Profile benchmark execution failed"
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $gmonPath -PathType Leaf)) {
    throw "gprof data was not produced: $gmonPath"
}

& $gprof $benchmark $gmonPath | Out-File -LiteralPath $gprofPath -Encoding ascii
if ($LASTEXITCODE -ne 0) {
    throw "gprof report generation failed"
}
if (-not (Test-Path -LiteralPath $gprofPath -PathType Leaf)) {
    throw "gprof report was not produced: $gprofPath"
}
if ((Get-Item -LiteralPath $gprofPath).Length -eq 0) {
    throw "gprof report is empty: $gprofPath"
}

$sourceCommit = (& git -C $repository rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($sourceCommit)) {
    throw "Source commit could not be read"
}

$metadata = [ordered]@{
    timestamp = (Get-Date).ToString("o")
    source_commit = $sourceCommit
    operating_system = [Runtime.InteropServices.RuntimeInformation]::OSDescription
    machine = [Environment]::MachineName
    build_directory = $buildPath
    result_directory = $resultPath
    compiler = (& $compiler -dumpfullversion).Trim()
    compiler_path = $compiler
    gprof = (& $gprof --version | Select-Object -First 1)
    gprof_path = $gprof
    build_type = "Release"
    build_jobs = 2
    native_optimization = $true
    profile_flag = "-pg"
    iterations = $Iterations
    benchmark = "flash_benchmark_core"
    benchmark_command = "$benchmark $Iterations"
    configure_arguments = ($configureArguments -join " ")
    scope = "framework request-processing microbenchmarks; excludes transport and sockets"
}
$metadata | ConvertTo-Json | Out-File -LiteralPath $metadataPath -Encoding ascii

Write-Output "profile_results=$resultPath"
Write-Output "gprof_report=$gprofPath"
