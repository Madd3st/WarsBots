[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SprintLog,
    [Parameter(Mandatory)][string]$NormalJumpLog,
    [Parameter(Mandatory)][string]$StrengthJumpLog,
    [string]$WalkLog,
    [string[]]$ClientLogs = @()
)
# Checks observations from real game runs. Does not launch/stop games or prove visual animation.
$ErrorActionPreference = 'Stop'
$warsCulture = [Globalization.CultureInfo]::InvariantCulture
function Get-WarsNumber([string]$Value) { [double]::Parse($Value, $warsCulture) }
function Assert-WarsObservation([bool]$Condition, [string]$Description) {
    if (-not $Condition) { throw "Missing/failed observation: $Description" }
    Write-Output "PASS: $Description"
}
$warsSprint = Get-Content -LiteralPath $SprintLog -Raw
$warsSamples = @([regex]::Matches($warsSprint,
    'ABILITY PHYSICS: speed=(?<speed>\d+\.\d+) vz=-?\d+\.\d+ grounded=\d sprint=(?<sprint>\d) mode=\d energy=(?<energy>\d+\.\d+) active=1') | ForEach-Object {
    [pscustomobject]@{ Speed = Get-WarsNumber $_.Groups['speed'].Value
        Sprint = $_.Groups['sprint'].Value -eq '1'; Energy = Get-WarsNumber $_.Groups['energy'].Value }
})
$warsFast = @($warsSamples | Where-Object Sprint)
$warsRun = @($warsSamples | Where-Object { -not $_.Sprint -and $_.Speed -gt 1 })
Assert-WarsObservation ($warsFast.Count -gt 0 -and $warsRun.Count -gt 0) 'physical sprint and normal movement samples'
$warsFastSpeed = ($warsFast | Measure-Object Speed -Maximum).Maximum
$warsRunSpeed = ($warsRun | Measure-Object Speed -Maximum).Maximum
Assert-WarsObservation ($warsFastSpeed -gt $warsRunSpeed * 1.2) "sprint faster than run ($warsFastSpeed vs $warsRunSpeed m/s)"
Assert-WarsObservation ($warsSprint.Contains('SPRINT START') -and $warsSprint.Contains('SPRINT STOP')) 'sprint start/stop'
$warsMinimumEnergy = ($warsFast | Measure-Object Energy -Minimum).Minimum
$warsRecoveredEnergy = ($warsRun | Measure-Object Energy -Maximum).Maximum
Assert-WarsObservation ($warsMinimumEnergy -lt 180 -and $warsRecoveredEnergy -gt $warsMinimumEnergy + 10) 'native energy consumption and recovery'

$warsRises = @()
foreach ($warsCase in @(@{ Path = $NormalJumpLog; Strength = 0 }, @{ Path = $StrengthJumpLog; Strength = 1 })) {
    $warsText = Get-Content -LiteralPath $warsCase.Path -Raw
    $warsKind = $warsCase.Strength
    $warsTakeoff = [regex]::Match($warsText, "JUMP TAKEOFF: strength=$warsKind vz=(?<vz>\d+\.\d+) energy=(?<energy>\d+\.\d+)")
    $warsLanding = [regex]::Match($warsText, "JUMP LANDING: strength=$warsKind rise=(?<rise>\d+\.\d+)")
    Assert-WarsObservation ($warsTakeoff.Success -and $warsLanding.Success) "physical takeoff and landing, strength=$warsKind"
    $warsRise = Get-WarsNumber $warsLanding.Groups['rise'].Value
    Assert-WarsObservation ($warsRise -gt 0.2) "measured jump height, strength=$warsKind ($warsRise m)"
    $warsRises += $warsRise
    if ($warsKind -eq 1) {
        $warsRequest = [regex]::Match($warsText, 'BOT ACTION: JUMP strength=1 energy=(?<energy>\d+\.\d+)')
        Assert-WarsObservation $warsRequest.Success 'strength jump request with measured initial energy'
        $warsCost = (Get-WarsNumber $warsRequest.Groups['energy'].Value) - (Get-WarsNumber $warsTakeoff.Groups['energy'].Value)
        Assert-WarsObservation ($warsCost -gt 60) "native Strength energy cost ($warsCost units before takeoff)"
    }
}
Assert-WarsObservation ($warsRises[1] -gt $warsRises[0] * 1.5) 'Strength jump higher than normal jump'
if ($WalkLog) {
    $warsWalk = Get-Content -LiteralPath $WalkLog -Raw
    $warsWalkSamples = @([regex]::Matches($warsWalk, 'ABILITY PHYSICS: speed=(?<speed>\d+\.\d+)') | Select-Object -First 3 | ForEach-Object {
        Get-WarsNumber $_.Groups['speed'].Value
    })
    $warsWalkSpeed = ($warsWalkSamples | Measure-Object -Maximum).Maximum
    Assert-WarsObservation ($warsWalkSpeed -gt 0.5 -and $warsWalkSpeed -lt $warsRunSpeed * 0.8) "walk slower than run ($warsWalkSpeed vs $warsRunSpeed m/s)"
    Assert-WarsObservation ($warsWalk.Contains('abilities require running owned bot and local IA gate') -and
        $warsWalk.Contains('AI start refused')) 'closed local IA gate rejects ability/start commands'
    Assert-WarsObservation ($warsWalk.Contains('cloak_experimental_disabled')) 'experimental cloak defaults OFF'
}
foreach ($warsClientLog in $ClientLogs) {
    $warsClient = Get-Content -LiteralPath $warsClientLog -Raw
    Assert-WarsObservation ($warsClient.Contains('Level multiplayer/ia/steelmill loading time') -and
        $warsClient -match 'NETWORK MOTION / SUIT REPLICA .*active=1') "client map and active suit replica: $warsClientLog"
}
Write-Output 'Requested log checks passed. Unprovided client logs, visual animation and combat regression remain unverified.'
