# Waits for the checks of a pull request to finish, then beeps:
# rising tones when all checks passed, falling tones when any failed.
#
# Usage: tools/checks.ps1 [<pr number | url | branch>] [other gh pr checks options]
#
# Examples:
#   tools\checks.ps1                # the PR of the current branch
#   tools\checks.ps1 42             # PR #42
#   tools\checks.ps1 docs/plans-schema
#                                   # the PR of another branch
#   tools\checks.ps1 --fail-fast    # stop at the first failing check
#   tools\checks.ps1 --required     # only the required checks
#   tools\checks.ps1 -i 30          # refresh every 30 seconds (default 10)
#
# From Windows PowerShell with a restrictive execution policy:
#   powershell -ExecutionPolicy Bypass -File tools\checks.ps1

gh pr checks --watch @args
$code = $LASTEXITCODE

if ($code -eq 0) {
    600, 900, 1200 | ForEach-Object { [console]::Beep($_, 200) }
} else {
    1200, 900, 600 | ForEach-Object { [console]::Beep($_, 250) }
}

exit $code
