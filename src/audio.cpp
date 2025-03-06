#include "audio.h"
#include <vector>

AudioSystem::AudioSystem()
    : m_initialized(false)
    , m_audioRate(44100)
    , m_audioFormat(AUDIO_S16SYS)
    , m_audioChannels(2)
    , m_audioBuffers(4096)
    , m_currentMusic(nullptr)
    , m_musicVolume(MIX_MAX_VOLUME)
    , m_sfxVolume(MIX_MAX_VOLUME)
    , m_usingPulseAudio(false)
{
}

AudioSystem::~AudioSystem() {
    cleanup();
}

bool AudioSystem::init(int audioRate, Uint16 audioFormat, int audioChannels, int audioBuffers) {
    std::cout << "[AUDIO] Initializing audio system..." << std::endl;
    std::cout << "[AUDIO] Rate: " << audioRate << ", Format: " << audioFormat 
              << ", Channels: " << audioChannels << ", Buffers: " << audioBuffers << std::endl;
    
    // Store audio settings
    m_audioRate = audioRate;
    m_audioFormat = audioFormat;
    m_audioChannels = audioChannels;
    m_audioBuffers = audioBuffers;
    
    // Detect if we're running in WSL
    bool isWSL = false;
    #ifdef __linux__
    FILE* fp = fopen("/proc/version", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "microsoft") || strstr(buffer, "Microsoft")) {
                isWSL = true;
                std::cout << "[AUDIO] Detected running in Windows Subsystem for Linux (WSL)" << std::endl;
            }
        }
        fclose(fp);
    }
    #endif
    
    // First, check if PulseAudio is available (preferred for WSL)
    bool pulseAudioAvailable = false;
    #ifdef __linux__
    // Check for the pulse environment variable first
    const char* pulseServer = SDL_getenv("PULSE_SERVER");
    if (pulseServer) {
        std::cout << "[AUDIO] PulseAudio server detected: " << pulseServer << std::endl;
        pulseAudioAvailable = true;
    } else {
        // Check for pulseaudio with the 'pactl' command
        FILE* cmdOutput = popen("which pactl 2>/dev/null", "r");
        if (cmdOutput) {
            char path[256];
            if (fgets(path, sizeof(path), cmdOutput) != nullptr) {
                pulseAudioAvailable = true;
                std::cout << "[AUDIO] PulseAudio found: " << path;
                
                // Check if PulseAudio server is running
                FILE* paCheck = popen("pactl info 2>/dev/null", "r");
                if (!paCheck || pclose(paCheck) != 0) {
                    std::cout << "[AUDIO] PulseAudio server not running, attempting to start..." << std::endl;
                    
                    // Try to start PulseAudio server
                    system("pulseaudio --start --log-target=syslog 2>/dev/null");
                    
                    // Sleep a bit to give it time to start
                    SDL_Delay(1000);
                }
            }
            pclose(cmdOutput);
        }
    }
    
    // In WSL, we recommend using PulseAudio for best audio quality
    if (isWSL && pulseAudioAvailable) {
        std::cout << "[AUDIO] Using PulseAudio for WSL audio (recommended)" << std::endl;
        SDL_setenv("SDL_AUDIODRIVER", "pulseaudio", 1);
    } 
    else if (isWSL) {
        std::cout << "[AUDIO] PulseAudio not detected. For best audio in WSL, install PulseAudio:" << std::endl;
        std::cout << "[AUDIO] sudo apt-get install pulseaudio pulseaudio-utils" << std::endl;
    }
    #endif
    
    // If not WSL, handle default drivers
    if (!isWSL) {
        // If running in WSL, we need different MIDI handling
        // Set environment variable to prefer native MIDI over Timidity
        #ifdef _WIN32
        SDL_setenv("SDL_FORCE_NATIVE_MIDI", "1", 1);
        std::cout << "[AUDIO] Set SDL_FORCE_NATIVE_MIDI=1 to prefer Windows native MIDI over Timidity" << std::endl;
        #endif
    }
    
    // Initialize SDL_mixer
    if (Mix_OpenAudio(m_audioRate, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
        std::cerr << "[AUDIO] SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        
        // If PulseAudio failed, try falling back to another driver
        if (isWSL && pulseAudioAvailable) {
            std::cerr << "[AUDIO] PulseAudio initialization failed, trying default driver" << std::endl;
            SDL_setenv("SDL_AUDIODRIVER", "", 1);  // Clear the driver setting
            
            if (Mix_OpenAudio(m_audioRate, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
                std::cerr << "[AUDIO] Fallback audio initialization also failed! SDL_mixer Error: " 
                          << Mix_GetError() << std::endl;
                return false;
            }
            std::cout << "[AUDIO] Fallback audio initialized successfully" << std::endl;
            m_usingPulseAudio = false;
        } else {
            return false;
        }
    } else {
        // Successfully initialized audio
        std::cout << "[AUDIO] SDL_mixer initialized successfully" << std::endl;
        
        // Check if we're using PulseAudio
        #ifdef __linux__
        char* sdlAudioDriver = SDL_getenv("SDL_AUDIODRIVER");
        if (sdlAudioDriver && strcmp(sdlAudioDriver, "pulseaudio") == 0) {
            m_usingPulseAudio = true;
            std::cout << "[AUDIO] Using PulseAudio as SDL audio driver" << std::endl;
        }
        #endif
    }
    
    // Allocate more channels for sound effects (default is 8)
    // This allows multiple sound effects to play simultaneously
    // Reduced from 32 to 16 to prevent resource contention with MIDI playback
    Mix_AllocateChannels(16);
    std::cout << "[AUDIO] Allocated 16 channels for sound effects" << std::endl;
    
    // Initialize MIDI support
    int flags = Mix_Init(MIX_INIT_MID);
    std::cout << "[AUDIO] Mix_Init flags: " << flags << std::endl;
    
    // Check if MIDI initialization was successful
    if ((flags & MIX_INIT_MID) != MIX_INIT_MID) {
        std::cerr << "[AUDIO] MIDI support could not be initialized! SDL_mixer Error: " << Mix_GetError() << std::endl;
        std::cerr << "[AUDIO] MIDI playback may have degraded quality or fail entirely." << std::endl;
        // Continue anyway, but warn the user
    } else {
        std::cout << "[AUDIO] MIDI support initialized successfully" << std::endl;
    }
    
    // Set default volumes
    Mix_VolumeMusic(m_musicVolume);
    Mix_Volume(-1, m_sfxVolume);  // Set volume for all channels
    std::cout << "[AUDIO] Set default volumes - Music: " << m_musicVolume << ", SFX: " << m_sfxVolume << std::endl;
    
    std::cout << "[AUDIO] Audio system initialized successfully" << std::endl;
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

bool AudioSystem::configureMidiQuality(int frequency) {
    if (!m_initialized) return false;
    
    // Get the music type of the current track
    Mix_MusicType type = Mix_GetMusicType(m_currentMusic);
    
    // Only apply settings if it's a MIDI file
    if (type == MUS_MID) {
        std::cout << "[AUDIO] Configuring MIDI quality settings..." << std::endl;
        std::cout << "[AUDIO] " << getMidiBackendInfo() << std::endl;
        
        // Close and reopen audio with the specified frequency
        // This can improve MIDI synthesis quality
        Mix_CloseAudio();
        
        if (Mix_OpenAudio(frequency, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
            std::cerr << "[AUDIO] Failed to reconfigure audio! SDL_mixer Error: " << Mix_GetError() << std::endl;
            
            // Try to reopen with original settings
            if (Mix_OpenAudio(m_audioRate, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
                std::cerr << "[AUDIO] Critical error: Failed to restore audio settings!" << std::endl;
                m_initialized = false;
                return false;
            }
            return false;
        }
        
        std::cout << "[AUDIO] MIDI quality settings adjusted. New frequency: " << frequency << std::endl;
        std::cout << "[AUDIO] " << getMidiBackendInfo() << std::endl;
        
        // Re-allocate channels
        Mix_AllocateChannels(16);
        
        // We need to reload and restart the music since we closed the audio
        if (m_currentMusic) {
            std::string currentMusicPath = m_currentMusicName;
            Mix_FreeMusic(m_currentMusic);
            m_currentMusic = nullptr;
            
            if (!loadMusic(currentMusicPath)) {
                std::cerr << "[AUDIO] Failed to reload music after quality adjustment!" << std::endl;
                return false;
            }
            
            if (!playMusic(true)) {
                std::cerr << "[AUDIO] Failed to restart music after quality adjustment!" << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
    std::cout << "[AUDIO] Current music is not MIDI format, skipping quality adjustment" << std::endl;
    return false;
}

std::string AudioSystem::getMidiBackendInfo() const {
    if (!m_initialized) return "Audio system not initialized";
    
    const char* midiDriver = SDL_getenv("SDL_SOUNDFONTS");
    if (midiDriver == nullptr) {
        midiDriver = "Not set";
    }
    
    std::string backendInfo = "MIDI Backend: ";
    
    // Detect if we're running in WSL
    bool isWSL = false;
    #ifdef __linux__
    FILE* fp = fopen("/proc/version", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "microsoft") || strstr(buffer, "Microsoft")) {
                isWSL = true;
            }
        }
        fclose(fp);
    }
    #endif
    
    if (isWSL) {
        backendInfo += "WSL (Linux) - ";
        if (SDL_getenv("SDL_SOUNDFONTS") != nullptr) {
            backendInfo += "FluidSynth (using soundfont: " + std::string(SDL_getenv("SDL_SOUNDFONTS")) + ")";
        } else {
            backendInfo += "Likely Timidity (native Windows MIDI not available in WSL)";
        }
    }
    #ifdef _WIN32
    else if (!isWSL) {
        // Native Windows environment
        if (SDL_getenv("SDL_FORCE_NATIVE_MIDI") != nullptr) {
            backendInfo += "Windows Native MIDI (forced)";
        } else {
            // Try to detect based on available information
            if (SDL_getenv("SDL_SOUNDFONTS") != nullptr) {
                backendInfo += "FluidSynth (using soundfont: " + std::string(SDL_getenv("SDL_SOUNDFONTS")) + ")";
            } else if (Mix_GetMusicType(m_currentMusic) == MUS_MID) {
                // We can't be sure which one is being used at runtime without more info from SDL_mixer
                backendInfo += "Either Windows Native MIDI or Timidity";
            } else {
                backendInfo += "Unknown (no MIDI music loaded)";
            }
        }
    }
    #endif
    #ifndef _WIN32
    else if (!isWSL) {
        // Non-Windows, non-WSL platform (native Linux, macOS, etc.)
        if (SDL_getenv("SDL_SOUNDFONTS") != nullptr) {
            backendInfo += "FluidSynth (using soundfont: " + std::string(SDL_getenv("SDL_SOUNDFONTS")) + ")";
        } else {
            backendInfo += "Likely Timidity";
        }
    }
    #endif
    
    // Add info about Timidity config if available
    const char* timidityConfig = SDL_getenv("TIMIDITY_CFG");
    if (timidityConfig != nullptr) {
        backendInfo += " (TIMIDITY_CFG: " + std::string(timidityConfig) + ")";
    }
    
    return backendInfo;
}

bool AudioSystem::setSoundFont(const std::string& soundfontPath) {
    if (!m_initialized) return false;
    
    std::cout << "[AUDIO] Setting soundfont to: " << soundfontPath << std::endl;
    
    // Set the SDL_SOUNDFONTS environment variable
    // This will make SDL_mixer use FluidSynth with this soundfont
    if (SDL_setenv("SDL_SOUNDFONTS", soundfontPath.c_str(), 1) != 0) {
        std::cerr << "[AUDIO] Failed to set SDL_SOUNDFONTS environment variable" << std::endl;
        return false;
    }
    
    // If we're forcing native MIDI, disable it to use FluidSynth
    if (SDL_getenv("SDL_FORCE_NATIVE_MIDI") != nullptr) {
        SDL_setenv("SDL_FORCE_NATIVE_MIDI", "0", 1);
        std::cout << "[AUDIO] Disabled SDL_FORCE_NATIVE_MIDI to use FluidSynth soundfont" << std::endl;
    }
    
    std::cout << "[AUDIO] Soundfont set successfully. Restart any playing music for changes to take effect" << std::endl;
    
    // If music is currently playing, we need to restart it for the changes to take effect
    if (m_currentMusic && Mix_PlayingMusic()) {
        std::string currentMusicPath = m_currentMusicName;
        stopMusic();
        Mix_FreeMusic(m_currentMusic);
        m_currentMusic = nullptr;
        
        if (loadMusic(currentMusicPath)) {
            playMusic(true);
            std::cout << "[AUDIO] Restarted music with new soundfont" << std::endl;
        }
    }
    
    return true;
}

bool AudioSystem::forceNativeMidi(bool useNative) {
    if (!m_initialized) return false;
    
    // Detect if we're running in WSL
    bool isWSL = false;
    #ifdef __linux__
    FILE* fp = fopen("/proc/version", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "microsoft") || strstr(buffer, "Microsoft")) {
                isWSL = true;
            }
        }
        fclose(fp);
    }
    #endif
    
    if (isWSL) {
        std::cout << "[AUDIO] Native Windows MIDI is not available in WSL" << std::endl;
        std::cout << "[AUDIO] Using Timidity or FluidSynth instead" << std::endl;
        SDL_setenv("SDL_FORCE_NATIVE_MIDI", "0", 1);
        
        // Optionally set up FluidSynth if a soundfont is available
        std::cout << "[AUDIO] Consider using a soundfont with setSoundFont() for better quality in WSL" << std::endl;
        
        return false;  // Can't force native MIDI in WSL
    } else {
        if (useNative) {
            std::cout << "[AUDIO] Forcing use of Native Windows MIDI" << std::endl;
            SDL_setenv("SDL_FORCE_NATIVE_MIDI", "1", 1);
            
            // When using native MIDI, soundfonts don't apply
            if (SDL_getenv("SDL_SOUNDFONTS") != nullptr) {
                SDL_setenv("SDL_SOUNDFONTS", "", 1);
                std::cout << "[AUDIO] Cleared soundfont setting (not used with native MIDI)" << std::endl;
            }
        } else {
            std::cout << "[AUDIO] Disabling forced Native Windows MIDI" << std::endl;
            SDL_setenv("SDL_FORCE_NATIVE_MIDI", "0", 1);
        }
        
        std::cout << "[AUDIO] MIDI backend preference updated. Restart any playing music for changes to take effect" << std::endl;
    }
    
    // If music is currently playing, we need to restart it for the changes to take effect
    if (m_currentMusic && Mix_PlayingMusic()) {
        std::string currentMusicPath = m_currentMusicName;
        stopMusic();
        Mix_FreeMusic(m_currentMusic);
        m_currentMusic = nullptr;
        
        if (loadMusic(currentMusicPath)) {
            playMusic(true);
            std::cout << "[AUDIO] Restarted music with new MIDI backend settings" << std::endl;
        }
    }
    
    return true;
}

bool AudioSystem::configureTimidityForWSL(const std::string& configPath) {
    if (!m_initialized) return false;
    
    // Detect if we're running in WSL
    bool isWSL = false;
    #ifdef __linux__
    FILE* fp = fopen("/proc/version", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "microsoft") || strstr(buffer, "Microsoft")) {
                isWSL = true;
            }
        }
        fclose(fp);
    }
    #endif
    
    if (!isWSL) {
        std::cout << "[AUDIO] This method is intended for WSL environments only" << std::endl;
        return false;
    }
    
    std::cout << "[AUDIO] Configuring Timidity for WSL environment..." << std::endl;
    
    // Check if Timidity is installed
    bool timidityInstalled = false;
    FILE* cmdOutput = popen("which timidity 2>/dev/null", "r");
    if (cmdOutput) {
        char path[256];
        if (fgets(path, sizeof(path), cmdOutput) != nullptr) {
            timidityInstalled = true;
            std::cout << "[AUDIO] Timidity found at: " << path;
        }
        pclose(cmdOutput);
    }
    
    if (!timidityInstalled) {
        std::cout << "[AUDIO] Timidity not found. MIDI playback quality may be poor." << std::endl;
        std::cout << "[AUDIO] Consider installing Timidity: sudo apt-get install timidity" << std::endl;
    }
    
    // Ensure we're not using native MIDI (which doesn't work in WSL)
    SDL_setenv("SDL_FORCE_NATIVE_MIDI", "0", 1);
    
    // Try to use FluidSynth first if a soundfont is available
    const char* soundfontEnv = SDL_getenv("SDL_SOUNDFONTS");
    
    if (soundfontEnv != nullptr && strlen(soundfontEnv) > 0) {
        std::cout << "[AUDIO] Using FluidSynth with soundfont: " << soundfontEnv << std::endl;
        return true; // FluidSynth is preferred and already configured
    }
    
    // No soundfont set, so we'll configure Timidity
    
    // Set Timidity configuration file if provided
    if (!configPath.empty()) {
        std::cout << "[AUDIO] Setting custom Timidity config: " << configPath << std::endl;
        if (SDL_setenv("TIMIDITY_CFG", configPath.c_str(), 1) != 0) {
            std::cerr << "[AUDIO] Failed to set TIMIDITY_CFG environment variable" << std::endl;
            return false;
        }
    } else {
        // Check for common Timidity config locations in WSL
        std::vector<std::string> configLocations = {
            "/etc/timidity/timidity.cfg", 
            "/etc/timidity++/timidity.cfg",
            "/usr/local/share/timidity/timidity.cfg"
        };
        
        bool configFound = false;
        for (const auto& path : configLocations) {
            FILE* cfgFile = fopen(path.c_str(), "r");
            if (cfgFile) {
                fclose(cfgFile);
                std::cout << "[AUDIO] Found Timidity config at: " << path << std::endl;
                if (SDL_setenv("TIMIDITY_CFG", path.c_str(), 1) == 0) {
                    configFound = true;
                    break;
                }
            }
        }
        
        if (!configFound) {
            std::cout << "[AUDIO] No Timidity config found. MIDI quality may be poor." << std::endl;
            std::cout << "[AUDIO] Consider installing Timidity package: sudo apt-get install timidity" << std::endl;
            std::cout << "[AUDIO] Or better yet, use FluidSynth with a soundfont for best quality" << std::endl;
        }
    }
    
    // Additional optimizations for Timidity in WSL
    
    // Set a higher sample rate for better quality
    Mix_CloseAudio();  // Close the audio
    
    // Try to reopen with better quality settings
    if (Mix_OpenAudio(48000, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
        std::cerr << "[AUDIO] Failed to reopen audio at higher sample rate" << std::endl;
        // Try to reopen with original settings
        Mix_OpenAudio(m_audioRate, m_audioFormat, m_audioChannels, m_audioBuffers);
    } else {
        std::cout << "[AUDIO] Reopened audio at 48000Hz for better MIDI quality" << std::endl;
    }
    
    // If we have current music, reload it
    if (m_currentMusic && Mix_GetMusicType(m_currentMusic) == MUS_MID) {
        std::string currentPath = m_currentMusicName;
        bool wasPlaying = Mix_PlayingMusic() && !Mix_PausedMusic();
        
        // Reload and restart if it was playing
        stopMusic();
        Mix_FreeMusic(m_currentMusic);
        m_currentMusic = nullptr;
        
        if (loadMusic(currentPath) && wasPlaying) {
            playMusic(true);
        }
    }
    
    std::cout << "[AUDIO] Timidity configuration for WSL complete" << std::endl;
    return true;
}

bool AudioSystem::installSoundFontForWSL(const std::string& destPath) {
    if (!m_initialized) return false;
    
    // Detect if we're running in WSL
    bool isWSL = false;
    #ifdef __linux__
    FILE* fp = fopen("/proc/version", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "microsoft") || strstr(buffer, "Microsoft")) {
                isWSL = true;
            }
        }
        fclose(fp);
    }
    #endif
    
    if (!isWSL) {
        std::cout << "[AUDIO] This method is intended for WSL environments only" << std::endl;
        return false;
    }
    
    std::cout << "[AUDIO] Setting up high-quality soundfont for MIDI playback in WSL..." << std::endl;
    
    // First, check if FluidSynth is installed
    bool fluidsynthInstalled = false;
    FILE* cmdOutput = popen("which fluidsynth 2>/dev/null", "r");
    if (cmdOutput) {
        char path[256];
        if (fgets(path, sizeof(path), cmdOutput) != nullptr) {
            fluidsynthInstalled = true;
            std::cout << "[AUDIO] FluidSynth found at: " << path;
        }
        pclose(cmdOutput);
    }
    
    if (!fluidsynthInstalled) {
        std::cout << "[AUDIO] FluidSynth not found. Installing..." << std::endl;
        std::cout << "[AUDIO] You may be prompted for your password to install packages" << std::endl;
        
        // Try to install FluidSynth
        int result = system("sudo apt-get update && sudo apt-get install -y fluidsynth libfluidsynth-dev");
        if (result != 0) {
            std::cerr << "[AUDIO] Failed to install FluidSynth. Please install manually:" << std::endl;
            std::cerr << "[AUDIO] sudo apt-get install fluidsynth libfluidsynth-dev" << std::endl;
            return false;
        }
    }
    
    // Determine where to save the soundfont
    std::string soundfontPath;
    if (!destPath.empty()) {
        soundfontPath = destPath;
    } else {
        // Default location
        char* homeDir = getenv("HOME");
        if (homeDir) {
            soundfontPath = std::string(homeDir) + "/.local/share/soundfonts";
            // Create directory if it doesn't exist
            system(("mkdir -p " + soundfontPath).c_str());
            soundfontPath += "/fluid-soundfont.sf2";
        } else {
            soundfontPath = "/tmp/fluid-soundfont.sf2";
        }
    }
    
    // Check if soundfont already exists
    FILE* sfFile = fopen(soundfontPath.c_str(), "r");
    if (sfFile) {
        fclose(sfFile);
        std::cout << "[AUDIO] Soundfont already exists at: " << soundfontPath << std::endl;
    } else {
        // Download a high-quality soundfont
        std::cout << "[AUDIO] Downloading FluidR3 GM soundfont..." << std::endl;
        std::string tempFile = "/tmp/fluid-soundfont.sf2";
        std::string cmd = "wget -O " + tempFile + " https://archive.org/download/fluidr3-gm-gs/FluidR3_GM.sf2";
        int result = system(cmd.c_str());
        
        if (result != 0) {
            std::cerr << "[AUDIO] Failed to download soundfont" << std::endl;
            return false;
        }
        
        // Move to final location if different from temp
        if (soundfontPath != tempFile) {
            cmd = "mv " + tempFile + " " + soundfontPath;
            result = system(cmd.c_str());
            if (result != 0) {
                std::cerr << "[AUDIO] Failed to move soundfont to final location" << std::endl;
                // Keep using the temp file location
                soundfontPath = tempFile;
            }
        }
    }
    
    // Set the soundfont
    std::cout << "[AUDIO] Setting soundfont to: " << soundfontPath << std::endl;
    if (!setSoundFont(soundfontPath)) {
        std::cerr << "[AUDIO] Failed to set soundfont" << std::endl;
        return false;
    }
    
    std::cout << "[AUDIO] High-quality soundfont setup complete" << std::endl;
    std::cout << "[AUDIO] MIDI playback should now have much better quality" << std::endl;
    
    return true;
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
    if (!m_initialized) {
        std::cerr << "[AUDIO] Cannot play sound - system not initialized!" << std::endl;
        return false;
    }
    
    // Find the sound effect
    auto it = m_soundEffects.find(name);
    if (it == m_soundEffects.end()) {
        std::cerr << "[AUDIO] Sound effect not found: " << name << std::endl;
        std::cerr << "[AUDIO] Available sounds:" << std::endl;
        for (const auto& pair : m_soundEffects) {
            std::cerr << "  - " << pair.first << std::endl;
        }
        return false;
    }
    
    std::cout << "[AUDIO] Attempting to play sound: " << name << std::endl;
    
    // Try to play the sound effect on the first available channel
    int channel = Mix_PlayChannel(-1, it->second, loops);
    if (channel == -1) {
        std::cerr << "[AUDIO] Failed to play sound effect! SDL_mixer Error: " << Mix_GetError() << std::endl;
        
        // Force play on channel 0 as a fallback
        channel = Mix_PlayChannel(0, it->second, loops);
        if (channel == -1) {
            std::cerr << "[AUDIO] Fallback also failed! SDL_mixer Error: " << Mix_GetError() << std::endl;
            return false;
        }
        std::cout << "[AUDIO] Forced sound effect to play on channel 0" << std::endl;
    }
    
    std::cout << "[AUDIO] Successfully played sound effect '" << name << "' on channel " << channel << std::endl;
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

bool AudioSystem::configurePulseAudio(bool force) {
    if (!m_initialized) return false;
    
    // Check if PulseAudio is available
    bool pulseAudioAvailable = false;
    
    #ifdef __linux__
    // Check for pulseaudio with the 'pactl' command
    FILE* cmdOutput = popen("which pactl 2>/dev/null", "r");
    if (cmdOutput) {
        char path[256];
        if (fgets(path, sizeof(path), cmdOutput) != nullptr) {
            pulseAudioAvailable = true;
            std::cout << "[AUDIO] PulseAudio found: " << path;
        }
        pclose(cmdOutput);
    }
    
    // Check if we're already running with PulseAudio
    char* sdlAudioDriver = SDL_getenv("SDL_AUDIODRIVER");
    if (sdlAudioDriver && strcmp(sdlAudioDriver, "pulseaudio") == 0) {
        m_usingPulseAudio = true;
        std::cout << "[AUDIO] Already using PulseAudio as SDL audio driver" << std::endl;
    }
    #endif
    
    if (!pulseAudioAvailable && !force) {
        std::cout << "[AUDIO] PulseAudio not detected on this system" << std::endl;
        return false;
    }
    
    // Set SDL to use PulseAudio
    std::cout << "[AUDIO] Configuring SDL to use PulseAudio..." << std::endl;
    
    // Need to close the audio before changing the driver
    Mix_CloseAudio();
    
    // Set SDL to use PulseAudio driver
    if (SDL_setenv("SDL_AUDIODRIVER", "pulseaudio", 1) != 0) {
        std::cerr << "[AUDIO] Failed to set SDL_AUDIODRIVER environment variable" << std::endl;
        
        // Try to reopen audio with original settings anyway
        if (Mix_OpenAudio(m_audioRate, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
            std::cerr << "[AUDIO] Critical error: Failed to restore audio with original driver! " 
                      << Mix_GetError() << std::endl;
            m_initialized = false;
            return false;
        }
        return false;
    }
    
    // For PulseAudio, a higher quality setting usually works better
    int newRate = 48000;  // Higher sample rate for better quality
    int newBuffers = 2048;  // Smaller buffer size for lower latency
    
    // Reopen audio with new settings
    std::cout << "[AUDIO] Reopening audio with PulseAudio driver at " << newRate << "Hz" << std::endl;
    if (Mix_OpenAudio(newRate, m_audioFormat, m_audioChannels, newBuffers) < 0) {
        std::cerr << "[AUDIO] Failed to initialize PulseAudio! SDL_mixer Error: " 
                  << Mix_GetError() << std::endl;
        
        // Try to reopen with original settings
        SDL_setenv("SDL_AUDIODRIVER", "", 1);  // Reset to default driver
        if (Mix_OpenAudio(m_audioRate, m_audioFormat, m_audioChannels, m_audioBuffers) < 0) {
            std::cerr << "[AUDIO] Critical error: Failed to restore audio with default driver! " 
                      << Mix_GetError() << std::endl;
            m_initialized = false;
            return false;
        }
        
        std::cerr << "[AUDIO] Reverted to default audio driver" << std::endl;
        m_usingPulseAudio = false;
        return false;
    }
    
    // Successfully set up PulseAudio
    m_usingPulseAudio = true;
    m_audioRate = newRate;
    m_audioBuffers = newBuffers;
    
    std::cout << "[AUDIO] PulseAudio configured successfully at " << newRate << "Hz" << std::endl;
    
    // For MIDI playback, prefer FluidSynth with PulseAudio
    // Only override if not already set to a soundfont
    if (SDL_getenv("SDL_SOUNDFONTS") == nullptr || strlen(SDL_getenv("SDL_SOUNDFONTS")) == 0) {
        // Check for system soundfonts
        std::vector<std::string> systemSoundfonts = {
            "/usr/share/sounds/sf2/FluidR3_GM.sf2",
            "/usr/share/soundfonts/FluidR3_GM.sf2",
            "/usr/share/sounds/sf2/default.sf2"
        };
        
        for (const auto& path : systemSoundfonts) {
            FILE* sf = fopen(path.c_str(), "r");
            if (sf) {
                fclose(sf);
                std::cout << "[AUDIO] Found system soundfont: " << path << std::endl;
                SDL_setenv("SDL_SOUNDFONTS", path.c_str(), 1);
                break;
            }
        }
    }
    
    // Ensure we're not trying to use native MIDI (not compatible with PulseAudio)
    SDL_setenv("SDL_FORCE_NATIVE_MIDI", "0", 1);
    
    // Now re-initialize MIDI support
    int flags = Mix_Init(MIX_INIT_MID);
    std::cout << "[AUDIO] Mix_Init flags with PulseAudio: " << flags << std::endl;
    
    // Allocate channels
    Mix_AllocateChannels(16);
    
    // If we have current music, reload it
    if (m_currentMusic) {
        std::string currentMusicPath = m_currentMusicName;
        bool wasPlaying = Mix_PlayingMusic() && !Mix_PausedMusic();
        
        // Stop and free the current music
        stopMusic();
        Mix_FreeMusic(m_currentMusic);
        m_currentMusic = nullptr;
        
        // Reload and restart if it was playing
        if (loadMusic(currentMusicPath)) {
            if (wasPlaying) {
                playMusic(true);
            }
        }
    }
    
    return true;
} 