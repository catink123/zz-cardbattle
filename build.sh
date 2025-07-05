#!/bin/bash

# Build script for zz-cardbattle project
# Builds both Server (C++ with CMake) and Client (Flutter)

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to build Server
build_server() {
    print_status "Building Server..."
    
    if ! command_exists cmake; then
        print_error "CMake is not installed. Please install CMake first."
        exit 1
    fi
    
    # Use CMake presets
    cmake --preset=debug
    cmake --build --preset=debug
    
    print_success "Server built successfully!"
}

# Function to build Client
build_client() {
    print_status "Building Client..."
    
    if ! command_exists flutter; then
        print_error "Flutter is not installed. Please install Flutter first."
        exit 1
    fi
    
    cd src/Client
    
    # Get Flutter dependencies
    print_status "Getting Flutter dependencies..."
    flutter pub get
    
    # Build for web (output to top-level build directory)
    print_status "Building Flutter app for web..."
    if flutter build web --output-dir ../../build/debug/web; then
        print_success "Flutter web build completed with --output-dir."
    else
        print_warning "--output-dir not supported, trying --output."
        flutter build web --output ../../build/debug/web
    fi
    
    cd ../..
    print_success "Client built successfully!"
}

run_dev_server() {
    print_status "Starting development server..."
    ./build/debug/src/Server/Server --config build/release/src/Server/static_config.yaml &
    SERVER_PID=$!
    print_status "Server running on: http://localhost:8080"
}

run_dev_client() {
    print_status "Starting development server..."
    cd src/Client
    flutter run -d web-server --web-port 3000 &
    FLUTTER_PID=$!
    cd ..
    print_status "Client running on: http://localhost:3000"
}

# Function to run development server
run_dev() {
    print_status "Starting development environment..."
    
    # Start the C++ server in background
    run_dev_server
    
    # Start Flutter development server
    run_dev_client
    
    print_success "Development environment started!"
    print_status "Press Ctrl+C to stop both servers"
    
    # Wait for interrupt
    trap "kill $SERVER_PID $FLUTTER_PID 2>/dev/null; print_status 'Servers stopped'; exit" INT
    wait
}

# Function to clean build artifacts
clean() {
    print_status "Cleaning build artifacts..."
    
    # Clean CMake build
    if [ -d "build" ]; then
        rm -rf build
        print_success "CMake build artifacts cleaned"
    fi
    
    # Clean Flutter build
    if [ -d "src/Client/build" ]; then
        cd src/Client
        flutter clean
        cd ../..
        print_success "Flutter build artifacts cleaned"
    fi
    
    # Clean Flutter web build output
    if [ -d "build/web" ]; then
        rm -rf build/web
        print_success "Flutter web build output cleaned"
    fi
    
    print_success "All build artifacts cleaned!"
}

run_only_server() {
    print_status "Running backend server only..."
    ./build/debug/src/Server/Server --config build/debug/src/Server/static_config.yaml
}

run_only_client() {
    print_status "Running Flutter client only..."
    cd src/Client
    flutter run -d web-server --web-port 3000
    cd ../..
}

# Function to show help
show_help() {
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  server        Build only the C++ Server"
    echo "  client        Build only the Flutter Client"
    echo "  all           Build both Server and Client (default)"
    echo "  dev           Start development environment (Server + Flutter dev server, both in background)"
    echo "  run-server    Run only the backend server (foreground)"
    echo "  run-client    Run only the Flutter client (foreground)"
    echo "  clean         Clean all build artifacts"
    echo "  help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                # Build both Server and Client"
    echo "  $0 server         # Build only Server"
    echo "  $0 client         # Build only Client"
    echo "  $0 dev            # Start development environment (both servers, background)"
    echo "  $0 run-server     # Run only backend server"
    echo "  $0 run-client     # Run only Flutter client"
}

# Main script logic
case "${1:-all}" in
    "server")
        build_server
        ;;
    "client")
        build_client
        ;;
    "all")
        build_server
        build_client
        print_success "Both Server and Client built successfully!"
        ;;
    "dev")
        run_dev
        ;;
    "run-server")
        run_only_server
        ;;
    "run-client")
        run_only_client
        ;;
    "clean")
        clean
        ;;
    "help"|"-h"|"--help")
        show_help
        ;;
    *)
        print_error "Unknown option: $1"
        show_help
        exit 1
        ;;
esac 