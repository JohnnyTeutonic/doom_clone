#ifndef MAP_H
#define MAP_H

#include <vector>
#include <string>
#include "utils.h"

// Map cell types
enum class CellType {
    Empty = 0,
    Wall = 1,
    Door = 2,
    Item = 3,
    Enemy = 4,
    Stairs = 5,        // Stairs entry point
    ElevatedWall = 6,  // Wall on the elevated level
    StairStep1 = 7,    // First step of stairs (25% elevation)
    StairStep2 = 8,    // Second step of stairs (50% elevation)
    StairStep3 = 9,     // Third step of stairs (75% elevation)
    Floor = 10,
    ElevatedFloor = 11,
    SecretWall = 12,    // Wall that can be opened to reveal a secret area
    TeleportPad = 13    // Teleports player to another location
};

// Door states
enum class DoorState {
    Closed,
    Opening,
    Open,
    Closing
};

// Sector structure for visibility culling
struct Sector {
    int id;                     // Unique sector ID
    std::vector<Vec2> vertices; // Vertices defining the sector boundary
    std::vector<int> neighbors; // IDs of neighboring sectors
    bool isVisible;             // Visibility flag for culling
};

class Map {
private:
    int m_width;
    int m_height;
    std::vector<std::vector<CellType>> m_cells;
    std::vector<std::vector<int>> m_wallTextures;  // texture ID for each wall cell
    std::vector<std::vector<int>> m_elevations; // Elevation level for each cell (0=ground, 1=elevated)
    std::vector<std::vector<float>> m_stepHeights;  // Fractional height for stairs (0.0-1.0)
    
    // Door properties
    std::vector<std::vector<DoorState>> m_doorStates;
    std::vector<std::vector<float>> m_doorOpenAmount;
    std::vector<std::vector<std::pair<int, int>>> m_teleportTargets;   // Target cell coordinates for teleport pads
    
    // Secret wall properties
    std::vector<std::vector<bool>> m_secretFound;  // Whether a secret wall has been found

    // Sector-based culling
    std::vector<Sector> m_sectors;
    std::vector<std::vector<int>> m_cellToSector;  // Maps each cell to its sector ID
    int m_playerSector;  // Current sector the player is in
    std::vector<int> m_visibleSectors;  // List of currently visible sectors
    
public:
    Map(int width = 20, int height = 20);
    ~Map();
    
    // Map loading/saving
    bool loadFromString(const std::string& mapStr);
    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename) const;
    
    // Basic getters
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Cell access
    CellType getCell(int x, int y) const;
    void setCell(int x, int y, CellType type);
    
    // Position validation
    bool isValidPosition(double x, double y) const;
    
    // Wall texture access
    int getWallTexture(int x, int y) const;
    void setWallTexture(int x, int y, int textureId);
    
    // Get a random empty position in the map
    Vec2 getRandomEmptyPosition() const;
    
    // Cast a ray from start to direction and find the first wall it hits
    // Returns the distance to the wall and sets outHitX and outHitY
    double castRay(double startX, double startY, double dirX, double dirY, 
                  double& outHitX, double& outHitY, int& outHitTexture) const;

    // Collision detection
    bool isSolid(int x, int y) const;

    // Door methods
    DoorState getDoorState(int x, int y) const;
    void setDoorState(int x, int y, DoorState state);
    void updateDoors(double deltaTime);
    bool activateDoor(int x, int y);  // Returns true if door was activated
    
    // Secret wall methods
    bool isSecretWall(int x, int y) const;
    bool isSecretFound(int x, int y) const;
    void setSecretFound(int x, int y, bool found);
    bool activateSecret(int x, int y);  // Returns true if secret was activated
    
    // Teleport methods
    bool isTeleportPad(int x, int y) const;
    int getTeleportTarget(int x, int y) const;
    void setTeleportTarget(int x, int y, int targetX, int targetY);
    bool activateTeleport(int x, int y, Vec2& outDestination);  // Returns true if teleport was activated
    
    // Sector-based culling methods
    void createSectors();
    void updateVisibility(const Vec2& playerPos);
    bool isSectorVisible(int sectorId) const;
    int getSectorAt(double x, double y) const;
    int getSectorId(int x, int y) const { return m_cellToSector[y][x]; }
    bool isInSameSector(double x1, double y1, double x2, double y2) const;
    
    // Get the current player sector
    int getPlayerSector() const { return m_playerSector; }
    void setPlayerSector(int sector) { m_playerSector = sector; }
    
    // Get the list of visible sectors
    const std::vector<int>& getVisibleSectors() const { return m_visibleSectors; }
    const std::vector<Sector>& getSectors() const { return m_sectors; }
    
    // Elevation methods
    int getCellElevation(int x, int y) const;
    void setCellElevation(int x, int y, int elevation);
    
    // Step height methods
    float getStepHeight(int x, int y) const;
    void setStepHeight(int x, int y, float height);
    
    // Stairs methods
    bool isStairs(int x, int y) const;
    bool isStairStep(int x, int y) const;
};

#endif // MAP_H 