#!/usr/bin/env bash
# scripts/hdrcheck.sh — проверка самодостаточности заголовков (NFR-E12).
# Каждый заголовок компилируется отдельно как самостоятельная единица:
# это ловит отсутствующие #include и ошибки синтаксиса до сборки целей.
set -u
cd "$(dirname "$0")/.."
fail=0
for h in "$@"; do
    tmp="$(mktemp /tmp/hdrcheck_XXXXXX.cpp)"
    printf '#include "%s"\nint main() { return 0; }\n' "$h" > "$tmp"
    if ! g++ -std=c++20 -Wall -Wextra -Werror -I. -Iengine -Igame -fsyntax-only "$tmp" 2>/tmp/hdr_err.txt; then
        echo "FAIL: $h"; sed 's/^/    /' /tmp/hdr_err.txt | head -12; fail=1
    fi
    rm -f "$tmp"
done
exit $fail
