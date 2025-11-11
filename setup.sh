#!/bin/bash

# setup.sh - Setup script for GitHubUploader project

set -e  # Exit on error

echo "=========================================="
echo "GitHubUploader Setup Script"
echo "=========================================="
echo ""

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "Checking for required dependencies..."
echo ""

# Function to check if a package is installed
check_package() {
    if command -v pkg-config &> /dev/null; then
        pkg-config --exists "$1" 2>/dev/null
        return $?
    fi
    return 1
}

# Check for nlohmann_json
if check_package nlohmann_json; then
    echo "✓ nlohmann_json is already installed"
    NLOHMANN_INSTALLED=true
else
    echo "✗ nlohmann_json is NOT installed"
    NLOHMANN_INSTALLED=false
fi

echo ""

# If not installed, offer to install
if [ "$NLOHMANN_INSTALLED" = false ]; then
    echo "nlohmann_json is required but not installed."
    echo ""
    echo "Would you like to install it now? (y/n)"
    read -r response
    
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo ""
        bash "$SCRIPT_DIR/bash_scripts/install_dependencies.sh"
    else
        echo ""
        echo "Installation cancelled."
        echo "You can install manually later by running:"
        echo "  bash $SCRIPT_DIR/bash_scripts/install_dependencies.sh"
        exit 1
    fi
fi

echo ""
echo "=========================================="
echo "All dependencies are installed!"
echo "=========================================="
echo ""

# --- Create build directory if it doesn't exist ---
if [ ! -d "$SCRIPT_DIR/build" ]; then
    mkdir -p "$SCRIPT_DIR/build"
fi

# --- Create data directory if it doesn't exist ---
DATA_DIR="$SCRIPT_DIR/data"
if [ ! -d "$DATA_DIR" ]; then
    mkdir -p "$DATA_DIR"
    echo "Created data directory: $DATA_DIR"
fi

# --- Create default exclude_patterns.json if missing ---
EXCLUDE_JSON="$DATA_DIR/exclude_patterns.json"
if [ ! -f "$EXCLUDE_JSON" ]; then
    cat > "$EXCLUDE_JSON" <<EOL
{
    "files": [
        "githubtoken.dat",
        ".env",
        ".gitignore"
    ],
    "dirs": [
        ".git",
        "build",
        "dist"
    ],
    "patterns": [
        "secret",
        ".token",
        ".key"
    ]
}
EOL
    echo "Created default exclude_patterns.json in $DATA_DIR"
else
    echo "exclude_patterns.json already exists in $DATA_DIR"
fi

echo ""
echo "=========================================="
echo "Building the project..."
echo ""

cd "$SCRIPT_DIR/build"

# Run CMake
echo "Running CMake..."
cmake ..

echo ""
echo "Building with make..."
make

echo ""
echo "=========================================="
echo "✓ Build complete!"
echo "=========================================="
echo ""
echo "To run the application:"
echo "  cd build"
echo "  ./dist/GitHubUploader"
echo ""
