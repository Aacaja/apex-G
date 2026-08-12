#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build/core
clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 \
  tests/test_arc_core.cpp -o build/core/test_arc_core
./build/core/test_arc_core
clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 \
  tests/baseline_test.cpp -o build/core/baseline_test
./build/core/baseline_test

forbidden='OpenProcess|ReadProcessMemory|WriteProcessMemory|VirtualAllocEx|CreateRemoteThread|SetWindowsHookEx|SendInput|keybd_event|mouse_event|SetCursorPos|PostMessage|SendMessage|FindWindow|GetWindowThreadProcessId|CreateToolhelp32Snapshot|EnumProcesses|Windows[.]Media[.]Ocr|D3D11|Direct3D|DXGI|Windows[.]Graphics[.]Capture|MediaFoundation|MFCreate|WSAStartup|WinHttp|InternetOpen|obs-websocket'
if rg -n -i "$forbidden" src resources; then
  echo "FORBIDDEN_API_SCAN_FAIL"
  exit 1
fi
echo "FORBIDDEN_API_SCAN_OK runtime_source=src,resources"
