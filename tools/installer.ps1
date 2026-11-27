# Installe sur Windows la derniere version publiee d'un plugin du depot droz-lab/plugins.
#   irm https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.ps1 | iex
# Le plus simple reste le Droz Software Center (voir le README). Ce script sert sans lui :
#   $env:DROZ_PLUGIN = "<dossier>"  avant la commande.
$ErrorActionPreference = "Stop"
$plugin = $env:DROZ_PLUGIN
if (-not $plugin) { Write-Host 'Donner le dossier du plugin : $env:DROZ_PLUGIN = "<dossier>" avant la commande.'; return }
$dest = Join-Path $env:CommonProgramFiles "VST3"

$versions = Invoke-RestMethod "https://api.github.com/repos/droz-lab/plugins/releases?per_page=100" -Headers @{ "User-Agent" = "droz-lab" }
$version = $versions | Where-Object { $_.tag_name -like "$plugin-v*" } | Select-Object -First 1
$fichier = if ($version) { $version.assets | Where-Object { $_.name -like "*-Windows.zip" } | Select-Object -First 1 }
if (-not $fichier) {
    Write-Host "Aucune version publiee pour '$plugin'. Voir https://github.com/droz-lab/plugins/releases"
    return
}

$tmp = Join-Path $env:TEMP ("droz-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
    Write-Host "Telechargement : $($fichier.browser_download_url)"
    Invoke-WebRequest $fichier.browser_download_url -OutFile (Join-Path $tmp "plugin.zip")
    Expand-Archive (Join-Path $tmp "plugin.zip") -DestinationPath (Join-Path $tmp "contenu")
    $source = Join-Path $tmp "contenu\*"
    $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if ($admin) {
        New-Item -ItemType Directory -Force -Path $dest | Out-Null
        Copy-Item -Path $source -Destination $dest -Recurse -Force
    } else {
        # Le dossier VST3 du systeme demande les droits administrateur : Windows va le demander une fois.
        $commande = "New-Item -ItemType Directory -Force -Path '$dest' | Out-Null; Copy-Item -Path '$source' -Destination '$dest' -Recurse -Force"
        Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile", "-Command", $commande
    }
    Write-Host "Installe dans $dest ($($version.tag_name)). Ferme le DAW avant de remplacer une version, puis relance le scan des plugins."
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
