#!/usr/bin/env bash
# Run in MSYS2 UCRT64. Build exactly one backend and one dashboard.
set -euo pipefail
cd "$(dirname "$0")"
if [[ ${MSYSTEM:-} != UCRT64 ]]; then echo "Open MSYS2 UCRT64 first."; exit 1; fi
if ! pkg-config --exists opencv5; then echo "Install the UCRT64 OpenCV 5 and pkgconf packages (see README)."; exit 1; fi
mkdir -p build
stage=$(mktemp -d build/package.XXXXXX)
trap 'rm -rf -- "$stage"' EXIT
app="$stage/WorkstationMonitor"
mkdir -p "$app/licenses"
g++ -std=c++17 -O2 -pthread Shreyan/{main,Score,Alerts,Files}.cpp KeyboardMouse/{Keyboard,Mouse}.cpp \
    $(pkg-config --cflags --libs opencv5) -luser32 -o "$app/monitor.exe"
g++ -std=c++17 -O2 -mwindows Shreyan/display.cpp -lgdi32 -luser32 -lshell32 -o "$app/Workstation.exe"
cp UTEK2026/{face_mesh_Nx3x192x192.onnx,haarcascade_frontalface_default.xml,person_detection_mediapipe_2023mar.onnx,pose_estimation_mediapipe_2023mar.onnx} "$app/"
cp README.md "$app/"
if [[ -f LICENSE ]]; then cp LICENSE "$app/licenses/project.txt"; fi
cp Shreyan/picojson.h "$app/licenses/"
# Follow imported DLLs recursively. Windows supplies its own system DLLs.
queue=("$app/Workstation.exe" "$app/monitor.exe")
system32=$(cygpath -S)
declare -A seen=() owners=()
for ((i=0; i<${#queue[@]}; i++)); do
    imports=$(objdump -p "${queue[i]}" | awk '/DLL Name:/ {print $3}' | tr -d '\r')
    while IFS= read -r dll; do
        [[ -n $dll ]] || continue
        key=${dll,,}
        [[ -z ${seen[$key]:-} ]] || continue
        seen[$key]=1
        src="/ucrt64/bin/$dll"
        if [[ -f $src ]]; then
            cp "$src" "$app/"
            queue+=("$app/$dll")
            owner=$(pacman -Qqo "$src")
            owners[$owner]=1
        elif [[ ! -f "$system32/$dll" && $key != api-ms-* && $key != ext-ms-* ]]; then
            echo "Missing DLL: $dll"; exit 1
        fi
    done <<< "$imports"
done
for owner in "${!owners[@]}"; do
    while IFS= read -r path; do
        if [[ $path == */share/licenses/* && -f $path ]]; then
            mkdir -p "$app/licenses/$owner"
            cp "$path" "$app/licenses/$owner/"
        fi
    done < <(pacman -Qlq "$owner")
done
export APP_DIR=$(cygpath -aw "$app") ZIP_OUT=$(cygpath -aw "$stage/WorkstationMonitor-Windows.zip")
powershell.exe -NoProfile -Command '$ErrorActionPreference="Stop"; Compress-Archive -LiteralPath $env:APP_DIR -DestinationPath $env:ZIP_OUT'
mv -f "$stage/WorkstationMonitor-Windows.zip" build/WorkstationMonitor-Windows.zip
echo "Ready: build/WorkstationMonitor-Windows.zip"
