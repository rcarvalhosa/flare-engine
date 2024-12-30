#!/usr/bin/env bash
set -e  # Exit on any error

cd "`dirname "$0"`"
cd ../../

FLARE_EXE=$1

if [ -z "${FLARE_EXE}" ]; then
    echo "Usage: $0 <path to flare executable>"
    exit 1
fi

# Get the absolute path to directories
ENGINE_DIR=$(pwd)
GAME_DIR="../flare-game"

DST="${ENGINE_DIR}/Flare.app"

# Remove existing app bundle
echo "Cleaning up old bundle..."
rm -rf "${DST}"

# Create bundle structure
echo "Creating app bundle..."
mkdir -p "${DST}/Contents/MacOS"
mkdir -p "${DST}/Contents/Frameworks"

# Copy executable
echo "Copying executable..."
cp "${FLARE_EXE}" "${DST}/Contents/MacOS/flare.orig"
chmod 755 "${DST}/Contents/MacOS/flare.orig"

# Copy mods directory next to executable
echo "Creating mods directory..."
mkdir -p "${DST}/Contents/MacOS/mods"
MODS_DST="${DST}/Contents/MacOS/mods"

# Copy default mod from engine first
echo "Copying default mod..."
cp -R "${ENGINE_DIR}/mods/default" "${MODS_DST}/"

# Copy game mods
echo "Copying game mods..."
cp -R "${GAME_DIR}/mods/fantasycore" "${MODS_DST}/"
cp -R "${GAME_DIR}/mods/empyrean_campaign" "${MODS_DST}/"

# Create launcher script
echo "Creating launcher script..."
cat > "${DST}/Contents/MacOS/flare" << 'EOF'
#!/bin/bash
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR"  # Change to executable directory
exec "./flare.orig"
EOF

chmod 755 "${DST}/Contents/MacOS/flare"

# Copy and fix library dependencies
echo "Fixing library dependencies..."
if ! command -v dylibbundler &> /dev/null; then
    echo "Error: dylibbundler not found. Please install with: brew install dylibbundler"
    exit 1
fi

dylibbundler -cd -b -x "${DST}/Contents/MacOS/flare.orig" \
    -d "${DST}/Contents/Frameworks/" \
    -p @executable_path/../Frameworks/

echo "App bundle created successfully at ${DST}"
