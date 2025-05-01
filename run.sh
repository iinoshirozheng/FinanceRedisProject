#!/bin/zsh

# This script builds and runs the Finance Manager project

# Make sure the build is up-to-date
cd "$(dirname "$0")"
echo "Building project..."
rm -rf build/* 2>/dev/null
cmake -S . -B build
cd build
make -j$(sysctl -n hw.ncpu)

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo ""
    echo "NOTE: The application requires the following dependencies to run correctly:"
    echo "  - Redis server (default port 6379)"
    echo "  - Configuration files (connection.json and area_branch.json in the root directory)"
    echo ""
    echo "If you don't have Redis installed, you can install it with:"
    echo "  brew install redis"
    echo "And start it with:"
    echo "  brew services start redis"
    echo ""
    
    echo "Do you want to run the application now? (y/n)"
    read answer
    if [[ "$answer" =~ ^[Yy] ]]; then
        echo "Running finance_manager..."
        cd ..
        ./build/src/finance_manager "$@"
    else
        echo "Exiting without running the application."
    fi
else
    echo "Build failed!"
    exit 1
fi 