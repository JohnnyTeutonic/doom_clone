#include "map.h"
#include <fstream>
#include <random>
#include <algorithm>
#include <queue>

Map::Map(int width, int height) : m_width(width), m_height(height) {
    m_cells.resize(height, std::vector<CellType>(width, CellType::Empty));
    m_wallTextures.resize(height, std::vector<int>(width, 0));  // Default texture ID is 0
    m_elevations.resize(height, std::vector<int>(width, 0)); // Default elevation is ground level (0)
    m_stepHeights.resize(height, std::vector<float>(width, 0.0f)); // Default step height is 0
    m_doorStates.resize(height, std::vector<DoorState>(width, DoorState::Closed));
    m_doorOpenAmount.resize(height, std::vector<float>(width, 0.0f));
    m_secretFound.resize(height, std::vector<bool>(width, false));
    m_teleportTargets.resize(height, std::vector<std::pair<int, int>>(width, {-1, -1}));
    
    // Initialize sector-based culling data
    m_cellToSector.resize(height, std::vector<int>(width, -1));
    m_playerSector = -1;
    m_visibleSectors.clear();
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
    m_cells.resize(m_height, std::vector<CellType>(m_width, CellType::Empty));
    m_wallTextures.resize(m_height, std::vector<int>(m_width, 0));
    
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
                    m_cells[y][x] = CellType::Wall;
                    // Choose wall texture based on position (for variety)
                    m_wallTextures[y][x] = (x + y) % 4;
                    break;
                case '.':
                    m_cells[y][x] = CellType::Empty;
                    break;
                case 'D':
                    m_cells[y][x] = CellType::Door;
                    break;
                case 'I':
                    m_cells[y][x] = CellType::Item;
                    break;
                case 'E':
                    m_cells[y][x] = CellType::Enemy;
                    break;
                default:
                    m_cells[y][x] = CellType::Empty;
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
    
    return m_cells[y][x];
}

void Map::setCell(int x, int y, CellType type) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_cells[y][x] = type;
    }
}

bool Map::isValidPosition(double x, double y) const {
    // Convert to integer cell coordinates
    int cellX = static_cast<int>(x);
    int cellY = static_cast<int>(y);
    
    // Check bounds
    if (cellX < 0 || cellX >= m_width || cellY < 0 || cellY >= m_height) {
        return false;
    }
    
    // Get the cell type
    CellType cellType = m_cells[cellY][cellX];
    
    // Check if cell is empty, stairs, or stair steps
    return cellType == CellType::Empty || 
           cellType == CellType::Stairs || 
           cellType == CellType::StairStep1 || 
           cellType == CellType::StairStep2 || 
           cellType == CellType::StairStep3;
}

int Map::getWallTexture(int x, int y) const {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        if (m_cells[y][x] == CellType::Wall) {
            return m_wallTextures[y][x];
        }
    }
    return 0;
}

void Map::setWallTexture(int x, int y, int textureId) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return; // Out of bounds, do nothing
    }
    
    m_wallTextures[y][x] = textureId;
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

int Map::getCellElevation(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return 0; // Default elevation for out of bounds
    }
    
    return m_elevations[y][x];
}

void Map::setCellElevation(int x, int y, int elevation) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return; // Out of bounds, do nothing
    }
    
    m_elevations[y][x] = elevation;
}

float Map::getStepHeight(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return 0.0f; // Default step height for out of bounds
    }
    
    return m_stepHeights[y][x];
}

void Map::setStepHeight(int x, int y, float height) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return; // Out of bounds, do nothing
    }
    
    m_stepHeights[y][x] = height;
}

bool Map::isStairs(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return false;
    }
    
    return m_cells[y][x] == CellType::Stairs;
}

bool Map::isStairStep(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return false;
    }
    
    CellType cell = m_cells[y][x];
    return cell == CellType::StairStep1 || cell == CellType::StairStep2 || cell == CellType::StairStep3;
}

bool Map::isSolid(int x, int y) const {
    CellType cell = getCell(x, y);
    return cell == CellType::Wall || cell == CellType::Door || cell == CellType::ElevatedWall;
}

// Create sectors based on the map layout
void Map::createSectors() {
    m_sectors.clear();
    m_playerSector = 0;
    m_visibleSectors.clear();
    
    // Initialize the cell to sector mapping
    m_cellToSector.resize(m_height, std::vector<int>(m_width, -1));
    
    // For a simple implementation, we'll create sectors based on room layouts
    // This is a simplified approach - a more advanced implementation would use
    // flood fill or other algorithms to detect enclosed spaces
    
    // First, identify enclosed rooms by looking for walls
    std::vector<std::vector<bool>> visited(m_width, std::vector<bool>(m_height, false));
    int sectorId = 0;
    
    for (int x = 0; x < m_width; x++) {
        for (int y = 0; y < m_height; y++) {
            // Skip walls and already visited cells
            if (getCell(x, y) == CellType::Wall || visited[x][y]) {
                continue;
            }
            
            // Found a new potential sector
            Sector sector;
            sector.id = sectorId++;
            sector.isVisible = false;
            
            // Use flood fill to find all cells in this sector
            std::queue<std::pair<int, int>> queue;
            queue.push({x, y});
            visited[x][y] = true;
            
            std::vector<std::pair<int, int>> sectorCells;
            
            while (!queue.empty()) {
                auto [cx, cy] = queue.front();
                queue.pop();
                
                sectorCells.push_back({cx, cy});
                
                // Map this cell to the current sector
                m_cellToSector[cy][cx] = sector.id;
                
                // Check adjacent cells
                const int dx[] = {0, 1, 0, -1};
                const int dy[] = {-1, 0, 1, 0};
                
                for (int i = 0; i < 4; i++) {
                    int nx = cx + dx[i];
                    int ny = cy + dy[i];
                    
                    // Check bounds
                    if (nx < 0 || nx >= m_width || ny < 0 || ny >= m_height) {
                        continue;
                    }
                    
                    // Skip walls and visited cells
                    if (getCell(nx, ny) == CellType::Wall || visited[nx][ny]) {
                        continue;
                    }
                    
                    queue.push({nx, ny});
                    visited[nx][ny] = true;
                }
            }
            
            // Create a simplified boundary for the sector
            // For now, we'll just use the min/max coordinates to create a rectangle
            int minX = m_width, minY = m_height, maxX = 0, maxY = 0;
            
            for (const auto& cell : sectorCells) {
                minX = std::min(minX, cell.first);
                minY = std::min(minY, cell.second);
                maxX = std::max(maxX, cell.first);
                maxY = std::max(maxY, cell.second);
            }
            
            // Create vertices for the sector boundary (clockwise order)
            sector.vertices.push_back(Vec2(minX, minY));
            sector.vertices.push_back(Vec2(maxX, minY));
            sector.vertices.push_back(Vec2(maxX, maxY));
            sector.vertices.push_back(Vec2(minX, maxY));
            
            m_sectors.push_back(sector);
        }
    }
    
    // Find neighboring sectors
    for (auto& sector : m_sectors) {
        for (auto& otherSector : m_sectors) {
            if (sector.id == otherSector.id) {
                continue;
            }
            
            // Check if sectors share a boundary
            // This is a simplified approach - a more accurate approach would check
            // if any edges of the sectors are adjacent
            bool isNeighbor = false;
            
            for (const auto& v1 : sector.vertices) {
                for (const auto& v2 : otherSector.vertices) {
                    double dist = (v1 - v2).length();
                    if (dist < 2.0) {  // If vertices are close, consider them neighbors
                        isNeighbor = true;
                        break;
                    }
                }
                if (isNeighbor) break;
            }
            
            if (isNeighbor) {
                sector.neighbors.push_back(otherSector.id);
            }
        }
    }
    
    std::cout << "Created " << m_sectors.size() << " sectors for visibility culling" << std::endl;
}

// Update sector visibility based on player position
void Map::updateVisibility(const Vec2& playerPos) {
    // Reset visibility
    for (auto& sector : m_sectors) {
        sector.isVisible = false;
    }
    
    // Clear the visible sectors list
    m_visibleSectors.clear();
    
    // Find the sector containing the player
    m_playerSector = getSectorAt(playerPos.x, playerPos.y);
    
    if (m_playerSector < 0 || m_playerSector >= static_cast<int>(m_sectors.size())) {
        // Player is not in any sector, make all sectors visible
        for (size_t i = 0; i < m_sectors.size(); i++) {
            m_sectors[i].isVisible = true;
            m_visibleSectors.push_back(i);
        }
        return;
    }
    
    // Mark the player's sector as visible
    m_sectors[m_playerSector].isVisible = true;
    m_visibleSectors.push_back(m_playerSector);
    
    // Mark neighboring sectors as visible
    std::queue<int> queue;
    std::vector<bool> visited(m_sectors.size(), false);
    
    queue.push(m_playerSector);
    visited[m_playerSector] = true;
    
    // Only consider sectors up to a certain distance from the player
    const int MAX_SECTOR_DISTANCE = 2;
    int distance = 0;
    
    while (!queue.empty() && distance < MAX_SECTOR_DISTANCE) {
        int size = queue.size();
        
        for (int i = 0; i < size; i++) {
            int currentSector = queue.front();
            queue.pop();
            
            // Mark as visible
            m_sectors[currentSector].isVisible = true;
            
            // Add to visible sectors list if not already added
            if (std::find(m_visibleSectors.begin(), m_visibleSectors.end(), currentSector) == m_visibleSectors.end()) {
                m_visibleSectors.push_back(currentSector);
            }
            
            // Add neighbors to queue
            for (int neighborId : m_sectors[currentSector].neighbors) {
                if (!visited[neighborId]) {
                    queue.push(neighborId);
                    visited[neighborId] = true;
                }
            }
        }
        
        distance++;
    }
}

// Check if a sector is visible
bool Map::isSectorVisible(int sectorId) const {
    if (sectorId < 0 || sectorId >= static_cast<int>(m_sectors.size())) {
        return true;  // If sector ID is invalid, assume it's visible
    }
    
    return m_sectors[sectorId].isVisible;
}

// Get the sector at a specific position
int Map::getSectorAt(double x, double y) const {
    int cellX = static_cast<int>(x);
    int cellY = static_cast<int>(y);
    
    // Check bounds
    if (cellX < 0 || cellX >= m_width || cellY < 0 || cellY >= m_height) {
        return -1;
    }
    
    // Check if position is a wall
    if (getCell(cellX, cellY) == CellType::Wall) {
        return -1;
    }
    
    // Find the sector containing this position
    for (size_t i = 0; i < m_sectors.size(); i++) {
        const auto& sector = m_sectors[i];
        
        // Check if point is inside the sector boundary
        // Using a simple point-in-polygon test for rectangular sectors
        if (sector.vertices.size() >= 4) {
            double minX = std::min(sector.vertices[0].x, std::min(sector.vertices[1].x, 
                          std::min(sector.vertices[2].x, sector.vertices[3].x)));
            double maxX = std::max(sector.vertices[0].x, std::max(sector.vertices[1].x, 
                          std::max(sector.vertices[2].x, sector.vertices[3].x)));
            double minY = std::min(sector.vertices[0].y, std::min(sector.vertices[1].y, 
                          std::min(sector.vertices[2].y, sector.vertices[3].y)));
            double maxY = std::max(sector.vertices[0].y, std::max(sector.vertices[1].y, 
                          std::max(sector.vertices[2].y, sector.vertices[3].y)));
            
            if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                return i;
            }
        }
    }
    
    return -1;
}

// Check if two positions are in the same sector
bool Map::isInSameSector(double x1, double y1, double x2, double y2) const {
    int sector1 = getSectorAt(x1, y1);
    int sector2 = getSectorAt(x2, y2);
    
    return (sector1 >= 0 && sector1 == sector2);
}

Map::~Map() {
    // No dynamic memory to clean up
} 