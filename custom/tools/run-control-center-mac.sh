#!/bin/bash
set -euo pipefail

# Locate the project from this script's location.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "$script_dir/../../../.." && pwd)"

app_executable="$project_dir/builds/control-center-mac-debug/Debug/YousifControlCenter.app/Contents/MacOS/YousifControlCenter"
log_file="$project_dir/notes/control-center-startup.log"

if [[ ! -x "$app_executable" ]]; then
    printf 'Application executable not found:\n%s\nBuild the application before running this script.\n' "$app_executable" >&2
    exit 1
fi

mkdir -p "$project_dir/notes"

# The current Profile Inspector reads bundled JSON using XMLHttpRequest.
# Enable local reads for this application process only.
QML_XHR_ALLOW_FILE_READ=1 "$app_executable" 2>&1 | tee "$log_file"
