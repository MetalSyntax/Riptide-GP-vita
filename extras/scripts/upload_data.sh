#!/bin/bash
# Upload the staged game data (ux0_data/riptidegp/) to ux0:data/riptidegp/ over
# FTP (VitaShell, port 1337). Usage: extras/scripts/upload_data.sh <vita-ip> [--no-assets]
#   --no-assets  skip assets/Base.apf (37 MB) when only the .so files changed.
set -e

IP="$1"
[ -z "$IP" ] && { echo "usage: $0 <vita-ip> [--no-assets]"; exit 1; }

SRC="$(cd "$(dirname "$0")/../.." && pwd)/ux0_data/riptidegp"
DST="ftp://$IP:1337/ux0:/data/riptidegp"

cd "$SRC"
find . -type f ! -name '._*' ! -name '.gitkeep' | sed 's|^\./||' | while read -r f; do
    if [ "$2" = "--no-assets" ] && [ "${f#assets/}" != "$f" ]; then
        continue
    fi
    echo "-> $f"
    curl -s --ftp-create-dirs -T "$f" "$DST/$f"
done

# logs/ and saves/ must exist even when empty (PORTING_PLAN.md s.6).
for d in logs saves files obb; do
    curl -s --ftp-create-dirs -Q "-MKD /ux0:/data/riptidegp/$d" "$DST/" -o /dev/null || true
done
echo "done."
