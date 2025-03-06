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

// Convert WebP frame to SDL_Surface
SDL_Surface* webpFrameToSurface(const uint8_t* data, size_t data_size) {
    int width, height;
    
    // Get the dimensions of the WebP image
    if (!WebPGetInfo(data, data_size, &width, &height)) {
        std::cerr << "Failed to get WebP image dimensions" << std::endl;
        return nullptr;
    }
    
    // Decode the WebP image with alpha channel
    uint8_t* rgba = WebPDecodeRGBA(data, data_size, &width, &height);
    if (!rgba) {
        std::cerr << "Failed to decode WebP image" << std::endl;
        return nullptr;
    }
    
    // Create an SDL surface with the correct format for your platform
    // For Windows/SDL2, RGBA is typically stored as ABGR in memory
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        Uint32 rmask = 0xFF000000;
        Uint32 gmask = 0x00FF0000;
        Uint32 bmask = 0x0000FF00;
        Uint32 amask = 0x000000FF;
    #else
        Uint32 rmask = 0x000000FF;
        Uint32 gmask = 0x0000FF00;
        Uint32 bmask = 0x00FF0000;
        Uint32 amask = 0xFF000000;
    #endif
    
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                              rmask, gmask, bmask, amask);
    
    if (!surface) {
        std::cerr << "Failed to create SDL surface: " << SDL_GetError() << std::endl;
        WebPFree(rgba);
        return nullptr;
    }
    
    // Copy the decoded data to the SDL surface
    SDL_LockSurface(surface);
    memcpy(surface->pixels, rgba, width * height * 4);
    SDL_UnlockSurface(surface);
    
    // Set the alpha blending mode
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    
    // Free the decoded data
    WebPFree(rgba);
    
    // Debug info
    std::cout << "Created SDL surface from WebP frame: " << width << "x" << height 
              << " with " << (surface->format->BitsPerPixel) << " bits per pixel" << std::endl;
    
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