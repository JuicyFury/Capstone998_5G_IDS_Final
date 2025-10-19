param(
    [string]$BuildDir = ".",
    [string]$Binary = "nr_ddos_dataset",
    [int]$Repetitions = 70
)

$ErrorActionPreference = "Stop"

$outRoot = Join-Path -Path (Get-Location) -ChildPath "datasets"
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

$ueCounts = @(10, 15, 25, 50)
$modes = @("mobile", "static")
$transports = @("udp", "tcp")
$attack = @("ddos", "benign")
$intensity = @(
    @{ tag = "low"; interval = "0.0008" },
    @{ tag = "med"; interval = "0.0004" },
    @{ tag = "high"; interval = "0.00015" }
)

for ($r = 1; $r -le $Repetitions; $r++) {
    foreach ($ue in $ueCounts) {
        foreach ($mode in $modes) {
            foreach ($tp in $transports) {
                foreach ($atk in $attack) {
                    foreach ($inten in $intensity) {
                        $mobile = if ($mode -eq "mobile") { "1" } else { "0" }
                        $useTcp = if ($tp -eq "tcp") { "1" } else { "0" }
                        $enableAttack = if ($atk -eq "ddos") { "1" } else { "0" }

                        $tag = "${ue}_${mode}_${tp}_${atk}_${($inten.tag)}_r${r}"
                        $args = @(
                            "--ueTotal=$ue",
                            "--mobile=$mobile",
                            "--useTcp=$useTcp",
                            "--enableAttack=$enableAttack",
                            "--attackInterval=$($inten.interval)",
                            "--simTime=20",
                            "--outPath=$outRoot",
                            "--scenarioTag=$tag",
                            "--window=1"
                        )

                        Write-Host "Running $tag"
                        & "$BuildDir\$Binary" $args | Out-Null
                    }
                }
            }
        }
    }
}

Write-Host "All runs complete. Datasets in: $outRoot"


