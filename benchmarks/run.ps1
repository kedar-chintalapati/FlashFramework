param(
    [ValidateRange(1, 1000000)]
    [int]$RequestsPerConnection = 200,
    [ValidateRange(1, 512)]
    [int[]]$Connections = @(1, 8, 32, 128),
    [ValidateRange(1, 128)]
    [int[]]$WorkerCounts = @(1, 2),
    [ValidateRange(1, 65534)]
    [int]$BasePort = 19082,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDirectory = Join-Path $repository "build\benchmark-results\$stamp"
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

Push-Location $repository
try {
    cmake --build --preset native --parallel 2
    if ($LASTEXITCODE -ne 0) {
        throw "native build failed"
    }

    $commit = (git rev-parse HEAD).Trim()
    $compiler = (& "C:\msys64\ucrt64\bin\g++.exe" --version | Select-Object -First 1)
    $operatingSystem = Get-CimInstance Win32_OperatingSystem
    $processor = Get-CimInstance Win32_Processor | Select-Object -First 1
    $metadata = [ordered]@{
        commit = $commit
        timestamp = (Get-Date).ToString("o")
        operating_system = $operatingSystem.Caption
        operating_system_version = $operatingSystem.Version
        processor = $processor.Name.Trim()
        logical_processors = $processor.NumberOfLogicalProcessors
        compiler = $compiler
        build_preset = "native"
        worker_counts = $WorkerCounts
        requests_per_connection = $RequestsPerConnection
        connections = $Connections
    }
    [IO.File]::WriteAllText(
        (Join-Path $OutputDirectory "metadata.json"),
        ($metadata | ConvertTo-Json -Depth 4))

    $body100Path = Join-Path $OutputDirectory "body-100.json"
    $body1kPath = Join-Path $OutputDirectory "body-1k.json"
    $body64kPath = Join-Path $OutputDirectory "body-64k.json"
    [IO.File]::WriteAllText($body100Path, '{"value":42,"name":"' + ('x' * 72) + '"}')
    [IO.File]::WriteAllText($body1kPath, '{"value":42,"name":"' + ('x' * 996) + '"}')
    [IO.File]::WriteAllText($body64kPath, '{"value":42,"name":"' + ('x' * 65000) + '"}')

    $workloads = @(
        [pscustomobject]@{ name = "health"; method = "GET"; target = "/health"; body = ""; content_type = ""; status = 200; divisor = 1 },
        [pscustomobject]@{ name = "integer_add"; method = "GET"; target = "/add/20/22"; body = ""; content_type = ""; status = 200; divisor = 1 },
        [pscustomobject]@{ name = "json_output"; method = "GET"; target = "/object"; body = ""; content_type = ""; status = 200; divisor = 1 },
        [pscustomobject]@{ name = "json_echo_100"; method = "POST"; target = "/echo"; body = "@$body100Path"; content_type = "application/json"; status = 200; divisor = 1 },
        [pscustomobject]@{ name = "json_echo_1k"; method = "POST"; target = "/echo"; body = "@$body1kPath"; content_type = "application/json"; status = 200; divisor = 2 },
        [pscustomobject]@{ name = "json_echo_64k"; method = "POST"; target = "/echo"; body = "@$body64kPath"; content_type = "application/json"; status = 200; divisor = 20 },
        [pscustomobject]@{ name = "validation_failure"; method = "POST"; target = "/validate"; body = '{"value":0}'; content_type = "application/json"; status = 422; divisor = 1 },
        [pscustomobject]@{ name = "async_wait"; method = "GET"; target = "/wait?delay=1"; body = ""; content_type = ""; status = 200; divisor = 20 }
    )

    $servers = @(
        [pscustomobject]@{ name = "flash"; executable = "flash_benchmark_typed.exe"; port = $BasePort },
        [pscustomobject]@{ name = "manual"; executable = "flash_benchmark_manual.exe"; port = $BasePort + 1 }
    )
    $clientPath = Join-Path $repository "build\native\benchmarks\flash_benchmark_load.exe"
    $resultPath = Join-Path $OutputDirectory "results.jsonl"
    [IO.File]::WriteAllText($resultPath, "")

    $corePath = Join-Path $repository "build\native\benchmarks\flash_benchmark_core.exe"
    $coreResults = & $corePath
    if ($LASTEXITCODE -ne 0) {
        throw "core benchmark failed"
    }
    [IO.File]::WriteAllLines(
        (Join-Path $OutputDirectory "core.jsonl"),
        [string[]]$coreResults)

    $allocationPath = Join-Path `
        $repository "build\native\benchmarks\flash_benchmark_allocations.exe"
    $allocationResults = & $allocationPath
    if ($LASTEXITCODE -ne 0) {
        throw "allocation benchmark failed"
    }
    [IO.File]::WriteAllLines(
        (Join-Path $OutputDirectory "allocations.jsonl"),
        [string[]]$allocationResults)

    foreach ($workerCount in $WorkerCounts) {
        foreach ($server in $servers) {
            $serverPath = Join-Path $repository "build\native\benchmarks\$($server.executable)"
            $serverProcess = Start-Process `
                -FilePath $serverPath `
                -ArgumentList @([string]$server.port, [string]$workerCount) `
                -PassThru `
                -WindowStyle Hidden
            try {
                Start-Sleep -Milliseconds 500
                foreach ($connectionCount in $Connections) {
                    foreach ($workload in $workloads) {
                        $requestCount = [Math]::Max(
                            5, [Math]::Floor($RequestsPerConnection / $workload.divisor))
                        $arguments = @(
                            [string]$server.port,
                            $workload.method,
                            $workload.target,
                            [string]$connectionCount,
                            [string]$requestCount,
                            $workload.body,
                            $workload.content_type,
                            [string]$workload.status
                        )
                        $raw = & $clientPath @arguments
                        if ($LASTEXITCODE -ne 0) {
                            throw "$($server.name) $($workload.name) failed"
                        }
                        $measurement = $raw | ConvertFrom-Json
                        $measurement | Add-Member `
                            -NotePropertyName "server" `
                            -NotePropertyValue $server.name
                        $measurement | Add-Member `
                            -NotePropertyName "workload" `
                            -NotePropertyValue $workload.name
                        $measurement | Add-Member `
                            -NotePropertyName "workers" `
                            -NotePropertyValue $workerCount
                        [IO.File]::AppendAllText(
                            $resultPath,
                            ($measurement | ConvertTo-Json -Compress) + [Environment]::NewLine)
                        Write-Output `
                            "$($server.name) $($workload.name) workers=$workerCount c=$connectionCount complete"
                    }
                }
            }
            finally {
                if (!$serverProcess.HasExited) {
                    Stop-Process -Id $serverProcess.Id
                    $serverProcess.WaitForExit(5000) | Out-Null
                }
            }
        }
    }

    Write-Output "results=$resultPath"
}
finally {
    Pop-Location
}
