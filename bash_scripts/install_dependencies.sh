#!/bin/bash

# install_dependencies.sh - Installs nlohmann_json based on detected OS

set -e  # Exit on error

echo "=========================================="
echo "nlohmann_json Installation Script"
echo "=========================================="
echo ""

# Detect OS
if [ -f /etc/debian_version ]; then
    OS="debian"
    OS_NAME="Ubuntu/Debian"
elif [ -f /etc/fedora-release ] || [ -f /etc/redhat-release ]; then
    OS="fedora"
    OS_NAME="Fedora/RHEL"
elif [ -f /etc/arch-release ]; then
    OS="arch"
    OS_NAME="Arch Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    OS_NAME="macOS"
else
    OS="unknown"
    OS_NAME="Unknown"
fi

echo "Detected OS: $OS_NAME"
echo ""

# Check if running with proper privileges
if [ "$OS" != "macos" ] && [ "$EUID" -ne 0 ] && [ "$OS" != "unknown" ]; then
    echo "This script requires sudo privileges to install packages."
    echo "You may be prompted for your password."
    echo ""
fi

# Install based on OS
case $OS in
    debian)
        echo "Installing nlohmann_json on Ubuntu/Debian..."
        sudo apt update
        sudo apt install -y nlohmann-json3-dev
        echo "✓ Installation complete!"
        ;;
    
    fedora)
        echo "Installing nlohmann_json on Fedora/RHEL..."
        sudo dnf install -y json-devel
        echo "✓ Installation complete!"
        ;;
    
    arch)
        echo "Installing nlohmann_json on Arch Linux..."
        sudo pacman -S --noconfirm nlohmann-json
        echo "✓ Installation complete!"
        ;;
    
    macos)
        echo "Installing nlohmann_json on macOS..."
        if ! command -v brew &> /dev/null; then
            echo "Error: Homebrew is not installed."
            echo "Please install Homebrew first: https://brew.sh"
            exit 1
        fi
        brew install nlohmann-json
        echo "✓ Installation complete!"
        ;;
    
    unknown)
        echo "Error: Could not detect your operating system."
        echo ""
        echo "Please install nlohmann_json manually:"
        echo "  Ubuntu/Debian: sudo apt update && sudo apt install nlohmann-json3-dev"
        echo "  Fedora/RHEL:   sudo dnf install json-devel"
        echo "  Arch Linux:    sudo pacman -S nlohmann-json"
        echo "  macOS:         brew install nlohmann-json"
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "You can now run cmake again to build the project."
echo "=========================================="
