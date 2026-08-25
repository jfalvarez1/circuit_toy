# Build, package and publish a Windows release.
#   pwsh tools/make_release.ps1            # package only (dist/circuit-playground-windows-vX.Y.Z.zip)
#   pwsh tools/make_release.ps1 -Publish   # + git tag + GitHub release with the zip (needs gh auth)
# The version comes from include/version.h; the auto-updater downloads exactly this asset name.
param([switch]$Publish, [string]$Notes = "")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root
$ver = (Select-String -Path include/version.h -Pattern 'APP_VERSION "([^"]+)"').Matches[0].Groups[1].Value
$tag = "v$ver"
Write-Host "Version $tag"
if (Get-Process circuit-playground -ErrorAction SilentlyContinue) { throw "close circuit-playground.exe first (it blocks relinking)" }
meson compile -C build
if ($LASTEXITCODE -ne 0) { throw "build failed" }
& build/tools/template_smoke.exe | Select-Object -Last 1
& build/tools/template_smoke.exe --demo-test | Select-Object -Last 1
& build/circuit-playground.exe --layout-test | Select-Object -Last 1
$stage = Join-Path $root "dist/circuit-playground-$tag"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item build/circuit-playground.exe $stage
Copy-Item build/tools/template_smoke.exe $stage
Copy-Item guide.html, README.md, TEST_PLAN.md, TEMPLATE_AUDIT.md $stage
Copy-Item -Recurse docs (Join-Path $stage docs)
Get-ChildItem build -Filter *.dll -ErrorAction SilentlyContinue | Copy-Item -Destination $stage
$zip = Join-Path $root "dist/circuit-playground-windows-$tag.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Write-Host "Packaged $zip ($([math]::Round((Get-Item $zip).Length/1MB,1)) MB)"
if ($Publish) {
    if (-not $Notes) { $Notes = "Circuit Playground $tag - see README.md and docs/ for what changed." }
    if (-not (git tag -l $tag)) { git tag -a $tag -m "Release $tag"; git push origin $tag }
    gh release create $tag $zip --title "$tag" --notes $Notes
    Write-Host "Published https://github.com/jfalvarez1/circuit_toy/releases/tag/$tag"
}
