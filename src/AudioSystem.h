#ifndef AUDIO_SYSTEM_H
#define AUDIO_SYSTEM_H

// Platform-specific includes for SDL
#ifdef _WIN32
#include <SDL.h>
#include <SDL_mixer.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

#include <string>
#include <unordered_map>
#include <memory>

// Forward declaration
class Engine;

// Audio system for handling sound effects and music
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();
    
    // Initialize the audio system
    bool init(int frequency = 44100, int channels = 2, int chunkSize = 2048);
    
    // Shutdown and cleanup
    void shutdown();
    
    // Load a sound effect
    bool loadSound(const std::string& name, const std::string& filePath);
    
    // Load a music track
    bool loadMusic(const std::string& name, const std::string& filePath);
    
    // Play a sound effect
    void playSound(const std::string& name, int channel = -1, float volume = 1.0f);
    
    // Play a music track
    void playMusic(const std::string& name, bool loop = true);
    
    // Stop all sounds on a channel
    void stopSound(int channel = -1);
    
    // Stop music
    void stopMusic();
    
    // Set sound volume
    void setSoundVolume(float volume);
    
    // Set music volume
    void setMusicVolume(float volume);
    
    // Check if music is playing
    bool isMusicPlaying() const;
    
    // Set engine reference
    void setEngine(Engine* engine) { m_engine = engine; }
    
private:
    // Sound effect storage
    std::unordered_map<std::string, Mix_Chunk*> m_sounds;
    
    // Music storage
    std::unordered_map<std::string, Mix_Music*> m_music;
    
    // Currently playing music
    std::string m_currentMusic;
    
    // Volume levels
    float m_soundVolume;
    float m_musicVolume;
    
    // Engine reference
    Engine* m_engine;
    
    // Free resources
    void freeResources();
};

#endif // AUDIO_SYSTEM_H 