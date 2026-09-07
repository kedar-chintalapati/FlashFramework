param(
    [ValidateRange(1000, 100000000)]
    [int]$Iterations = 500000,
    [ValidateRange(1, 20)]
    [int]$Trials = 3,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
$compiler = "C:\msys64\ucrt64\bin\g++.exe"

if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "Required compiler was not found: $compiler"
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
$buildRoot = Join-Path $repository "build\profile-pgo-$stamp"
if ([IO.Directory]::Exists($buildRoot)) {
    throw "Generated build directory already exists: $buildRoot"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $resultRoot = Join-Path $repository "build\benchmark-results\pgo-$stamp"
}
else {
    $resultRoot = [IO.Path]::GetFullPath($OutputDirectory)
}
if ([IO.Directory]::Exists($resultRoot)) {
    throw "Profile result directory already exists: $resultRoot"
}

$baselineBuild = Join-Path $buildRoot "baseline"
$profiledBuild = Join-Path $buildRoot "profiled"
$dataPath = Join-Path $resultRoot "data"
$baselineResults = Join-Path $resultRoot "baseline"
$generationResults = Join-Path $resultRoot "generation"
$useResults = Join-Path $resultRoot "use"
foreach ($directory in @(
    $buildRoot, $resultRoot, $dataPath, $baselineResults,
    $generationResults, $useResults)) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
}

$profileDirectory = [IO.Path]::GetFullPath($dataPath)
$coreRelativePath = "benchmarks\flash_benchmark_core.exe"
$configureCommon = @(
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_CXX_COMPILER=$compiler",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DFLASH_BUILD_TESTS=OFF",
    "-DFLASH_BUILD_EXAMPLES=OFF",
    "-DFLASH_BUILD_BENCHMARKS=ON",
    "-DFLASH_NATIVE_OPTIMIZATION=ON"
)

function Invoke-ConfiguredCore {
    param(
        [string]$Name,
        [string]$BuildDirectory,
        [string]$Mode,
        [string]$ConfigureLog
    )

    $arguments = @(
        "-S", $repository,
        "-B", $BuildDirectory
    ) + $configureCommon + @(
        "-DFLASH_PROFILE_MODE=$Mode",
        "-DFLASH_PROFILE_DIR=$profileDirectory"
    )
    $configureOutput = & cmake @arguments 2>&1
    $configureOutput | Out-File -LiteralPath $ConfigureLog -Encoding ascii
    if ($LASTEXITCODE -ne 0) {
        throw "$Name configure failed; see $ConfigureLog"
    }

    $buildOutput = & cmake --build $BuildDirectory --target flash_benchmark_core `
        --parallel 2 --verbose 2>&1
    $buildOutput | Out-File -LiteralPath $ConfigureLog -Append -Encoding ascii
    if ($LASTEXITCODE -ne 0) {
        throw "$Name build failed; see $ConfigureLog"
    }
}

function Invoke-CoreTrials {
    param(
        [string]$Name,
        [string]$BuildDirectory,
        [string]$ResultDirectory,
        [int]$Count
    )

    $benchmark = Join-Path $BuildDirectory $coreRelativePath
    if (-not (Test-Path -LiteralPath $benchmark -PathType Leaf)) {
        throw "$Name benchmark executable was not found: $benchmark"
    }
    for ($trial = 1; $trial -le $Count; ++$trial) {
        $trialDirectory = Join-Path $ResultDirectory "trial-$trial"
        [IO.Directory]::CreateDirectory($trialDirectory) | Out-Null
        $jsonPath = Join-Path $trialDirectory "core.jsonl"
        Push-Location $trialDirectory
        try {
            & $benchmark ([string]$Iterations) | Out-File -LiteralPath $jsonPath -Encoding ascii
            if ($LASTEXITCODE -ne 0) {
                throw "$Name trial $trial failed"
            }
        }
        finally {
            Pop-Location
        }
    }
}

$baselineLog = Join-Path $baselineResults "build.log"
$generationLog = Join-Path $generationResults "build.log"
$useLog = Join-Path $useResults "build.log"

Invoke-ConfiguredCore "baseline" $baselineBuild "OFF" $baselineLog
Invoke-ConfiguredCore "generation" $profiledBuild "GENERATE" $generationLog
Invoke-CoreTrials "generation" $profiledBuild $generationResults 1

$profileFiles = @(Get-ChildItem -LiteralPath $profileDirectory -Recurse -File -Filter "*.gcda")
$nonemptyProfileFiles = @($profileFiles | Where-Object { $_.Length -gt 0 })
if ($nonemptyProfileFiles.Count -eq 0) {
    throw "GCC profile generation produced no nonempty .gcda files in $profileDirectory"
}

Invoke-ConfiguredCore "use" $profiledBuild "USE" $useLog
$missingProfileDiagnostics = Select-String -LiteralPath $useLog -Pattern `
    "(?i)missing profile|profile count data file not found|profile data.*not found|not found.*profile"
if ($null -ne $missingProfileDiagnostics) {
    throw "GCC reported missing profile data; see $useLog"
}
Invoke-CoreTrials "baseline" $baselineBuild $baselineResults $Trials
Invoke-CoreTrials "use" $profiledBuild $useResults $Trials

$commit = (& git -C $repository rev-parse HEAD 2>$null).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commit)) {
    throw "Source commit could not be read"
}
$os = Get-CimInstance Win32_OperatingSystem
$processor = Get-CimInstance Win32_Processor | Select-Object -First 1
$configureCommand = "cmake -S `"$repository`" -G Ninja -DCMAKE_BUILD_TYPE=Release " +
    "-DCMAKE_CXX_COMPILER=$compiler -DCMAKE_TOOLCHAIN_FILE=$toolchain " +
    "-DFLASH_BUILD_TESTS=OFF -DFLASH_BUILD_EXAMPLES=OFF " +
    "-DFLASH_BUILD_BENCHMARKS=ON -DFLASH_NATIVE_OPTIMIZATION=ON"
$buildCommand = "cmake --build <mode-build-directory> --target flash_benchmark_core --parallel 2 --verbose"
$runCommand = "flash_benchmark_core.exe $Iterations"
$metadata = [ordered]@{
    timestamp = (Get-Date).ToString("o")
    source_commit = $commit
    operating_system = $os.Caption
    operating_system_version = $os.Version
    processor = $processor.Name.Trim()
    logical_processors = $processor.NumberOfLogicalProcessors
    compiler = (& $compiler -dumpfullversion).Trim()
    compiler_path = $compiler
    build_type = "Release"
    build_jobs = 2
    native_optimization = $true
    profile_mode_generate = "-fprofile-generate=$profileDirectory"
    profile_mode_use = "-fprofile-use=$profileDirectory -fprofile-correction -Wmissing-profile"
    profile_directory = $profileDirectory
    profile_file_count = $nonemptyProfileFiles.Count
    profile_file_bytes = ($nonemptyProfileFiles | Measure-Object Length -Sum).Sum
    profile_files = @($nonemptyProfileFiles | ForEach-Object { $_.FullName })
    iterations = $Iterations
    trials = $Trials
    configure_command = $configureCommand
    build_command = $buildCommand
    run_command = $runCommand
    baseline_build_directory = $baselineBuild
    generation_build_directory = $profiledBuild
    use_build_directory = $profiledBuild
    profile_build_reused = $true
    profile_build_reuse_reason = "GCC profile file names include the object path"
    result_directory = $resultRoot
    scope = "framework request-processing microbenchmarks; excludes transport and sockets"
}
$metadata | ConvertTo-Json -Depth 5 | Out-File `
    -LiteralPath (Join-Path $resultRoot "metadata.json") -Encoding ascii

Write-Output "profile_results=$resultRoot"
Write-Output "profile_data=$profileDirectory"
