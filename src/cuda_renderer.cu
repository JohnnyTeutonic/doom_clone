#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdio.h>
#include <math.h>

#include "cuda_renderer.h"

// Helper functions for CUDA lighting calculations
__device__ float3 calculatePointLight(
    const CudaLight& light,
    float3 normal,
    float2 position,
    float distance,
    float ambientR, float ambientG, float ambientB
) {
    float2 lightDir;
    lightDir.x = light.posX - position.x;
    lightDir.y = light.posY - position.y;
    
    // Normalize light direction
    float lightDist = sqrtf(lightDir.x * lightDir.x + lightDir.y * lightDir.y);
    
    // Skip if point is outside light radius
    if (lightDist > light.radius) {
        return make_float3(ambientR, ambientG, ambientB);
    }
    
    // Normalize direction vector
    if (lightDist > 0.0001f) {
        lightDir.x /= lightDist;
        lightDir.y /= lightDist;
    }
    
    // Calculate diffuse factor (dot product of normal and light direction)
    float diffuse = normal.x * lightDir.x + normal.y * lightDir.y;
    diffuse = fmaxf(0.0f, diffuse);
    
    // Calculate attenuation (falloff with distance)
    float attenuation = fmaxf(0.0f, 1.0f - (lightDist / light.radius));
    attenuation = attenuation * attenuation; // Squared falloff for more realistic effect
    
    // Calculate the final light contribution
    float3 result;
    result.x = light.r * light.intensity * diffuse * attenuation;
    result.y = light.g * light.intensity * diffuse * attenuation;
    result.z = light.b * light.intensity * diffuse * attenuation;
    
    return result;
}

__device__ float3 calculateDirectionalLight(
    const CudaLight& light,
    float3 normal
) {
    // Calculate diffuse factor (dot product of normal and negative light direction)
    float diffuse = -(normal.x * light.dirX + normal.y * light.dirY);
    diffuse = fmaxf(0.0f, diffuse);
    
    // Calculate the final light contribution
    float3 result;
    result.x = light.r * light.intensity * diffuse;
    result.y = light.g * light.intensity * diffuse;
    result.z = light.b * light.intensity * diffuse;
    
    return result;
}

__device__ float3 calculateLighting(
    float2 position,
    float3 normal,
    float distance,
    const float2 playerPos,
    const CudaLight* lights,
    int numActiveLights,
    const CudaAmbientLight& ambient
) {
    // Start with ambient light
    float3 totalLight;
    totalLight.x = ambient.r * ambient.intensity;
    totalLight.y = ambient.g * ambient.intensity;
    totalLight.z = ambient.b * ambient.intensity;
    
    // Apply all light sources - make sure we don't exceed array bounds
    for (int i = 0; i < numActiveLights; i++) {
        const CudaLight& light = lights[i];
        
        // Skip disabled lights
        if (light.enabled == 0) continue;

        // Calculate light contribution based on type
        float3 lightColor;
        
        switch (light.type) {
            case 0: // Point
            case 2: // Flickering
            case 3: // Pulsing  
            case 4: // Strobe
            case 5: // Glow
                // All point-like lights use the point light calculation
                lightColor = calculatePointLight(light, normal, position, distance, 
                                               totalLight.x, totalLight.y, totalLight.z);
                break;
                
            case 1: // Directional
                // Directional light calculation
                lightColor = calculateDirectionalLight(light, normal);
                break;
                
            default:
                // Skip unknown light types
                continue;
        }
        
        // Add this light's contribution
        totalLight.x += lightColor.x;
        totalLight.y += lightColor.y;
        totalLight.z += lightColor.z;
    }
    
    // Apply distance attenuation
    float distFactor = fminf(1.0f, 8.0f / distance);
    
    // Ensure minimum brightness
    totalLight.x = fmaxf(0.2f, totalLight.x * distFactor);
    totalLight.y = fmaxf(0.2f, totalLight.y * distFactor);
    totalLight.z = fmaxf(0.2f, totalLight.z * distFactor);
    
    // Ensure maximum brightness
    totalLight.x = fminf(1.0f, totalLight.x);
    totalLight.y = fminf(1.0f, totalLight.y);
    totalLight.z = fminf(1.0f, totalLight.z);
    
    return totalLight;
}

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
    int textureHeight,
    CudaLight* lights,
    int numLights,  // This is the actual count of active lights
    CudaAmbientLight* ambient
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
    float verticalAngle = playerData->verticalAngle;  // Look up/down angle
    float jumpHeight = playerData->jumpHeight;        // Current jump height
    
    // Create player position for lighting
    float2 playerPos = make_float2(posX, posY);
    
    // Calculate vertical offsets for rendering
    float lookOffset = verticalAngle * screenHeight / 2.0f;  // Scale to screen space
    float jumpOffset = jumpHeight * screenHeight * 0.75f;    // Scale jump height appropriately
    float totalVerticalOffset = lookOffset + jumpOffset;     // Combine both effects
    
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
    
    // Debug variables
    float maxDist = 100.0f; // Maximum ray distance
    float totalDist = 0.0f; // Current ray distance
    
    while (hit == 0 && totalDist < maxDist) {
        // Jump to next map square
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
            totalDist += deltaDistX;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
            totalDist += deltaDistY;
        }
        
        // Check if ray has hit a wall
        if (mapX < 0 || mapX >= mapWidth || mapY < 0 || mapY >= mapHeight) {
            break; // Ray is out of bounds
        }
        
        cellValue = mapData[mapY * mapWidth + mapX];
        
        // Check for walls (either regular walls with value 1 or encoded walls with values > 100)
        // The encoding is (textureId + 1) * 100 + 1, so valid wall values are 101, 201, 301, 401
        if (cellValue == 1 || (cellValue > 100 && cellValue <= 401)) {
            hit = 1; // Wall hit
            
            // If this is a regular wall (value 1), set it to use texture 0
            if (cellValue == 1) {
                cellValue = 101; // Encode as texture 0
            }
        }
    }
    
    // If no wall was hit within maxDist, treat it as a hit at maxDist
    if (hit == 0) {
        float perpWallDist = maxDist;
        return;
    }
    
    // Calculate distance projected on camera direction
    float perpWallDist;
    if (side == 0) {
        perpWallDist = (sideDistX - deltaDistX);
    } else {
        perpWallDist = (sideDistY - deltaDistY);
    }
    
    // Save z-buffer value - use original y coordinate
    zBuffer[y * screenWidth + x] = perpWallDist;
    
    // Calculate height of line to draw on screen
    int lineHeight = static_cast<int>(screenHeight / perpWallDist);
    
    // Calculate drawing boundaries with vertical offset
    int drawStart = -lineHeight / 2 + screenHeight / 2 + static_cast<int>(totalVerticalOffset);
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + screenHeight / 2 + static_cast<int>(totalVerticalOffset);
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
        texNum = max(0, min(3, texNum));
    } else if (cellValue == 1) {
        // For regular walls, use texture 0
        texNum = 0;
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
    
    // Calculate surface normal for lighting
    float3 normal;
    if (side == 0) {
        normal.x = -stepX;
        normal.y = 0.0f;
        normal.z = 0.0f;
    } else {
        normal.x = 0.0f;
        normal.y = -stepY;
        normal.z = 0.0f;
    }
    
    // Calculate surface position for lighting
    float2 wallPos;
    wallPos.x = posX + rayDirX * perpWallDist;
    wallPos.y = posY + rayDirY * perpWallDist;
    
    // Calculate lighting for this wall
    float3 lighting = calculateLighting(wallPos, normal, perpWallDist, playerPos, lights, numLights, *ambient);
    
    // Draw the wall
    for (int i = drawStart; i <= drawEnd; i++) {
        // Calculate y coordinate on the texture
        int texY = static_cast<int>((i - drawStart) * static_cast<float>(textureHeight) / lineHeight);
        
        // Ensure texture coordinates are within bounds
        texX = (texX < 0) ? 0 : (texX >= textureWidth) ? textureWidth - 1 : texX;
        texY = (texY < 0) ? 0 : (texY >= textureHeight) ? textureHeight - 1 : texY;
        
        // Get texture pixel - use texNum as the texture index
        uint32_t color = wallTextures[texNum * textureWidth * textureHeight + texY * textureWidth + texX];
        
        // Extract RGB components
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        
        // Apply lighting to the color
        r = static_cast<uint8_t>(r * lighting.x);
        g = static_cast<uint8_t>(g * lighting.y);
        b = static_cast<uint8_t>(b * lighting.z);
        
        // Make color darker for y-sides (additional shading for depth perception)
        if (side == 1) {
            r = static_cast<uint8_t>(r * 0.8f);
            g = static_cast<uint8_t>(g * 0.8f);
            b = static_cast<uint8_t>(b * 0.8f);
        }
        
        // Recombine
        color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        
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
            // Adjust i for the vertical offset when calculating distances
            float adjustedI = i - totalVerticalOffset;
            
            // Current distance from camera to floor using adjusted i
            float currentDist = screenHeight / (2.0f * adjustedI - screenHeight);
            
            // Weight for interpolation between wall position and player position
            float weight = currentDist / perpWallDist;
            
            // Get floor coordinates
            float floorX = weight * floorXWall + (1.0f - weight) * posX;
            float floorY = weight * floorYWall + (1.0f - weight) * posY;
            
            // Create floor position for lighting
            float2 floorPos = make_float2(floorX, floorY);
            
            // Create floor normal (facing up)
            float3 floorNormal = make_float3(0.0f, 0.0f, 1.0f);
            
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
            
            // Calculate lighting for floor
            float3 floorLighting = calculateLighting(floorPos, floorNormal, currentDist, playerPos, lights, numLights, *ambient);
            
            // Extract RGB components for floor
            uint8_t fr = (floorColor >> 16) & 0xFF;
            uint8_t fg = (floorColor >> 8) & 0xFF;
            uint8_t fb = floorColor & 0xFF;
            
            // Apply lighting to floor color
            fr = static_cast<uint8_t>(fr * floorLighting.x);
            fg = static_cast<uint8_t>(fg * floorLighting.y);
            fb = static_cast<uint8_t>(fb * floorLighting.z);
            
            // Recombine floor color
            floorColor = (0xFF << 24) | (fr << 16) | (fg << 8) | fb;
            
            // Extract RGB components for ceiling
            uint8_t cr = (ceilingColor >> 16) & 0xFF;
            uint8_t cg = (ceilingColor >> 8) & 0xFF;
            uint8_t cb = ceilingColor & 0xFF;
            
            // Create ceiling normal (facing down)
            float3 ceilingNormal = make_float3(0.0f, 0.0f, -1.0f);
            
            // Calculate lighting for ceiling
            float3 ceilingLighting = calculateLighting(floorPos, ceilingNormal, currentDist, playerPos, lights, numLights, *ambient);
            
            // Apply lighting to ceiling color (make ceiling slightly brighter)
            cr = static_cast<uint8_t>(cr * ceilingLighting.x * 1.1f);
            cg = static_cast<uint8_t>(cg * ceilingLighting.y * 1.1f);
            cb = static_cast<uint8_t>(cb * ceilingLighting.z * 1.1f);
            
            // Recombine ceiling color
            ceilingColor = (0xFF << 24) | (cr << 16) | (cg << 8) | cb;
            
            // Draw floor pixel using original i coordinate
            frameBuffer[i * screenWidth + x] = floorColor;
            
            // Draw ceiling pixel using original coordinates
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
        
        // Create ceiling position and normal for lighting
        float2 ceilingPos = make_float2(ceilingX, ceilingY);
        float3 ceilingNormal = make_float3(0.0f, 0.0f, -1.0f);
        
        // Get texture coordinates
        int ceilingTexX = static_cast<int>(ceilingX * textureWidth) % textureWidth;
        int ceilingTexY = static_cast<int>(ceilingY * textureHeight) % textureHeight;
        
        // Ensure texture coordinates are within bounds
        ceilingTexX = (ceilingTexX < 0) ? 0 : (ceilingTexX >= textureWidth) ? textureWidth - 1 : ceilingTexX;
        ceilingTexY = (ceilingTexY < 0) ? 0 : (ceilingTexY >= textureHeight) ? textureHeight - 1 : ceilingTexY;
        
        // Get ceiling texture pixel - use texture ID 2 (ceiling texture)
        uint32_t ceilingColor = ceilingTextures[ceilingTexY * textureWidth + ceilingTexX];
        
        // Calculate lighting for this part of the ceiling
        float3 ceilingLighting = calculateLighting(ceilingPos, ceilingNormal, currentDist, playerPos, lights, numLights, *ambient);
        
        // Extract RGB components
        uint8_t r = (ceilingColor >> 16) & 0xFF;
        uint8_t g = (ceilingColor >> 8) & 0xFF;
        uint8_t b = ceilingColor & 0xFF;
        
        // Apply lighting (make ceiling slightly brighter)
        r = static_cast<uint8_t>(r * ceilingLighting.x * 1.1f);
        g = static_cast<uint8_t>(g * ceilingLighting.y * 1.1f);
        b = static_cast<uint8_t>(b * ceilingLighting.z * 1.1f);
        
        // Recombine
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
    int textureHeight,
    CudaLight* lights,
    int numLights,
    CudaAmbientLight* ambient
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
        textureHeight,
        lights,
        numLights,
        ambient
    );
}

// Helper function to check CUDA errors
void checkCudaError(cudaError_t error, const char* message) {
    if (error != cudaSuccess) {
        fprintf(stderr, "CUDA error: %s: %s\n", message, cudaGetErrorString(error));
        exit(-1);
    }
} 