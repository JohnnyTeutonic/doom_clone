#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdio.h>
#include <math.h>

#include "cuda_renderer.h"

// CUDA kernel for raycasting
extern "C" __global__ void raycastKernel(
    uint32_t* frameBuffer,
    float* zBuffer,
    int screenWidth,
    int screenHeight,
    int* mapData,
    int mapWidth,
    int mapHeight,
    PlayerData* playerData,
    uint32_t* wallTextures,
    uint32_t* floorTextures,
    uint32_t* ceilingTextures,
    int textureWidth,
    int textureHeight
) {
    // Calculate pixel coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    // Check if pixel is within screen bounds
    if (x >= screenWidth || y >= screenHeight) return;
    
    // Get player data
    float posX = playerData->posX;
    float posY = playerData->posY;
    float dirX = playerData->dirX;
    float dirY = playerData->dirY;
    float planeX = playerData->planeX;
    float planeY = playerData->planeY;
    
    // Calculate ray position and direction
    float cameraX = 2.0f * x / static_cast<float>(screenWidth) - 1.0f;
    float rayDirX = dirX + planeX * cameraX;
    float rayDirY = dirY + planeY * cameraX;
    
    // Map position
    int mapX = static_cast<int>(posX);
    int mapY = static_cast<int>(posY);
    
    // Length of ray from current position to next x or y-side
    float sideDistX, sideDistY;
    
    // Length of ray from one x or y-side to next x or y-side
    float deltaDistX = (rayDirX == 0) ? 1e30f : fabsf(1.0f / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30f : fabsf(1.0f / rayDirY);
    
    // Direction to step in x or y direction (either +1 or -1)
    int stepX, stepY;
    
    // Calculate step and initial sideDist
    if (rayDirX < 0) {
        stepX = -1;
        sideDistX = (posX - mapX) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = (mapX + 1.0f - posX) * deltaDistX;
    }
    
    if (rayDirY < 0) {
        stepY = -1;
        sideDistY = (posY - mapY) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = (mapY + 1.0f - posY) * deltaDistY;
    }
    
    // Perform DDA
    int side; // 0 for x-side, 1 for y-side
    int hit = 0; // Was a wall hit?
    int cellValue = 0; // Value of the cell that was hit
    
    while (hit == 0) {
        // Jump to next map square
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
        }
        
        // Check if ray has hit a wall
        if (mapX < 0 || mapX >= mapWidth || mapY < 0 || mapY >= mapHeight) {
            break; // Ray is out of bounds
        }
        
        cellValue = mapData[mapY * mapWidth + mapX];
        if (cellValue > 0) {
            hit = 1; // Wall hit
        }
    }
    
    // Calculate distance projected on camera direction
    float perpWallDist;
    if (side == 0) {
        perpWallDist = (sideDistX - deltaDistX);
    } else {
        perpWallDist = (sideDistY - deltaDistY);
    }
    
    // Save z-buffer value
    zBuffer[y * screenWidth + x] = perpWallDist;
    
    // Calculate height of line to draw on screen
    int lineHeight = static_cast<int>(screenHeight / perpWallDist);
    
    // Calculate lowest and highest pixel to fill in current stripe
    int drawStart = -lineHeight / 2 + screenHeight / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + screenHeight / 2;
    if (drawEnd >= screenHeight) drawEnd = screenHeight - 1;
    
    // Texturing calculations
    int texNum = 0; // Default to texture 0
    
    // Extract the texture ID from the cell value
    // If the cell value is > 100, it's a wall with an encoded texture ID
    if (cellValue > 100) {
        // The texture ID is encoded as (textureId + 1) * 100 + cellType
        // So we divide by 100 and subtract 1 to get the texture ID
        texNum = (cellValue / 100) - 1;
        
        // Ensure texNum is valid (0-3)
        texNum = (texNum < 0) ? 0 : (texNum > 3) ? 3 : texNum;
    } else {
        // For regular cells, use the cell value - 1 as the texture index
        texNum = cellValue - 1;
        
        // Ensure texNum is valid (prevent negative indices)
        if (texNum < 0) texNum = 0;
    }
    
    // Calculate where exactly the wall was hit
    float wallX;
    if (side == 0) {
        wallX = posY + perpWallDist * rayDirY;
    } else {
        wallX = posX + perpWallDist * rayDirX;
    }
    wallX -= floorf(wallX);
    
    // X coordinate on the texture
    int texX = static_cast<int>(wallX * static_cast<float>(textureWidth));
    if (side == 0 && rayDirX > 0) texX = textureWidth - texX - 1;
    if (side == 1 && rayDirY < 0) texX = textureWidth - texX - 1;
    
    // Draw the wall
    for (int i = drawStart; i <= drawEnd; i++) {
        // Calculate y coordinate on the texture
        int texY = static_cast<int>((i - drawStart) * static_cast<float>(textureHeight) / lineHeight);
        
        // Ensure texture coordinates are within bounds
        texX = (texX < 0) ? 0 : (texX >= textureWidth) ? textureWidth - 1 : texX;
        texY = (texY < 0) ? 0 : (texY >= textureHeight) ? textureHeight - 1 : texY;
        
        // Get texture pixel - use texNum as the texture index
        uint32_t color = wallTextures[texNum * textureWidth * textureHeight + texY * textureWidth + texX];
        
        // Make color darker for y-sides
        if (side == 1) {
            // Extract RGB components
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            
            // Darken
            r = static_cast<uint8_t>(r * 0.7f);
            g = static_cast<uint8_t>(g * 0.7f);
            b = static_cast<uint8_t>(b * 0.7f);
            
            // Recombine
            color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
        
        // Draw pixel
        frameBuffer[i * screenWidth + x] = color;
    }
    
    // Calculate floor and ceiling textures
    if (drawEnd < screenHeight - 1) {
        // Floor casting
        float floorXWall, floorYWall;
        
        // 4 different wall directions possible
        if (side == 0 && rayDirX > 0) {
            floorXWall = mapX;
            floorYWall = mapY + wallX;
        } else if (side == 0 && rayDirX < 0) {
            floorXWall = mapX + 1.0f;
            floorYWall = mapY + wallX;
        } else if (side == 1 && rayDirY > 0) {
            floorXWall = mapX + wallX;
            floorYWall = mapY;
        } else {
            floorXWall = mapX + wallX;
            floorYWall = mapY + 1.0f;
        }
        
        // Draw floor and ceiling from drawEnd to bottom of screen
        for (int i = drawEnd + 1; i < screenHeight; i++) {
            // Current distance from camera to floor
            float currentDist = screenHeight / (2.0f * i - screenHeight);
            
            // Weight for interpolation between wall position and player position
            float weight = currentDist / perpWallDist;
            
            // Get floor coordinates
            float floorX = weight * floorXWall + (1.0f - weight) * posX;
            float floorY = weight * floorYWall + (1.0f - weight) * posY;
            
            // Get texture coordinates
            int floorTexX = static_cast<int>(floorX * textureWidth) % textureWidth;
            int floorTexY = static_cast<int>(floorY * textureHeight) % textureHeight;
            
            // Ensure texture coordinates are within bounds
            floorTexX = (floorTexX < 0) ? 0 : (floorTexX >= textureWidth) ? textureWidth - 1 : floorTexX;
            floorTexY = (floorTexY < 0) ? 0 : (floorTexY >= textureHeight) ? textureHeight - 1 : floorTexY;
            
            // Get floor texture pixel - use texture ID 1 (floor texture)
            uint32_t floorColor = floorTextures[floorTexY * textureWidth + floorTexX];
            
            // Get ceiling texture pixel - use texture ID 2 (ceiling texture)
            uint32_t ceilingColor = ceilingTextures[floorTexY * textureWidth + floorTexX];
            
            // Apply distance-based darkening for floor
            uint8_t fr = (floorColor >> 16) & 0xFF;
            uint8_t fg = (floorColor >> 8) & 0xFF;
            uint8_t fb = floorColor & 0xFF;
            
            float distFactor = fminf(1.0f, 5.0f / currentDist);
            fr = static_cast<uint8_t>(fr * distFactor);
            fg = static_cast<uint8_t>(fg * distFactor);
            fb = static_cast<uint8_t>(fb * distFactor);
            
            floorColor = (0xFF << 24) | (fr << 16) | (fg << 8) | fb;
            
            // Apply distance-based darkening for ceiling
            uint8_t cr = (ceilingColor >> 16) & 0xFF;
            uint8_t cg = (ceilingColor >> 8) & 0xFF;
            uint8_t cb = ceilingColor & 0xFF;
            
            cr = static_cast<uint8_t>(cr * distFactor);
            cg = static_cast<uint8_t>(cg * distFactor);
            cb = static_cast<uint8_t>(cb * distFactor);
            
            ceilingColor = (0xFF << 24) | (cr << 16) | (cg << 8) | cb;
            
            // Draw floor pixel
            frameBuffer[i * screenWidth + x] = floorColor;
            
            // Draw ceiling pixel (symmetrical)
            int ceilingY = screenHeight - i - 1;
            if (ceilingY >= 0 && ceilingY < drawStart) {
                frameBuffer[ceilingY * screenWidth + x] = ceilingColor;
            }
        }
    }
    
    // Draw ceiling from top of screen to drawStart
    for (int i = 0; i < drawStart; i++) {
        if (i >= screenHeight - drawEnd - 1) continue; // Skip if already drawn by floor casting
        
        // Current distance from camera to ceiling
        float currentDist = screenHeight / (screenHeight - 2.0f * i);
        
        // Direction of ray for this pixel
        float rayDirXCeiling = dirX - planeX * (2.0f * x / static_cast<float>(screenWidth) - 1.0f);
        float rayDirYCeiling = dirY - planeY * (2.0f * x / static_cast<float>(screenWidth) - 1.0f);
        
        // Calculate ceiling position
        float ceilingX = posX + rayDirXCeiling * currentDist;
        float ceilingY = posY + rayDirYCeiling * currentDist;
        
        // Get texture coordinates
        int ceilingTexX = static_cast<int>(ceilingX * textureWidth) % textureWidth;
        int ceilingTexY = static_cast<int>(ceilingY * textureHeight) % textureHeight;
        
        // Ensure texture coordinates are within bounds
        ceilingTexX = (ceilingTexX < 0) ? 0 : (ceilingTexX >= textureWidth) ? textureWidth - 1 : ceilingTexX;
        ceilingTexY = (ceilingTexY < 0) ? 0 : (ceilingTexY >= textureHeight) ? textureHeight - 1 : ceilingTexY;
        
        // Get ceiling texture pixel - use texture ID 2 (ceiling texture)
        uint32_t ceilingColor = ceilingTextures[ceilingTexY * textureWidth + ceilingTexX];
        
        // Apply distance-based darkening
        uint8_t r = (ceilingColor >> 16) & 0xFF;
        uint8_t g = (ceilingColor >> 8) & 0xFF;
        uint8_t b = ceilingColor & 0xFF;
        
        float distFactor = fminf(1.0f, 5.0f / currentDist);
        r = static_cast<uint8_t>(r * distFactor);
        g = static_cast<uint8_t>(g * distFactor);
        b = static_cast<uint8_t>(b * distFactor);
        
        ceilingColor = (0xFF << 24) | (r << 16) | (g << 8) | b;
        
        // Draw ceiling pixel
        frameBuffer[i * screenWidth + x] = ceilingColor;
    }
}

// Wrapper function to launch the CUDA kernel
extern "C" void launchRaycastKernel(
    dim3 gridSize,
    dim3 blockSize,
    uint32_t* frameBuffer,
    float* zBuffer,
    int screenWidth,
    int screenHeight,
    int* mapData,
    int mapWidth,
    int mapHeight,
    PlayerData* playerData,
    uint32_t* wallTextures,
    uint32_t* floorTextures,
    uint32_t* ceilingTextures,
    int textureWidth,
    int textureHeight
) {
    raycastKernel<<<gridSize, blockSize>>>(
        frameBuffer,
        zBuffer,
        screenWidth,
        screenHeight,
        mapData,
        mapWidth,
        mapHeight,
        playerData,
        wallTextures,
        floorTextures,
        ceilingTextures,
        textureWidth,
        textureHeight
    );
}

// Helper function to check CUDA errors
void checkCudaError(cudaError_t error, const char* message) {
    if (error != cudaSuccess) {
        fprintf(stderr, "CUDA error: %s: %s\n", message, cudaGetErrorString(error));
        exit(-1);
    }
} 