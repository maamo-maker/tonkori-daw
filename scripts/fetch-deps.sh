#!/usr/bin/env bash
# Clones the pinned engine + JUCE into libs/. Used by CI; you never need to run this by hand.
set -euo pipefail
cd "$(dirname "$0")/.."

TE_URL=$(awk '$1=="tracktion_engine"{print $2}' deps.lock)
TE_REV=$(awk '$1=="tracktion_engine"{print $3}' deps.lock)
JUCE_URL=$(awk '$1=="juce"{print $2}' deps.lock)
JUCE_REV=$(awk '$1=="juce"{print $3}' deps.lock)

mkdir -p libs

clone_at () {
    local url="$1" rev="$2" dir="$3"
    if [ -d "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD)" = "$rev" ]; then
        echo "$dir already at $rev"; return
    fi
    rm -rf "$dir"
    git init -q "$dir"
    git -C "$dir" remote add origin "$url"
    git -C "$dir" fetch -q --depth 1 origin "$rev"
    git -C "$dir" checkout -q FETCH_HEAD
}

clone_at "$TE_URL"   "$TE_REV"   libs/tracktion_engine
clone_at "$JUCE_URL" "$JUCE_REV" libs/tracktion_engine/modules/juce
echo "deps ready"
