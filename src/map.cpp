#include "map.h"
#include <fstream>
#include <random>
#include <algorithm>

Map::Map() : m_width(0), m_height(0) {
}

Map::Map(int width, int height) : m_width(width), m_height(height) {
    m_cells.resize(width * height, CellType::Empty);
    m_wallTextures.resize(width * height, 0);  // Default texture ID is 0
}

bool Map::loadFromString(const std::string& mapStr) {
    // Find the width of the map (length until first newline)
    size_t firstNewline = mapStr.find('\n');
    if (firstNewline == std::string::npos) {
        // Single line map
        m_width = mapStr.length();
        m_height = 1;
    } else {
        m_width = firstNewline;
        
        // Count the number of lines
        m_height = 1; // First line
        for (size_t i = 0; i < mapStr.length(); ++i) {
            if (mapStr[i] == '\n') {
                m_height++;
            }
        }
    }
    
    // Resize the cells vector
    m_cells.resize(m_width * m_height, CellType::Empty);
    m_wallTextures.resize(m_width * m_height, 0);
    
    // Parse the map string
    int x = 0, y = 0;
    for (char c : mapStr) {
        if (c == '\n') {
            y++;
            x = 0;
            continue;
        }
        
        if (x < m_width && y < m_height) {
            // Set cell type based on character
            switch (c) {
                case '#':
                    m_cells[y * m_width + x] = CellType::Wall;
                    // Choose wall texture based on position (for variety)
                    m_wallTextures[y * m_width + x] = (x + y) % 4;
                    break;
                case '.':
                    m_cells[y * m_width + x] = CellType::Empty;
                    break;
                case 'D':
                    m_cells[y * m_width + x] = CellType::Door;
                    break;
                case 'I':
                    m_cells[y * m_width + x] = CellType::Item;
                    break;
                case 'E':
                    m_cells[y * m_width + x] = CellType::Enemy;
                    break;
                default:
                    m_cells[y * m_width + x] = CellType::Empty;
                    break;
            }
        }
        
        x++;
    }
    
    return true;
}

bool Map::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content;
    std::string line;
    
    while (std::getline(file, line)) {
        content += line + '\n';
    }
    
    file.close();
    
    return loadFromString(content);
}

bool Map::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            CellType cell = getCell(x, y);
            
            switch (cell) {
                case CellType::Wall:
                    file << '#';
                    break;
                case CellType::Empty:
                    file << '.';
                    break;
                case CellType::Door:
                    file << 'D';
                    break;
                case CellType::Item:
                    file << 'I';
                    break;
                case CellType::Enemy:
                    file << 'E';
                    break;
                default:
                    file << ' ';
                    break;
            }
        }
        file << '\n';
    }
    
    file.close();
    return true;
}

CellType Map::getCell(int x, int y) const {
    // Check bounds
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return CellType::Wall;  // Out of bounds is considered a wall
    }
    
    return m_cells[y * m_width + x];
}

void Map::setCell(int x, int y, CellType type) {
    // Check bounds
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;  // Out of bounds, do nothing
    }
    
    m_cells[y * m_width + x] = type;
}

bool Map::isValidPosition(double x, double y) const {
    // Convert to integer cell coordinates
    int cellX = static_cast<int>(x);
    int cellY = static_cast<int>(y);
    
    // Check bounds
    if (cellX < 0 || cellX >= m_width || cellY < 0 || cellY >= m_height) {
        return false;
    }
    
    // Check if cell is empty
    return m_cells[cellY * m_width + cellX] == CellType::Empty;
}

int Map::getWallTexture(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return 0; // Default texture for out of bounds
    }
    
    if (m_cells[y * m_width + x] == CellType::Wall) {
        return m_wallTextures[y * m_width + x];
    }
    
    return 0; // Default texture for non-walls
}

void Map::setWallTexture(int x, int y, int textureId) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return; // Out of bounds, do nothing
    }
    
    m_wallTextures[y * m_width + x] = textureId;
}

Vec2 Map::getRandomEmptyPosition() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distribX(0, m_width - 1);
    std::uniform_int_distribution<> distribY(0, m_height - 1);
    
    // Try to find an empty cell (max 100 attempts)
    for (int i = 0; i < 100; ++i) {
        int x = distribX(gen);
        int y = distribY(gen);
        
        if (getCell(x, y) == CellType::Empty) {
            // Add 0.5 to position the entity in the middle of the cell
            return Vec2(x + 0.5, y + 0.5);
        }
    }
    
    // Fallback if no empty cell is found (shouldn't happen in normal maps)
    return Vec2(1.5, 1.5);
}

double Map::castRay(double startX, double startY, double dirX, double dirY, 
                   double& outHitX, double& outHitY, int& outHitTexture) const {
    // Implementation of Digital Differential Analysis (DDA) algorithm for raycasting
    // Based on the approach used in Wolfenstein 3D / Doom
    
    // Calculate cell position
    int mapX = static_cast<int>(startX);
    int mapY = static_cast<int>(startY);
    
    // Length of ray from current position to next x or y-side
    double sideDistX, sideDistY;
    
    // Length of ray from one x or y-side to next x or y-side
    double deltaDistX = std::abs(1.0 / dirX);
    double deltaDistY = std::abs(1.0 / dirY);
    
    // Direction to step in x or y direction (either +1 or -1)
    int stepX, stepY;
    
    // Calculate step and initial sideDist
    if (dirX < 0) {
        stepX = -1;
        sideDistX = (startX - mapX) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = (mapX + 1.0 - startX) * deltaDistX;
    }
    
    if (dirY < 0) {
        stepY = -1;
        sideDistY = (startY - mapY) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = (mapY + 1.0 - startY) * deltaDistY;
    }
    
    // Perform DDA
    bool hit = false;
    bool hitSideX = false; // Was a NS or a EW wall hit?
    
    while (!hit) {
        // Jump to next map square, either in x-direction, or in y-direction
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            hitSideX = true;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            hitSideX = false;
        }
        
        // Check if ray has hit a wall
        if (getCell(mapX, mapY) == CellType::Wall) {
            hit = true;
        }
        
        // Safety check to prevent infinite loops
        if (mapX < 0 || mapX >= m_width || mapY < 0 || mapY >= m_height) {
            hit = true;
        }
    }
    
    // Calculate distance projected on camera direction
    double perpWallDist;
    if (hitSideX) {
        perpWallDist = (mapX - startX + (1 - stepX) / 2) / dirX;
    } else {
        perpWallDist = (mapY - startY + (1 - stepY) / 2) / dirY;
    }
    
    // Calculate the exact hit position
    if (hitSideX) {
        outHitX = startX + perpWallDist * dirX;
        outHitY = startY + perpWallDist * dirY;
    } else {
        outHitX = startX + perpWallDist * dirX;
        outHitY = startY + perpWallDist * dirY;
    }
    
    // Get the texture ID for the hit wall
    outHitTexture = getWallTexture(mapX, mapY);
    
    // Return the distance to the wall
    return perpWallDist;
}

bool Map::isSolid(int x, int y) const {
    // Check bounds
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return true;  // Out of bounds is considered solid
    }
    
    // Get the cell type at the position
    CellType cell = m_cells[y * m_width + x];
    
    // Return true for walls and doors
    return (cell == CellType::Wall || cell == CellType::Door);
} 