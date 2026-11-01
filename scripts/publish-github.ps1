param([string]$GitHubCli='gh',[string]$Repository='edge-gateway-h750')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    & $GitHubCli auth status
    if($LASTEXITCODE -ne 0){throw 'Please complete gh auth login --hostname github.com --git-protocol https --web first. Do not put passwords or tokens in this script.'}
    $owner=(& $GitHubCli api user --jq .login).Trim()
    if($LASTEXITCODE -ne 0 -or $owner -notmatch '^[A-Za-z0-9-]+$'){throw 'Could not verify the authenticated GitHub owner'}
    if(git status --porcelain){throw 'Commit the reviewed source before publishing'}
    $remote=git remote get-url origin 2>$null
    if($LASTEXITCODE -eq 0){
        if($remote -ne "https://github.com/$owner/$Repository.git"){throw 'Existing remote differs from the intended authenticated account; inspect it first'}
        git push -u origin HEAD
        if($LASTEXITCODE -ne 0){throw 'Push failed'}
    } else {
        & $GitHubCli repo create "$owner/$Repository" --public --description 'STM32H750 Polaris RGB800x480 FreeRTOS/LVGL Wi-Fi Modbus gateway port' --source $root --remote origin --push
        if($LASTEXITCODE -ne 0){throw 'Repository creation/push failed; no existing repository was overwritten'}
    }
    & $GitHubCli repo view "$owner/$Repository" --json url,defaultBranchRef --jq '{url:.url,branch:.defaultBranchRef.name}'
    if($LASTEXITCODE -ne 0){throw 'Remote repository verification failed'}
} finally {Pop-Location}
