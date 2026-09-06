param([string]$Label = 'release')

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
if (Get-Process cc1plus -ErrorAction SilentlyContinue) {
    throw 'Wait for the existing compiler process before measuring'
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$buildPath = Join-Path $repository "build/measure-$stamp"
if (Test-Path -LiteralPath $buildPath) { throw 'Measurement directory already exists' }
$toolchain = Join-Path $repository 'build/conan/release/conan_toolchain.cmake'
cmake -S $repository -B $buildPath -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    -DFLASH_BUILD_TESTS=OFF -DFLASH_BUILD_BENCHMARKS=OFF
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }

$buildArguments = @('--build', ('"' + $buildPath + '"'), '--target',
    'flash_hello', 'flash_openapi_export', '--parallel', '2')
$timer = [Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath (Get-Command cmake).Source `
    -ArgumentList $buildArguments -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $buildPath 'build.log') `
    -RedirectStandardError (Join-Path $buildPath 'build-errors.log')
$peakCompilerBytes = 0L
do {
    $compilerBytes = (Get-Process cc1plus -ErrorAction SilentlyContinue |
        Measure-Object WorkingSet64 -Sum).Sum
    $peakCompilerBytes = [Math]::Max($peakCompilerBytes, [long]$compilerBytes)
    Start-Sleep -Milliseconds 200
    $process.Refresh()
} while (!$process.HasExited)
$process.WaitForExit()
$timer.Stop()
if ($process.ExitCode -ne 0) { throw "Build failed, see $buildPath" }
$report = [ordered]@{
    label = $Label
    commit = (& git -C $repository rev-parse HEAD).Trim()
    compiler = (& C:/msys64/ucrt64/bin/g++.exe -dumpfullversion)
    build_seconds = $timer.Elapsed.TotalSeconds
    sampled_peak_compiler_working_set_bytes = $peakCompilerBytes
    sampling_interval_ms = 200
    build_jobs = 2
    runtime_library_bytes = (Get-Item (Join-Path $buildPath 'libflash_runtime.a')).Length
    hello_bytes = (Get-Item (Join-Path $buildPath 'examples/flash_hello.exe')).Length
    openapi_export_bytes = (Get-Item (Join-Path $buildPath 'examples/flash_openapi_export.exe')).Length
}
$json = $report | ConvertTo-Json
[IO.File]::WriteAllText((Join-Path $buildPath 'measurements.json'), $json)
Write-Output $json
Write-Output $buildPath
