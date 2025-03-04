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
    Enemy = 4
};

class Map {
private:
    int m_width;
    int m_height;
    std::vector<CellType> m_cells;
    std::vector<int> m_wallTextures;  // texture ID for each wall cell

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