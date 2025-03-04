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
    ElevatedFloor = 11
};

class Map {
private:
    int m_width;
    int m_height;
    std::vector<CellType> m_cells;
    std::vector<int> m_wallTextures;  // texture ID for each wall cell
    std::vector<int> m_cellElevation; // Elevation level for each cell (0=ground, 1=elevated)
    std::vector<float> m_stepHeight;  // Fractional height for stairs (0.0-1.0)

public:
    Map();
    Map(int width, int height);
    
    // Load map from string (similar to ASCII game)
    bool loadFromString(const std::string& mapStr);
    
    // Load map from file
    bool loadFromFile(const std::string& filename);
    
    // Save map to file
    bool saveToFile(const std::string& filename) const;
    
    // Getters
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Cell access
    CellType getCell(int x, int y) const;
    void setCell(int x, int y, CellType type);
    
    // Check if a position is valid (in bounds and not a wall)
    bool isValidPosition(double x, double y) const;
    
    // Wall texture access
    int getWallTexture(int x, int y) const;
    void setWallTexture(int x, int y, int textureId);
    
    // Elevation access
    int getCellElevation(int x, int y) const;
    void setCellElevation(int x, int y, int elevation);
    
    // Get step height for stairs (0.0-1.0)
    float getStepHeight(int x, int y) const;
    void setStepHeight(int x, int y, float height);
    
    // Check if a cell is a stair or stair step
    bool isStairs(int x, int y) const;
    bool isStairStep(int x, int y) const;
    
    // Find a random empty position
    Vec2 getRandomEmptyPosition() const;
    
    // Cast a ray from start to direction and find the first wall it hits
    // Returns the distance to the wall and sets outHitX and outHitY
    double castRay(double startX, double startY, double dirX, double dirY, 
                  double& outHitX, double& outHitY, int& outHitTexture) const;

    // Collision detection
    bool isSolid(int x, int y) const;
};

#endif // MAP_H 