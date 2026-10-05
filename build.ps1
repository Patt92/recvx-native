$ErrorActionPreference = 'Stop'
if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 "$PSScriptRoot/tools/build_native.py" @args
} else {
    & python "$PSScriptRoot/tools/build_native.py" @args
}
exit $LASTEXITCODE
