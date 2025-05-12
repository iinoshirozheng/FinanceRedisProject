#!/bin/zsh

# Exit on error
set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Configuration
BUILD_DIR="build"
CONFIG_FILES=("connection.json" "area_branch.json")
REDIS_PORT=6379
RUN_TESTS=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        --test|-t)
            RUN_TESTS=true
            shift
            ;;
        *)
            # Unknown option
            shift
            ;;
    esac
done

# If running tests only, use the simplified test build
if $RUN_TESTS; then
    print_status "Building and running tests only..."
    
    # Build and run tests
    cd "$SCRIPT_DIR/tests"
    mkdir -p build
    cd build
    cmake .. -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
    make -j$(sysctl -n hw.ncpu)
    
    if [ $? -eq 0 ]; then
        print_status "Tests built successfully. Running tests..."
        ./basic_tests
        exit $?
    else
        print_error "Test build failed!"
        exit 1
    fi
fi

check_redis() {
    if ! nc -z localhost $REDIS_PORT &>/dev/null; then
        print_warning "Redis is not running on port $REDIS_PORT"
        print_warning "You can install Redis with: brew install redis"
        print_warning "And start it with: brew services start redis"
        return 1
    fi
    return 0
}

check_config_files() {
    local missing_files=()
    for file in "${CONFIG_FILES[@]}"; do
        if [[ ! -f "$file" ]]; then
            missing_files+=("$file")
        fi
    done
    
    if [[ ${#missing_files[@]} -gt 0 ]]; then
        print_warning "Missing configuration files: ${missing_files[*]}"
        print_warning "Please ensure these files exist in the root directory"
        return 1
    fi
    return 0
}

# Main build process
print_status "Building project..."

# Clean build directory
rm -rf "$BUILD_DIR"/* 2>/dev/null || true

# Configure and build
cmake -S . -B "$BUILD_DIR" -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
cd "$BUILD_DIR"
make -j$(sysctl -n hw.ncpu)

if [ $? -eq 0 ]; then
    print_status "Build successful!"
    
    # Run tests if requested
    if $RUN_TESTS; then
        print_status "Running tests using integrated test framework..."
        if [ -f "./run_tests" ]; then
            ./run_tests
            print_status "Tests completed."
            exit 0
        else
            print_warning "Integrated test executable not found, skipping tests."
        fi
    fi
    
    # Check dependencies
    check_redis
    check_config_files
    
    echo ""
    print_status "The application requires the following dependencies to run correctly:"
    echo "  - Redis server (default port $REDIS_PORT)"
    echo "  - Configuration files (${CONFIG_FILES[*]})"
    echo ""
    
    echo "Do you want to run the application now? (y/n)"
    read -r answer
    if [[ "$answer" =~ ^[Yy]$ ]]; then
        print_status "Running CACB..."
        cd "$SCRIPT_DIR"
        ./"$BUILD_DIR"/CACB "$@"
    else
        print_status "Exiting without running the application."
    fi
else
    print_error "Build failed!"
    exit 1
fi 