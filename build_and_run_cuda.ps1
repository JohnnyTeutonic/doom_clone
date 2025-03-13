# Build and run script for DOOM Clone with CUDA support

# Check if CUDA is installed
Write-Host "Checking for CUDA installation..."
$nvccExists = Get-Command nvcc -ErrorAction SilentlyContinue

if ($null -eq $nvccExists) {
    Write-Host "CUDA compiler (nvcc) not found in PATH. Please make sure CUDA toolkit is installed and in your PATH."
    exit 1
} else {
    Write-Host "CUDA compiler (nvcc) found:"
    nvcc --version
}

# Create build directory if it doesn't exist
if (!(Test-Path -Path "build")) {
    New-Item -ItemType Directory -Path "build"
}

# Navigate to build directory
Set-Location -Path "build"

# Configure with CMake, explicitly enabling CUDA
Write-Host "Configuring with CMake (CUDA enabled)..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=ON

# Build
Write-Host "Building..."
cmake --build . --config Release

# Check if build was successful
if ($LastExitCode -eq 0) {
    Write-Host "Build successful! Running game..."
    
    # Navigate to bin directory
    Set-Location -Path "bin"
    
    # Run the game
    .\doom_clone.exe
    
    # Return to original directory
    Set-Location -Path "../.."
} else {
    Write-Host "Build failed with exit code $LastExitCode"
    
    # Return to original directory
    Set-Location -Path ".."
} 