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

// Convert WebP frame to SDL_Surface using a completely manual approach
SDL_Surface* webpFrameToSurface(const uint8_t* data, size_t data_size) {
    // First, decode the WebP to get the raw RGBA data
    int width, height;
    uint8_t* rgba = WebPDecodeRGBA(data, data_size, &width, &height);
    if (!rgba) {
        std::cerr << "Failed to decode WebP image" << std::endl;
        return nullptr;
    }
    
    std::cout << "WebP decoded to dimensions: " << width << "x" << height << std::endl;
    
    // Create a new empty surface with the right format
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 
                                               0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!surface) {
        std::cerr << "Failed to create empty surface: " << SDL_GetError() << std::endl;
        WebPFree(rgba);
        return nullptr;
    }
    
    // Lock the surface for direct pixel manipulation
    if (SDL_LockSurface(surface) != 0) {
        std::cerr << "Failed to lock surface: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        WebPFree(rgba);
        return nullptr;
    }
    
    // Copy the pixel data manually
    Uint32* targetPixels = static_cast<Uint32*>(surface->pixels);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * 4; // RGBA uses 4 bytes per pixel
            
            Uint8 r = rgba[index];     // Red
            Uint8 g = rgba[index + 1]; // Green
            Uint8 b = rgba[index + 2]; // Blue
            Uint8 a = rgba[index + 3]; // Alpha
            
            // Create pixel in the format the surface expects
            Uint32 pixel = SDL_MapRGBA(surface->format, r, g, b, a);
            
            // Set the pixel in the target surface
            targetPixels[y * (surface->pitch / 4) + x] = pixel;
        }
    }
    
    // Unlock the surface
    SDL_UnlockSurface(surface);
    
    // Free the WebP decoded data
    WebPFree(rgba);
    
    // Set surface blend mode to ensure transparency is handled correctly
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    
    // Log the final surface details
    std::cout << "Created surface with dimensions " << surface->w << "x" << surface->h 
              << " format: " << SDL_GetPixelFormatName(surface->format->format) 
              << " Rmask: " << std::hex << surface->format->Rmask
              << " Gmask: " << surface->format->Gmask
              << " Bmask: " << surface->format->Bmask
              << " Amask: " << surface->format->Amask << std::dec
              << std::endl;
    
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
            // Extract the frame
            WebPData frame_data = iter.fragment;
            
            // Get the full frame with transparency
            // The frame dimensions may be different from canvas dimensions
            // and the position might be offset
            std::cout << "Frame dimensions: " << iter.width << "x" << iter.height 
                      << " at position (" << iter.x_offset << ", " << iter.y_offset << ")" 
                      << ", duration: " << iter.duration << "ms" << std::endl;
            
            // For animated WebP, we need special handling to handle frame dispose and blending methods
            // See: https://developers.google.com/speed/webp/docs/api
            bool hasAlpha = iter.has_alpha != 0;
            bool shouldBlend = iter.blend_method == WEBP_MUX_BLEND;
            
            std::cout << "Frame has alpha: " << (hasAlpha ? "yes" : "no") 
                      << ", blend method: " << (shouldBlend ? "blend" : "no blend")
                      << ", dispose method: " << (iter.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND ? "background" : "none") 
                      << std::endl;
            
            // Convert the frame to an SDL surface
            SDL_Surface* surface = webpFrameToSurface(frame_data.bytes, frame_data.size);
            if (surface) {
                frames.push_back(surface);
                std::cout << "Added frame " << frames.size() << " with dimensions " 
                          << surface->w << "x" << surface->h << std::endl;
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