# Checkpoints for the mud mod: save and restore the mod's code and content.
#   checkpoint.ps1 save "name"     - new checkpoint <Store>/v<NNN>_<date-time>_<name> (next version number)
#   checkpoint.ps1 list            - list checkpoints
#   checkpoint.ps1 restore 3       - restore version 3 ("3", "v3" or "v003"); any other text = newest whose name contains it
#                                    (saves a "before-restore" checkpoint first, so a restore can be undone)
# Covers: Scripts, Prefabs, Configs, Common, Missions, Data, root .ent files, *_Layers, addon.gproj.
# Not covered (too big or not code): worlds, Assets, Terrain, docs, .remember, mud_backups.
# Stop Play in Workbench before a restore, then reload scripts.
param([Parameter(Mandatory)][ValidateSet('save', 'list', 'restore')][string]$Action, [string]$Name = 'manual')

$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$Store = 'C:\Users\Steffen\Documents\A_documents\Github\Terrain deformation\Versions backup'    # user 2026-10-11: future GitHub repo folder
$Dirs = @('Scripts', 'Prefabs', 'Configs', 'Common', 'Missions', 'Data')

function Get-Items {
	$items = @($Dirs | Where-Object { Test-Path (Join-Path $Root $_) })
	$items += Get-ChildItem $Root -Directory -Filter '*_Layers' | ForEach-Object Name
	$items += Get-ChildItem $Root -File | Where-Object { $_.Extension -in '.ent', '.et', '.meta', '.gproj' } | ForEach-Object Name
	return $items
}

function Save-Checkpoint([string]$label) {
	$safe = ($label -replace '[^\w\-]', '-')
	$last = 0
	if (Test-Path $Store) {
		Get-ChildItem $Store -Directory | Where-Object Name -match '^v(\d+)_' | ForEach-Object { $last = [Math]::Max($last, [int]$Matches[1]) }
	}
	$ver = 'v{0:D3}' -f ($last + 1)
	$dest = Join-Path $Store ($ver + '_' + (Get-Date -Format 'yyyy-MM-dd_HHmm') + '_' + $safe)
	New-Item -ItemType Directory -Force $dest | Out-Null
	foreach ($i in Get-Items) {
		$src = Join-Path $Root $i
		if (Test-Path $src -PathType Container) {
			robocopy $src (Join-Path $dest $i) /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
			if ($LASTEXITCODE -ge 8) { throw "robocopy failed on $i ($LASTEXITCODE)" }
		} else {
			Copy-Item $src $dest
		}
	}
	"Saved $ver : $dest"
}

switch ($Action) {
	'save' { Save-Checkpoint $Name }
	'list' {
		if (Test-Path $Store) { Get-ChildItem $Store -Directory | Sort-Object Name | ForEach-Object Name } else { 'No checkpoints yet.' }
	}
	'restore' {
		if ($Name -match '^v?(\d+)$') {
			$want = 'v{0:D3}_' -f [int]$Matches[1]
			$cp = Get-ChildItem $Store -Directory | Where-Object { $_.Name.StartsWith($want) } | Select-Object -First 1
		} else {
			$cp = Get-ChildItem $Store -Directory | Where-Object Name -like "*$Name*" | Sort-Object Name | Select-Object -Last 1
		}
		if (!$cp) { throw "No checkpoint matching '$Name'" }
		Save-Checkpoint 'before-restore'
		foreach ($d in Get-ChildItem $cp.FullName -Directory) {
			# /MIR: files added after the checkpoint are removed too, so new scripts don't keep compiling
			robocopy $d.FullName (Join-Path $Root $d.Name) /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
			if ($LASTEXITCODE -ge 8) { throw "robocopy failed on $($d.Name) ($LASTEXITCODE)" }
		}
		Get-ChildItem $cp.FullName -File | Copy-Item -Destination $Root -Force
		"Restored: $($cp.Name)"
	}
}
exit 0    # robocopy leaves 1 (= files copied) in $LASTEXITCODE; real failures threw above
