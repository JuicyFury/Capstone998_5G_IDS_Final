param(
    [string]$NsPath = "C:\ns3\ns-3.36", # Path to ns-3 installation
    [string]$Repetitions = 3,          # Reduced from 70 for testing
    [switch]$CopyOnly = $false         # If true, only copy files, don't run
)

# Create datasets directory
$outDir = Join-Path $PSScriptRoot "datasets"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Copy our dataset generator to ns-3 scratch directory
$srcFile = Join-Path $PSScriptRoot "simple_ddos_dataset.cc"
$dstFile = Join-Path $NsPath "scratch\simple_ddos_dataset.cc"
Copy-Item -Path $srcFile -Destination $dstFile -Force
Write-Host "Copied dataset generator to $dstFile"

if ($CopyOnly) {
    Write-Host "Files copied. Exiting without running simulations."
    exit 0
}

# Configurations for batch runs
$ueCounts = @(10,15)
$modes = @("mobile","static")
$transports = @("udp","tcp")
$attack = @("ddos","benign")
$intensity = @(
    @{ tag = "low";  interval = "0.0008" },
    @{ tag = "med";  interval = "0.0004" },
    @{ tag = "high"; interval = "0.00015" }
)

# Change to ns-3 directory
Push-Location $NsPath

# Run a quick smoke test
$smokeCmd = "./waf --run 'scratch/simple_ddos_dataset --ueTotal=10 --mobile=1 --enableAttack=1 --attackInterval=0.0004 --simTime=10 --outPath=$outDir --scenarioTag=smoke --window=1'"
Write-Host "Running smoke test: $smokeCmd"
Invoke-Expression $smokeCmd

# Check if smoke test created a file
$smokeFile = Join-Path $outDir "dataset_smoke_ue10_udp_ddos.csv"
if (Test-Path $smokeFile) {
    Write-Host "Smoke test successful! File created: $smokeFile"
    
    # Run batch simulations
    for ($r = 1; $r -le $Repetitions; $r++) {
        foreach ($ue in $ueCounts) {
            foreach ($mode in $modes) {
                foreach ($tp in $transports) {
                    foreach ($atk in $attack) {
                        foreach ($inten in $intensity) {
                            $mobile = if ($mode -eq "mobile") { "1" } else { "0" }
                            $useTcp = if ($tp -eq "tcp") { "1" } else { "0" }
                            $enableAttack = if ($atk -eq "ddos") { "1" } else { "0" }
                            
                            $tag = "${ue}_${mode}_${tp}_${atk}_$($inten.tag)_r${r}"
                            $cmd = "./waf --run 'scratch/simple_ddos_dataset --ueTotal=$ue --mobile=$mobile --useTcp=$useTcp --enableAttack=$enableAttack --attackInterval=$($inten.interval) --simTime=20 --outPath=$outDir --scenarioTag=$tag --window=1'"
                            
                            Write-Host "Running scenario: $tag"
                            Invoke-Expression $cmd
                        }
                    }
                }
            }
        }
    }
} else {
    Write-Host "ERROR: Smoke test failed! No output file created."
}

Pop-Location
Write-Host "All simulations complete. Datasets saved to: $outDir"
