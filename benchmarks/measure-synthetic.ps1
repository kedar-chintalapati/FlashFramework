param(
    [ValidateSet(1, 10, 100, 1000)]
    [int]$Routes = 100,
    [ValidateRange(1073741824, 4294967296)]
    [long]$MaxCompilerBytes = 2684354560
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
if (Get-Process cc1plus -ErrorAction SilentlyContinue) {
    throw 'Wait for the existing compiler process before measuring'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$buildPath = Join-Path $repository "build/synthetic-$Routes-$stamp"
if (Test-Path -LiteralPath $buildPath) {
    throw 'Measurement directory already exists'
}
$toolchain = Join-Path $repository 'build/conan/release/conan_toolchain.cmake'
cmake -S $repository -B $buildPath -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    -DFLASH_BUILD_TESTS=OFF `
    -DFLASH_BUILD_EXAMPLES=OFF `
    -DFLASH_BUILD_BENCHMARKS=ON `
    -DFLASH_NATIVE_OPTIMIZATION=ON
if ($LASTEXITCODE -ne 0) {
    throw 'Configure failed'
}

$target = "flash_synthetic_routes_$Routes"
$buildArguments = @(
    '--build', ('"' + $buildPath + '"'), '--target', $target, '--parallel', '2')
$timer = [Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath (Get-Command cmake).Source `
    -ArgumentList $buildArguments -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $buildPath 'build.log') `
    -RedirectStandardError (Join-Path $buildPath 'build-errors.log')
$peakCompilerBytes = 0L
$memoryLimitReached = $false
do {
    $compilerProcesses = @(Get-Process cc1plus -ErrorAction SilentlyContinue)
    $compilerBytes = ($compilerProcesses | Measure-Object WorkingSet64 -Sum).Sum
    $peakCompilerBytes = [Math]::Max($peakCompilerBytes, [long]$compilerBytes)
    if ($compilerBytes -gt $MaxCompilerBytes) {
        $memoryLimitReached = $true
        foreach ($compilerProcess in $compilerProcesses) {
            Stop-Process -Id $compilerProcess.Id -ErrorAction SilentlyContinue
        }
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
        break
    }
    Start-Sleep -Milliseconds 200
    $process.Refresh()
} while (!$process.HasExited)
$process.WaitForExit()
$timer.Stop()

if ($memoryLimitReached) {
    throw "Compiler memory safety limit reached, see $buildPath"
}
if ($process.ExitCode -ne 0) {
    throw "Build failed, see $buildPath"
}

$executable = Join-Path $buildPath "benchmarks/$target.exe"
& $executable
if ($LASTEXITCODE -ne 0) {
    throw "Generated route check failed, see $buildPath"
}
$report = [ordered]@{
    commit = (& git -C $repository rev-parse HEAD).Trim()
    compiler = (& C:/msys64/ucrt64/bin/g++.exe -dumpfullversion)
    generated_routes = $Routes
    build_seconds = $timer.Elapsed.TotalSeconds
    sampled_peak_compiler_working_set_bytes = $peakCompilerBytes
    compiler_memory_safety_limit_bytes = $MaxCompilerBytes
    sampling_interval_ms = 200
    build_jobs = 2
    runtime_check = 'passed'
    executable_bytes = (Get-Item $executable).Length
}
$json = $report | ConvertTo-Json
[IO.File]::WriteAllText((Join-Path $buildPath 'measurements.json'), $json)
Write-Output $json
Write-Output $buildPath
