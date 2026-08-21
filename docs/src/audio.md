# Audio

## FMOD Integration

VkEngine uses FMOD Studio for professional audio management with spatial 3D positioning, effects, and channel control. The audio system is fully integrated with game objects.

## Audio System Initialization

FMOD is initialized during `Engine::init()`:

- **Max Channels**: 512 simultaneous sounds
- **Output**: Device default (speakers, headphones)
- **Formats**: WAV, OGG, MP3, FLAC, etc. (platform dependent)
- **3D Features**: Spatial positioning with distance-based attenuation
- **Effects**: Reverb and other FMOD effects (default: doppler disabled)

## Sound Resources

### Creating Sounds

```cpp
Sound* createSound(
    std::string name,      // Unique identifier
    const char* path,      // File path
    bool looping,          // true = loop, false = one-shot
    bool three_dim         // true = 3D spatial, false = 2D ambient
);
```

**Example**:
```cpp
// Create one-shot sound
Sound* footstep = engine->createSound("footstep", "assets/footstep.wav", false, true);

// Create looping ambient sound
Sound* windAmbient = engine->createSound("wind", "assets/wind.ogg", true, false);

// Create looping spatial sound (from specific location)
Sound* machinery = engine->createSound("machinery", "assets/machinery.ogg", true, true);
```

### Retrieving Sounds

```cpp
Sound* sound = engine->getSound("footstep");
// Returns cached instance (no reload)
```

### Supported Formats

Via FMOD: WAV, OGG, MP3, FLAC, XMA, AT9, etc.

Common choices:
- **WAV**: High quality, larger file size (good for critical SFX)
- **OGG**: Compressed, smaller file size (good for ambient/music)
- **MP3**: Widely compatible (good for main menu music)

## Playing Sounds from Game Objects

### Playing a Sound

```cpp
gameObject->playSound(Sound* sound, float volume);
```

**Example**:
```cpp
void PlayerCharacter::Update(Engine* engine) {
    Vector3 moveDir = getMovementInput();
    
    if (moveDir.length() > 0) {
        // Play footstep sound
        Sound* step = engine->getSound("footstep");
        playSound(step, 0.7f);  // 70% volume
    }
}
```

### Sound Control

```cpp
void stopAllSounds();
// Stops all sounds playing from this object

void setSoundPause(bool pause);
// Pause/resume all sounds from this object
```

### Channel Group Management

Each GameObject has an internal `FMOD::ChannelGroup*`:
- All sounds from that object belong to its channel group
- Channel groups allow per-object volume and pause control
- Automatically destroyed when object is destroyed

## Spatial Audio (3D Sound)

### 3D Listener

The engine automatically manages the 3D listener position:
- Position: Camera position (updated every frame)
- Forward/Up vectors: Calculated from camera rotation
- Velocity: Used for Doppler effect (currently disabled)

### 3D Sound Sources

GameObjects with 3D sounds:
- Position synchronized with GameObject transform every frame
- Distance attenuation applied automatically
- Panning based on relative position to listener

**Example**: Enemy audio from a specific location

```cpp
class Enemy : public GameObject {
    void Update(Engine* engine) override {
        // Sound automatically follows enemy position
        if (isAlive) {
            engine->getGameObject("sfx_player")->playSound(
                engine->getSound("enemy_growl"), 0.5f
            );
        }
    }
};
```

### Attenuation

3D sounds fade with distance based on FMOD settings:
- Sounds at close range: full volume
- Sounds at medium range: volume decreases
- Sounds at far range: silence

## Global Audio Control

### Master Mute

```cpp
void setGlobalMute(bool mute);
// Mute/unmute all audio
```

**Example**: Pause menu audio muting

```cpp
void PauseMenu::UpdateScene(Engine* engine) {
    if (isPaused) {
        engine->setGlobalMute(true);  // Silent during pause
    } else {
        engine->setGlobalMute(false); // Resume audio
    }
}
```

## Audio Implementation Pattern

### Scene-Based Audio

Audio is typically managed at the scene level:

```cpp
class GameLevel : public Scene {
private:
    Sound* ambience;
    Sound* musicTrack;
    GameObject* audioPlayer;  // For scene-level sounds
    
    void InitScene(Engine* engine) override {
        // Load audio resources
        ambience = engine->createSound("ambient_wind", "assets/wind.ogg", true, false);
        musicTrack = engine->createSound("level_music", "assets/level1_music.ogg", true, false);
        
        // Create "speaker" object for scene-level sounds
        audioPlayer = engine->createGameObject<GameObject>(
            {{0,0,0}, {0,0,0,1}, {1,1,1}},
            nullptr, nullptr, nullptr, false
        );
    }
    
    void UpdateScene(Engine* engine) override {
        // Play ambient sounds
        if (!ambiencePlaying) {
            audioPlayer->playSound(ambience, 0.3f);
            ambiencePlaying = true;
        }
    }
    
    void DestroyScene(Engine* engine) override {
        audioPlayer->stopAllSounds();
    }
};
```

### DualSense Haptics (Windows/PlayStation)

Provide haptic feedback feedback on DualSense controllers:

```cpp
engine->dualsense_playHaptics(Sound* sound, float volume);
```

Maps audio frequencies to haptic patterns.

**Example**: Impact feedback

```cpp
void Enemy::takeDamage(GameObject* attacker) {
    health -= 10;
    
    // Haptic feedback
    if (engine->isDualSenseAttached()) {
        Sound* impact = engine->getSound("impact_sfx");
        engine->dualsense_playHaptics(impact, 1.0f);
    }
}
```

## Audio Implementation Details

### FMOD System Update

- Called every frame in `Engine::update()`
- Updates channel states
- Processes 3D listener and source positions
- Applies effects

### Channel Groups

Per-object channel groups allow:
- Individual object volume control
- Per-object pause/resume
- Grouping of related sounds
- Easier audio debugging

### Memory Management

- Sounds kept resident in system memory
- FMOD manages memory internally
- Streaming supported for large files
- Cleanup on `Engine::cleanup()`

### Distance Attenuation

FMOD distance model (typical):
- Close range (< 1 unit): Full volume
- Far range (> 100 units): Silence
- Linear falloff in between
- Configurable per sound if needed

## Tips and Best Practices

1. **Use looping sounds sparingly**: Looping sounds consume channels longer
2. **One-shot effects are efficient**: Footsteps, impacts, UI clicks
3. **Music should be separate**: Use a dedicated music channel if possible
4. **Spatial audio for environment**: Use 3D for enemies, machinery, events
5. **Volume balancing**: Keep ambient sounds quiet, prioritize player feedback
6. **Test with headphones**: 3D audio is most noticeable with spatial audio
7. **Platform testing**: Audio behavior may differ on Windows vs Linux
8. **Cleanup properly**: Request destruction before unloading scenes
9. **DualSense haptics optional**: Always check `isDualSenseAttached()` first
10. **Profile performance**: Monitor max simultaneous channels in profiling

## Limitations and Future Work

- No real-time audio synthesis
- No custom effects (limited to FMOD built-in)
- No music streaming with playback synchronization
- No multi-listener 3D audio (single camera)
- No audio compression configuration per-file
- Doppler effect currently disabled
