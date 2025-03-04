#include "audio.h"

AudioSystem::AudioSystem()
    : m_initialized(false)
    , m_audioRate(44100)
    , m_audioFormat(AUDIO_S16SYS)
    , m_audioChannels(2)
    , m_audioBuffers(4096)
    , m_currentMusic(nullptr)
    , m_musicVolume(MIX_MAX_VOLUME)
    , m_sfxVolume(MIX_MAX_VOLUME)
{
}

AudioSystem::~AudioSystem() {
    cleanup();
}

bool AudioSystem::init(int audioRate, Uint16 audioFormat, int audioChannels, int audioBuffers) {
    // Store audio settings
    m_audioRate = audioRate;
    m_audioFormat = audioFormat;
    m_audioChannels = audioChannels;
    m_audioBuffers = audioBuffers;
    
    // Initialize SDL_mixer
    if (Mix_OpenAudio(m_audioRate, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
        std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }
    
    // Initialize MIDI support
    Mix_Init(MIX_INIT_MID);
    
    // Set default volumes
    Mix_VolumeMusic(m_musicVolume);
    
    std::cout << "Audio system initialized successfully" << std::endl;
    m_initialized = true;
    return true;
}

void AudioSystem::cleanup() {
    if (!m_initialized) return;
    
    // Stop any playing music
    stopMusic();
    
    // Free music resource
    if (m_currentMusic) {
        Mix_FreeMusic(m_currentMusic);
        m_currentMusic = nullptr;
    }
    
    // Free all sound effects
    for (auto& [name, chunk] : m_soundEffects) {
        if (chunk) {
            Mix_FreeChunk(chunk);
            chunk = nullptr;
        }
    }
    m_soundEffects.clear();
    
    // Close SDL_mixer
    Mix_CloseAudio();
    Mix_Quit();
    
    m_initialized = false;
    std::cout << "Audio system cleaned up" << std::endl;
}

bool AudioSystem::loadMusic(const std::string& filename) {
    if (!m_initialized) return false;
    
    // Stop any currently playing music
    stopMusic();
    
    // Free previous music if it exists
    if (m_currentMusic) {
        Mix_FreeMusic(m_currentMusic);
        m_currentMusic = nullptr;
    }
    
    // Load the music file
    m_currentMusic = Mix_LoadMUS(filename.c_str());
    if (!m_currentMusic) {
        std::cerr << "Failed to load music! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }
    
    m_currentMusicName = filename;
    std::cout << "Music loaded: " << filename << std::endl;
    return true;
}

bool AudioSystem::playMusic(bool loop) {
    if (!m_initialized || !m_currentMusic) return false;
    
    // Play the music, -1 for infinite loop, 0 for once
    if (Mix_PlayMusic(m_currentMusic, loop ? -1 : 0) == -1) {
        std::cerr << "Failed to play music! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }
    
    std::cout << "Playing music: " << m_currentMusicName << (loop ? " (looping)" : "") << std::endl;
    return true;
}

void AudioSystem::stopMusic() {
    if (!m_initialized) return;
    
    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
        std::cout << "Music stopped" << std::endl;
    }
}

void AudioSystem::pauseMusic() {
    if (!m_initialized) return;
    
    if (Mix_PlayingMusic() && !Mix_PausedMusic()) {
        Mix_PauseMusic();
        std::cout << "Music paused" << std::endl;
    }
}

void AudioSystem::resumeMusic() {
    if (!m_initialized) return;
    
    if (Mix_PausedMusic()) {
        Mix_ResumeMusic();
        std::cout << "Music resumed" << std::endl;
    }
}

bool AudioSystem::isMusicPlaying() const {
    if (!m_initialized) return false;
    return Mix_PlayingMusic() && !Mix_PausedMusic();
}

bool AudioSystem::loadSoundEffect(const std::string& name, const std::string& filename) {
    if (!m_initialized) return false;
    
    // Check if sound effect already exists
    auto it = m_soundEffects.find(name);
    if (it != m_soundEffects.end()) {
        // Free the existing sound effect
        Mix_FreeChunk(it->second);
        m_soundEffects.erase(it);
    }
    
    // Load the sound effect
    Mix_Chunk* chunk = Mix_LoadWAV(filename.c_str());
    if (!chunk) {
        std::cerr << "Failed to load sound effect! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }
    
    // Store the sound effect
    m_soundEffects[name] = chunk;
    std::cout << "Sound effect loaded: " << name << " (" << filename << ")" << std::endl;
    return true;
}

bool AudioSystem::playSoundEffect(const std::string& name, int loops) {
    if (!m_initialized) return false;
    
    // Find the sound effect
    auto it = m_soundEffects.find(name);
    if (it == m_soundEffects.end()) {
        std::cerr << "Sound effect not found: " << name << std::endl;
        return false;
    }
    
    // Play the sound effect on the first available channel
    if (Mix_PlayChannel(-1, it->second, loops) == -1) {
        std::cerr << "Failed to play sound effect! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }
    
    return true;
}

void AudioSystem::setMusicVolume(int volume) {
    if (!m_initialized) return;
    
    // Clamp volume to valid range (0-128)
    m_musicVolume = std::max(0, std::min(MIX_MAX_VOLUME, volume));
    Mix_VolumeMusic(m_musicVolume);
}

void AudioSystem::setSfxVolume(int volume) {
    if (!m_initialized) return;
    
    // Clamp volume to valid range (0-128)
    m_sfxVolume = std::max(0, std::min(MIX_MAX_VOLUME, volume));
    
    // Set volume for all channels
    Mix_Volume(-1, m_sfxVolume);
} 