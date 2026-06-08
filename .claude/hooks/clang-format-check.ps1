#Requires -Version 5.1
# Hook: PostToolUse Write|Edit
# Runs clang-format --dry-run --Werror on any .hxx/.cxx file Claude writes or edits.
# Exits non-zero (and prints violations) so Claude knows to fix formatting before continuing.

$clangFormat = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe'

$j = [Console]::In.ReadToEnd() | ConvertFrom-Json
$f = $j.tool_input.file_path

if (-not $f -or $f -notmatch '\.(hxx|cxx)$') { exit 0 }
if (-not (Test-Path $f)) { exit 0 }
if (-not (Test-Path $clangFormat)) { exit 0 }

& $clangFormat --dry-run --Werror $f
exit $LASTEXITCODE
