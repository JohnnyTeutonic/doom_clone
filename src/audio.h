#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <iostream>

// Audio system for handling music and sound effects
class AudioSystem {
private:
    bool m_initialized;
    int m_audioRate;
    Uint16 m_audioFormat;
    int m_audioChannels;
    int m_audioBuffers;
    
    // Current music track
    Mix_Music* m_currentMusic;
    std::string m_currentMusicName;
    
    // Volume settings
    int m_musicVolume;  // 0-128
    int m_sfxVolume;    // 0-128
    
    // Sound effect cache
    std::unordered_map<std::string, Mix_Chunk*> m_soundEffects;
    
    // PulseAudio configuration
    bool m_usingPulseAudio;
    
public:
    AudioSystem();
    ~AudioSystem();
    
    // Initialize the audio system
    bool init(int audioRate = 44100, Uint16 audioFormat = AUDIO_S16SYS, 
              int audioChannels = 2, int audioBuffers = 4096);
    
    // Clean up resources
    void cleanup();
    
    // Music functions
    bool loadMusic(const std::string& filename);
    bool playMusic(bool loop = true);
    void stopMusic();
    void pauseMusic();
    void resumeMusic();
    bool isMusicPlaying() const;
    
    // MIDI quality settings
    bool configureMidiQuality(int frequency = 44100);
    
    // MIDI backend detection
    std::string getMidiBackendInfo() const;
    
    // Set a soundfont for MIDI playback (using FluidSynth)
    bool setSoundFont(const std::string& soundfontPath);
    
    // Force specific MIDI backend
    bool forceNativeMidi(bool useNative = true);
    
    // WSL-specific Timidity configuration
    bool configureTimidityForWSL(const std::string& configPath = "");
    
    // Download and install a high-quality soundfont for WSL
    bool installSoundFontForWSL(const std::string& destPath = "");
    
    // Configure PulseAudio for MIDI playback
    bool configurePulseAudio(bool force = true);
    
    // Sound effect functions
    bool loadSoundEffect(const std::string& name, const std::string& filename);
    bool playSoundEffect(const std::string& name, int loops = 0);
    
    // Volume control
    void setMusicVolume(int volume);
    void setSfxVolume(int volume);
    int getMusicVolume() const { return m_musicVolume; }
    int getSfxVolume() const { return m_sfxVolume; }
    
    // Status
    bool isInitialized() const { return m_initialized; }
    bool isPulseAudioEnabled() const { return m_usingPulseAudio; }
};

// Sound effect types
enum class SoundEffect {
    PlayerHurt,
    PlayerDeath,
    EnemyHurt,
    EnemyDeath,
    WeaponFire,
    WeaponReload,
    ItemPickup,
    DoorOpen,
    DoorClose,
    Explosion,
    
    // Doom-specific sounds
    WeaponShotgun,
    WeaponChainsaw,
    WeaponRocketLaunch,
    WeaponPlasmaFire,
    WeaponBFG,
    PowerupPickup,
    TeleportSound,
    MonsterAlert,
    MonsterPain,
    MonsterDeath,
    ImpAttack,
    ImpDeath,
    PlayerGrunt
};

#endif // AUDIO_H 