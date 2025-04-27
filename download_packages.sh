#!/bin/zsh

# Exit on error
set -e

# Create lib directory if it doesn't exist
mkdir -p lib
cd lib

# Link to Homebrew's hiredis installation
link_hiredis() {
    if [ ! -d "hiredis" ]; then
        echo "Creating symbolic link to Homebrew's hiredis..."
        
        # Check if Homebrew is installed
        if ! command -v brew &> /dev/null; then
            echo "Homebrew is not installed. Skipping hiredis linking."
            return 1
        fi
        
        # Find hiredis installation directory
        HIREDIS_DIR=$(brew --prefix hiredis 2>/dev/null || echo "")
        
        if [ -z "$HIREDIS_DIR" ] || [ ! -d "$HIREDIS_DIR" ]; then
            echo "hiredis is not installed via Homebrew. Run 'brew install hiredis' to install it."
            return 1
        fi
        
        # Create symbolic link
        ln -sf "$HIREDIS_DIR" hiredis
        echo "Symbolic link to hiredis created successfully at $(pwd)/hiredis -> $HIREDIS_DIR"
    else
        echo "hiredis link already exists, skipping..."
    fi
}

# Download nlohmann json
clone_nlohmann_json() {
    if [ ! -d "nlohmann_json" ]; then
        echo "Cloning nlohmann/json..."
        git clone https://github.com/nlohmann/json.git nlohmann_json
    else
        echo "nlohmann/json already exists, skipping..."
    fi
}

# Download loguru
clone_loguru() {
    if [ ! -d "loguru" ]; then
        echo "Cloning loguru..."
        git clone https://github.com/emilk/loguru.git
    else
        echo "loguru already exists, skipping..."
    fi
}

# Link to Homebrew's Poco installation
link_poco() {
    if [ ! -d "poco" ]; then
        echo "Creating symbolic link to Homebrew's Poco..."
        
        # Check if Homebrew is installed
        if ! command -v brew &> /dev/null; then
            echo "Homebrew is not installed. Skipping Poco linking."
            return 1
        fi
        
        # Find Poco installation directory
        POCO_DIR=$(brew --prefix poco 2>/dev/null || echo "")
        
        if [ -z "$POCO_DIR" ] || [ ! -d "$POCO_DIR" ]; then
            echo "Poco is not installed via Homebrew. Run 'brew install poco' to install it."
            return 1
        fi
        
        # Create symbolic link
        ln -sf "$POCO_DIR" poco
        echo "Symbolic link to Poco created successfully at $(pwd)/poco -> $POCO_DIR"
    else
        echo "Poco link already exists, skipping..."
    fi
}

# Link to Homebrew's Boost installation
link_boost() {
    if [ ! -d "boost" ]; then
        echo "Creating symbolic link to Homebrew's Boost..."
        
        # Check if Homebrew is installed
        if ! command -v brew &> /dev/null; then
            echo "Homebrew is not installed. Skipping Boost linking."
            return 1
        fi
        
        # Find Boost installation directory
        BOOST_DIR=$(brew --prefix boost 2>/dev/null || echo "")
        
        if [ -z "$BOOST_DIR" ] || [ ! -d "$BOOST_DIR" ]; then
            echo "Boost is not installed via Homebrew. Run 'brew install boost' to install it."
            return 1
        fi
        
        # Create symbolic link
        ln -sf "$BOOST_DIR" boost
        echo "Symbolic link to Boost created successfully at $(pwd)/boost -> $BOOST_DIR"
    else
        echo "Boost link already exists, skipping..."
    fi
}

# Main function
main() {
    # Link and clone dependencies
    link_hiredis
    clone_nlohmann_json
    clone_loguru
    link_poco
    link_boost
    
    echo "All dependencies successfully linked or downloaded in lib directory!"
}

# Execute main function
main 