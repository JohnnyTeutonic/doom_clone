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
    
    // Create an SDL surface from the decoded data
    // Using 0 for the masks forces SDL to use the correct format for the current platform
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
    
    if (!surface) {
        std::cerr << "Failed to create SDL surface: " << SDL_GetError() << std::endl;
        WebPFree(rgba);
        return nullptr;
    }
    
    // We need to ensure the pixel format is correct for SDL
    SDL_Surface* formatted_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    
    if (!formatted_surface) {
        std::cerr << "Failed to convert surface format: " << SDL_GetError() << std::endl;
        WebPFree(rgba);
        return nullptr;
    }
    
    // Copy the decoded data to the SDL surface
    SDL_LockSurface(formatted_surface);
    memcpy(formatted_surface->pixels, rgba, width * height * 4);
    SDL_UnlockSurface(formatted_surface);
    
    // Free the decoded data
    WebPFree(rgba);
    
    // Set colorkey for transparency
    SDL_SetColorKey(formatted_surface, SDL_TRUE, SDL_MapRGB(formatted_surface->format, 0, 0, 0));
    
    return formatted_surface;
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
                      << " at position (" << iter.x_offset << ", " << iter.y_offset << ")" << std::endl;
            
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