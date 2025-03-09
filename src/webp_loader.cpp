#include "utils.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

// Include libwebp headers
#include <webp/decode.h>
#include <webp/demux.h>

// Helper function to read a file into memory
bool readFileToBuffer(const std::string& filename, std::vector<uint8_t>& buffer) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    buffer.resize(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return false;
    }
    
    return true;
}

// Convert WebP frame to SDL_Surface with simplified approach
SDL_Surface* webpFrameToSurface(const uint8_t* data, size_t data_size) {
    // First, decode the WebP to get the dimensions
    int width, height;
    if (!WebPGetInfo(data, data_size, &width, &height)) {
        std::cerr << "Failed to get WebP dimensions" << std::endl;
        return nullptr;
    }
    
    // Decode WebP directly to RGBA
    uint8_t* rgba = WebPDecodeRGBA(data, data_size, &width, &height);
    if (!rgba) {
        std::cerr << "Failed to decode WebP to RGBA" << std::endl;
        return nullptr;
    }
    
    std::cout << "WebP successfully decoded: " << width << "x" << height << " pixels" << std::endl;
    
    // Create SDL surface with the correct format
    // Note: On little-endian systems, RGBA when viewed as 32-bit is ABGR
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 
                                             0x000000FF,  // R mask
                                             0x0000FF00,  // G mask
                                             0x00FF0000,  // B mask
                                             0xFF000000); // A mask
    
    if (!surface) {
        std::cerr << "Failed to create SDL surface: " << SDL_GetError() << std::endl;
        WebPFree(rgba);
        return nullptr;
    }
    
    // Lock surface for pixel access
    if (SDL_LockSurface(surface) != 0) {
        std::cerr << "Failed to lock surface: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        WebPFree(rgba);
        return nullptr;
    }
    
    // Copy pixels manually, one by one to ensure correct format
    uint8_t* srcPixels = rgba;
    uint8_t* dstPixels = static_cast<uint8_t*>(surface->pixels);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Get source pixel components
            uint8_t r = srcPixels[0];
            uint8_t g = srcPixels[1];
            uint8_t b = srcPixels[2];
            uint8_t a = srcPixels[3];
            
            // Calculate destination pixel position
            int dstOffset = y * surface->pitch + x * 4;
            
            // Store in the destination surface's format
            dstPixels[dstOffset + 0] = r;  // R
            dstPixels[dstOffset + 1] = g;  // G
            dstPixels[dstOffset + 2] = b;  // B
            dstPixels[dstOffset + 3] = a;  // A
            
            // Move to next source pixel
            srcPixels += 4;
        }
    }
    
    // Unlock surface
    SDL_UnlockSurface(surface);
    
    // Free the WebP-decoded data
    WebPFree(rgba);
    
    // Set blend mode for transparency
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    
    std::cout << "SDL surface created successfully with blend mode: " << 
        (SDL_GetSurfaceBlendMode(surface, nullptr) == SDL_BLENDMODE_BLEND ? "BLEND" : "OTHER") << std::endl;
    
    return surface;
}

bool isWebpAnimated(const std::string& filename) {
    // Read the file into memory
    std::vector<uint8_t> data;
    if (!readFileToBuffer(filename, data)) {
        return false;
    }
    
    // Create a WebP data source
    WebPData webp_data;
    webp_data.bytes = data.data();
    webp_data.size = data.size();
    
    // Create a WebP demuxer
    WebPDemuxer* demux = WebPDemux(&webp_data);
    if (!demux) {
        std::cerr << "Failed to create WebP demuxer" << std::endl;
        return false;
    }
    
    // Get the number of frames
    uint32_t frameCount = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
    
    // Free the demuxer
    WebPDemuxDelete(demux);
    
    // If there's more than one frame, it's animated
    return frameCount > 1;
}

int getWebpFrameCount(const std::string& filename) {
    // Read the file into memory
    std::vector<uint8_t> data;
    if (!readFileToBuffer(filename, data)) {
        return 0;
    }
    
    // Create a WebP data source
    WebPData webp_data;
    webp_data.bytes = data.data();
    webp_data.size = data.size();
    
    // Create a WebP demuxer
    WebPDemuxer* demux = WebPDemux(&webp_data);
    if (!demux) {
        std::cerr << "Failed to create WebP demuxer" << std::endl;
        return 0;
    }
    
    // Get the number of frames
    uint32_t frameCount = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
    
    // Free the demuxer
    WebPDemuxDelete(demux);
    
    return frameCount;
}

std::vector<SDL_Surface*> loadAnimatedWebp(const std::string& filename) {
    std::vector<SDL_Surface*> frames;
    
    // Read the file into memory
    std::vector<uint8_t> data;
    if (!readFileToBuffer(filename, data)) {
        return frames;
    }
    
    // Create a WebP data source
    WebPData webp_data;
    webp_data.bytes = data.data();
    webp_data.size = data.size();
    
    // Create a WebP demuxer
    WebPDemuxer* demux = WebPDemux(&webp_data);
    if (!demux) {
        std::cerr << "Failed to create WebP demuxer" << std::endl;
        return frames;
    }
    
    // Get the number of frames
    uint32_t frameCount = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
    std::cout << "WebP file has " << frameCount << " frames" << std::endl;
    
    // Get canvas dimensions
    uint32_t width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
    uint32_t height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);
    std::cout << "WebP canvas dimensions: " << width << "x" << height << std::endl;
    
    // If it's not animated or has only one frame, try to load it with SDL_image
    if (frameCount <= 1) {
        SDL_Surface* surface = IMG_Load(filename.c_str());
        if (surface) {
            frames.push_back(surface);
        }
        WebPDemuxDelete(demux);
        return frames;
    }
    
    // Iterate through all frames
    WebPIterator iter;
    if (WebPDemuxGetFrame(demux, 1, &iter)) {
        do {
            // Get frame details
            std::cout << "Frame dimensions: " << iter.width << "x" << iter.height 
                      << " at position (" << iter.x_offset << ", " << iter.y_offset << ")" 
                      << ", duration: " << iter.duration << "ms" << std::endl;
            
            // WebP animated frames can have different characteristics
            bool hasAlpha = iter.has_alpha != 0;
            bool shouldBlend = iter.blend_method == WEBP_MUX_BLEND;
            
            std::cout << "Frame has alpha: " << (hasAlpha ? "yes" : "no") 
                      << ", blend method: " << (shouldBlend ? "blend" : "no blend")
                      << ", dispose method: " << (iter.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND ? "background" : "none") 
                      << std::endl;
            
            // Use larger dimensions for the imp (the original WebP has extra transparency around it)
            int frameWidth = std::max(200, static_cast<int>(iter.width));
            int frameHeight = std::max(300, static_cast<int>(iter.height));
            
            // Create a blank surface that's a bit larger than the frame
            SDL_Surface* surface = SDL_CreateRGBSurface(0, frameWidth, frameHeight, 32, 
                                                      0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
            
            if (surface) {
                // Fill with transparent black
                SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
                
                // Get a temporary surface with the actual frame data
                SDL_Surface* frameSurface = webpFrameToSurface(iter.fragment.bytes, iter.fragment.size);
                if (frameSurface) {
                    // Blit the frame onto our blank surface, centered
                    SDL_Rect dstRect = {
                        (frameWidth - frameSurface->w) / 2,   // Center horizontally
                        (frameHeight - frameSurface->h) / 2,  // Center vertically
                        frameSurface->w,
                        frameSurface->h
                    };
                    
                    SDL_BlitSurface(frameSurface, NULL, surface, &dstRect);
                    SDL_FreeSurface(frameSurface);
                    
                    // Make sure the blend mode is set
                    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
                    
                    frames.push_back(surface);
                    std::cout << "Added frame " << frames.size() << " with dimensions " 
                              << surface->w << "x" << surface->h << std::endl;
                } else {
                    // Failed to create frame surface, free the main surface
                    SDL_FreeSurface(surface);
                }
            }
        } while (WebPDemuxNextFrame(&iter));
        
        // Release the iterator
        WebPDemuxReleaseIterator(&iter);
    }
    
    // Free the demuxer
    WebPDemuxDelete(demux);
    
    return frames;
}

SDL_Surface* loadWebpFrame(const std::string& filename, int frameIndex) {
    // Read the file into memory
    std::vector<uint8_t> data;
    if (!readFileToBuffer(filename, data)) {
        return nullptr;
    }
    
    // Create a WebP data source
    WebPData webp_data;
    webp_data.bytes = data.data();
    webp_data.size = data.size();
    
    // Create a WebP demuxer
    WebPDemuxer* demux = WebPDemux(&webp_data);
    if (!demux) {
        std::cerr << "Failed to create WebP demuxer" << std::endl;
        return nullptr;
    }
    
    // Get the number of frames
    uint32_t frameCount = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
    
    // Check if the requested frame exists
    if (frameIndex < 0 || frameIndex >= static_cast<int>(frameCount)) {
        std::cerr << "Invalid frame index: " << frameIndex << " (total frames: " << frameCount << ")" << std::endl;
        WebPDemuxDelete(demux);
        return nullptr;
    }
    
    // Get the requested frame
    WebPIterator iter;
    SDL_Surface* surface = nullptr;
    
    if (WebPDemuxGetFrame(demux, frameIndex + 1, &iter)) {  // WebP frames are 1-indexed
        // Extract the frame
        WebPData frame_data = iter.fragment;
        
        // Convert the frame to an SDL surface
        surface = webpFrameToSurface(frame_data.bytes, frame_data.size);
        
        // Release the iterator
        WebPDemuxReleaseIterator(&iter);
    }
    
    // Free the demuxer
    WebPDemuxDelete(demux);
    
    return surface;
} 