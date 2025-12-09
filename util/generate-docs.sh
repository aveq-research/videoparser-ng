#!/usr/bin/env bash
#
# Generate API documentation for VideoParser C++ library using Doxygen
#
# This script generates HTML documentation from the C++ source code and
# updates the version number in the Doxygen configuration.

set -e

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Parse version numbers from VideoParser.h
VERSION_FILE="VideoParser/VideoParser.h"
if [ ! -f "$VERSION_FILE" ]; then
    echo "Error: Version file $VERSION_FILE not found"
    exit 1
fi

# Extract version numbers
MAJOR=$(grep "VIDEOPARSER_VERSION_MAJOR" "$VERSION_FILE" | head -1 | awk '{print $3}')
MINOR=$(grep "VIDEOPARSER_VERSION_MINOR" "$VERSION_FILE" | head -1 | awk '{print $3}')
PATCH=$(grep "VIDEOPARSER_VERSION_PATCH" "$VERSION_FILE" | head -1 | awk '{print $3}')
VERSION="$MAJOR.$MINOR.$PATCH"

echo "Generating documentation for VideoParser v$VERSION"

# Check if doxygen is installed
if ! command -v doxygen &> /dev/null; then
    echo "Error: Doxygen is not installed."
    echo "Please install it using:"
    echo "  macOS:   brew install doxygen"
    echo "  Ubuntu:  sudo apt-get install doxygen"
    echo "  Other:   See https://www.doxygen.nl/download.html"
    exit 1
fi

# Optional: Check if graphviz/dot is installed for better diagrams
if ! command -v dot &> /dev/null; then
    echo "Warning: Graphviz (dot) is not installed. Diagrams will be limited."
    echo "Install graphviz for better documentation diagrams:"
    echo "  macOS:   brew install graphviz"
    echo "  Ubuntu:  sudo apt-get install graphviz"
fi

# Create temporary Doxyfile with updated version
TEMP_DOXYFILE="Doxyfile.temp"
cp Doxyfile "$TEMP_DOXYFILE"

# Update version number in Doxyfile
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    sed -i '' "s/@VERSION@/$VERSION/g" "$TEMP_DOXYFILE"
else
    # Linux
    sed -i "s/@VERSION@/$VERSION/g" "$TEMP_DOXYFILE"
fi

# Clean previous documentation if it exists
if [ -d "docs" ]; then
    echo "Cleaning previous documentation..."
    rm -rf docs
fi

# Generate documentation
echo "Running Doxygen..."
doxygen "$TEMP_DOXYFILE" 2>&1 | tee doxygen.log

# Check if documentation was generated successfully
if [ ! -d "docs/html" ]; then
    echo "Error: Documentation generation failed. Check doxygen.log for details."
    rm -f "$TEMP_DOXYFILE"
    exit 1
fi

# Clean up
rm -f "$TEMP_DOXYFILE"

# Count warnings/errors
WARNINGS=$(grep -c "warning:" doxygen.log 2>/dev/null || true)
ERRORS=$(grep -c "error:" doxygen.log 2>/dev/null || true)

echo ""
echo "Documentation generated successfully!"
echo "Location: $PROJECT_ROOT/docs/html/index.html"
echo "Warnings: $WARNINGS"
echo "Errors: $ERRORS"

# Optional: Open documentation in browser (macOS only)
if [[ "$OSTYPE" == "darwin"* ]] && [ "$1" == "--open" ]; then
    echo "Opening documentation in browser..."
    open "docs/html/index.html"
fi

# Clean up log file if no errors/warnings
if [ "$WARNINGS" -eq 0 ] && [ "$ERRORS" -eq 0 ]; then
    rm -f doxygen.log
else
    echo ""
    echo "Check doxygen.log for warnings/errors"
fi

echo ""
echo "To view the documentation, open: file://$PROJECT_ROOT/docs/html/index.html"
