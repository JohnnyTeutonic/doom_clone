#include "map.h"
#include "utils.h"  // Make sure we include utils.h for LineSegment, EPSILON, etc.
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

// Initialize static member
int Sector::s_nextId = 0;

// Wall implementation
Wall::Wall(const Vec2& start, const Vec2& end, int textureId, WallType type) :
    m_start(start),
    m_end(end),
    m_textureId(textureId),
    m_type(type),
    m_height(0.0),
    m_bottomOffset(0.0),
    m_adjoiningSector(nullptr)
{
}

Vec2 Wall::getNormal() const
{
    Vec2 dir = getDirection();
    // Rotate 90 degrees counterclockwise for normal pointing outward
    return Vec2(-dir.y, dir.x).normalized();
}

bool Wall::containsPoint(const Vec2& point, double tolerance) const
{
    LineSegment segment(m_start, m_end);
    return segment.containsPoint(point, tolerance);
}

double Wall::pointSide(const Vec2& point) const
{
    // Cross product to determine which side of the wall the point is on
    return (m_end.x - m_start.x) * (point.y - m_start.y) - 
           (m_end.y - m_start.y) * (point.x - m_start.x);
}

bool Wall::intersects(const LineSegment& line, Vec2& intersection) const
{
    LineSegment wallSegment(m_start, m_end);
    return lineSegmentIntersection(wallSegment, line, intersection);
}

// Sector implementation
Sector::Sector(SectorType type) :
    m_floorHeight(0.0),
    m_ceilingHeight(4.0),  // Default ceiling height
    m_floorTextureId(-1),
    m_ceilingTextureId(-1),
    m_type(type),
    m_floorType(FloorCeilingType::NORMAL),
    m_ceilingType(FloorCeilingType::NORMAL),
    m_lightLevel(1.0),     // Full brightness by default
    m_id(s_nextId++)       // Assign unique ID
{
}

void Sector::addWall(std::shared_ptr<Wall> wall)
{
    if (wall) {
        m_walls.push_back(wall);
    }
}

bool Sector::containsPoint(const Vec2& point) const
{
    // Implementation of point-in-polygon algorithm
    bool inside = false;
    
    // Loop through all walls
    for (size_t i = 0, j = m_walls.size() - 1; i < m_walls.size(); j = i++) {
        const Vec2& vi = m_walls[i]->getStart();
        const Vec2& vj = m_walls[j]->getStart();
        
        // Check if point is inside the polygon using ray casting
        if (((vi.y > point.y) != (vj.y > point.y)) &&
            (point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y) + vi.x))
        {
            inside = !inside;
        }
    }
    
    return inside;
}

Rect Sector::getBounds() const
{
    if (m_walls.empty()) {
        return Rect();
    }
    
    // Find min/max coordinates
    double minX = m_walls[0]->getStart().x;
    double minY = m_walls[0]->getStart().y;
    double maxX = minX;
    double maxY = minY;
    
    for (const auto& wall : m_walls) {
        const Vec2& start = wall->getStart();
        const Vec2& end = wall->getEnd();
        
        minX = std::min(minX, std::min(start.x, end.x));
        minY = std::min(minY, std::min(start.y, end.y));
        maxX = std::max(maxX, std::max(start.x, end.x));
        maxY = std::max(maxY, std::max(start.y, end.y));
    }
    
    return Rect(minX, minY, maxX - minX, maxY - minY);
}

// BSP Node implementation
BSPNode::BSPNode() :
    m_splitter(nullptr),
    m_frontChild(nullptr),
    m_backChild(nullptr)
{
}

BSPNode::~BSPNode()
{
    // Children are deleted by unique_ptr automatically
}

bool BSPNode::build(const std::vector<std::shared_ptr<Wall>>& walls)
{
    // If no walls, this is a leaf node
    if (walls.empty()) {
        return true;
    }
    
    // Choose a wall as splitter
    m_splitter = chooseSplitter(walls);
    if (!m_splitter) {
        // If we can't choose a good splitter, store all walls as coplanar
        m_coplanarWalls = walls;
    return true;
}

    // Partition walls into front, back, and coplanar sets
    std::vector<std::shared_ptr<Wall>> frontWalls;
    std::vector<std::shared_ptr<Wall>> backWalls;
    
    for (const auto& wall : walls) {
        // Skip the splitter itself
        if (wall == m_splitter) {
            continue;
        }
        
        // Check if wall is coplanar with the splitter
        Vec2 normalSplitter = m_splitter->getNormal();
        Vec2 normalWall = wall->getNormal();
        Vec2 dirSplitter = m_splitter->getDirection().normalized();
        Vec2 dirWall = wall->getDirection().normalized();
        
        if (approxEqual(std::abs(dirSplitter.dot(dirWall)), 1.0)) {
            // Walls are coplanar, check if they face the same direction
            if (dirSplitter.dot(dirWall) > 0) {
                // Same direction, add to coplanar
                m_coplanarWalls.push_back(wall);
            } else {
                // Opposite direction, add to coplanar
                m_coplanarWalls.push_back(wall);
            }
            continue;
        }
        
        // Find which side each vertex of the wall is on
        double startSide = m_splitter->pointSide(wall->getStart());
        double endSide = m_splitter->pointSide(wall->getEnd());
        
        if (startSide >= 0 && endSide >= 0) {
            // Wall is entirely on the front side
            frontWalls.push_back(wall);
        } else if (startSide <= 0 && endSide <= 0) {
            // Wall is entirely on the back side
            backWalls.push_back(wall);
        } else {
            // Wall spans the splitter, split it
            auto [frontPart, backPart] = splitWall(wall);
            if (frontPart) {
                frontWalls.push_back(frontPart);
            }
            if (backPart) {
                backWalls.push_back(backPart);
            }
        }
    }
    
    // Build the child nodes recursively
    if (!frontWalls.empty()) {
        m_frontChild = std::make_unique<BSPNode>();
        if (!m_frontChild->build(frontWalls)) {
            return false;
        }
    }
    
    if (!backWalls.empty()) {
        m_backChild = std::make_unique<BSPNode>();
        if (!m_backChild->build(backWalls)) {
            return false;
        }
    }
    
    return true;
}

double BSPNode::pointSide(const Vec2& point) const
{
    if (!m_splitter) {
        return 0;
    }
    
    return m_splitter->pointSide(point);
}

void BSPNode::traverse(const Vec2& viewpoint, std::vector<std::shared_ptr<Wall>>& visibleWalls) const
{
    if (!m_splitter) {
        // Leaf node, add coplanar walls
        visibleWalls.insert(visibleWalls.end(), m_coplanarWalls.begin(), m_coplanarWalls.end());
        return;
    }
    
    // Determine which side of the splitter the viewpoint is on
    double side = pointSide(viewpoint);
    
    if (side > 0) {
        // Viewpoint is on the front side
        // First draw the back side (farther)
        if (m_backChild) {
            m_backChild->traverse(viewpoint, visibleWalls);
        }
        
        // Then draw the splitter
        visibleWalls.push_back(m_splitter);
        
        // Add other coplanar walls
        visibleWalls.insert(visibleWalls.end(), m_coplanarWalls.begin(), m_coplanarWalls.end());
        
        // Finally draw the front side (closer)
        if (m_frontChild) {
            m_frontChild->traverse(viewpoint, visibleWalls);
        }
    } else {
        // Viewpoint is on the back side
        // First draw the front side (farther)
        if (m_frontChild) {
            m_frontChild->traverse(viewpoint, visibleWalls);
        }
        
        // Then draw the splitter
        visibleWalls.push_back(m_splitter);
        
        // Add other coplanar walls
        visibleWalls.insert(visibleWalls.end(), m_coplanarWalls.begin(), m_coplanarWalls.end());
        
        // Finally draw the back side (closer)
        if (m_backChild) {
            m_backChild->traverse(viewpoint, visibleWalls);
        }
    }
}

std::pair<std::shared_ptr<Wall>, std::shared_ptr<Wall>> BSPNode::splitWall(
    const std::shared_ptr<Wall>& wall) const
{
    // Default result (no split)
    std::pair<std::shared_ptr<Wall>, std::shared_ptr<Wall>> result = {nullptr, nullptr};
    
    // If no splitter, return the wall unchanged
    if (!m_splitter || !wall) {
        result.first = wall;
        return result;
    }
    
    // Get wall and splitter info
    Vec2 wallStart = wall->getStart();
    Vec2 wallEnd = wall->getEnd();
    Vec2 splitStart = m_splitter->getStart();
    Vec2 splitDir = m_splitter->getDirection();
    
    // Calculate intersection point
    double num = (wallStart.y - splitStart.y) * splitDir.x - (wallStart.x - splitStart.x) * splitDir.y;
    double den = (wallEnd.x - wallStart.x) * splitDir.y - (wallEnd.y - wallStart.y) * splitDir.x;
    
    // Check if walls are parallel
    if (std::abs(den) < EPSILON) {
        result.first = wall;
        return result;
    }
    
    // Calculate intersection parameter
    double t = num / den;
    
    // Check if intersection is within the wall segment
    if (t <= EPSILON || t >= 1.0 - EPSILON) {
        // No meaningful intersection, return the wall on the appropriate side
        double side = m_splitter->pointSide(wallStart) + m_splitter->pointSide(wallEnd);
        if (side > 0) {
            result.first = wall;
        } else {
            result.second = wall;
        }
        return result;
    }
    
    // Calculate intersection point
    Vec2 intersection = Vec2(
        wallStart.x + t * (wallEnd.x - wallStart.x),
        wallStart.y + t * (wallEnd.y - wallStart.y)
    );
    
    // Create two new walls
    result.first = std::make_shared<Wall>(
        wallStart, intersection, wall->getTextureId(), wall->getType()
    );
    result.second = std::make_shared<Wall>(
        intersection, wallEnd, wall->getTextureId(), wall->getType()
    );
    
    // Copy properties
    result.first->setHeight(wall->getHeight());
    result.first->setBottomOffset(wall->getBottomOffset());
    result.first->setAdjoiningSector(wall->getAdjoiningSector());
    
    result.second->setHeight(wall->getHeight());
    result.second->setBottomOffset(wall->getBottomOffset());
    result.second->setAdjoiningSector(wall->getAdjoiningSector());
    
    return result;
}

std::shared_ptr<Wall> BSPNode::chooseSplitter(const std::vector<std::shared_ptr<Wall>>& walls) const
{
    if (walls.empty()) {
        return nullptr;
    }
    
    // Simple heuristic: just pick the first wall
    // In a real BSP builder, you'd use a more sophisticated heuristic
    return walls[0];
}

// BSP Tree implementation
BSPTree::BSPTree() :
    m_root(nullptr)
{
}

bool BSPTree::build(const std::vector<std::shared_ptr<Sector>>& sectors)
{
    m_sectors = sectors;
    
    // Extract all walls from all sectors
    std::vector<std::shared_ptr<Wall>> walls = extractWalls(sectors);
    
    // Build the BSP tree
    m_root = std::make_unique<BSPNode>();
    return m_root->build(walls);
}

std::vector<std::shared_ptr<Wall>> BSPTree::getVisibleWalls(const Vec2& viewpoint) const
{
    std::vector<std::shared_ptr<Wall>> visibleWalls;
    
    if (m_root) {
        m_root->traverse(viewpoint, visibleWalls);
    }
    
    return visibleWalls;
}

std::shared_ptr<Sector> BSPTree::findSectorContainingPoint(const Vec2& point) const
{
    // Simple linear search through all sectors
    for (const auto& sector : m_sectors) {
        if (sector->containsPoint(point)) {
            return sector;
        }
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<Wall>> BSPTree::extractWalls(
    const std::vector<std::shared_ptr<Sector>>& sectors) const
{
    std::vector<std::shared_ptr<Wall>> walls;
    
    for (const auto& sector : sectors) {
        const auto& sectorWalls = sector->getWalls();
        walls.insert(walls.end(), sectorWalls.begin(), sectorWalls.end());
    }
    
    return walls;
}

// Map implementation
Map::Map() :
    m_bspTree(std::make_unique<BSPTree>()),
    m_textureManager(nullptr)
{
}

Map::~Map()
{
    // Clean up resources
}

bool Map::loadFromFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open map file: " << filename << std::endl;
        return false;
    }
    
    // Check file format
    char header[4];
    file.read(header, 4);
    
    if (header[0] == 'M' && header[1] == 'A' && header[2] == 'P') {
        // Version 1 format
        int version = header[3] - '0';
        if (version == 1) {
            return loadMapV1(file);
        }
    }
    
    std::cerr << "Unknown map format" << std::endl;
        return false;
    }
    
bool Map::saveToFile(const std::string& filename) const
{
    // Implement map saving (placeholder)
        return false;
    }
    
void Map::addSector(std::shared_ptr<Sector> sector)
{
    if (sector) {
            m_sectors.push_back(sector);
        }
    }
    
bool Map::buildBSPTree()
{
    return m_bspTree->build(m_sectors);
}

std::vector<std::shared_ptr<Wall>> Map::getVisibleWalls(const Vec2& viewpoint) const
{
    return m_bspTree->getVisibleWalls(viewpoint);
}

std::shared_ptr<Sector> Map::findSectorContainingPoint(const Vec2& point) const
{
    return m_bspTree->findSectorContainingPoint(point);
}

Rect Map::getBounds() const
{
    if (m_sectors.empty()) {
        return Rect();
    }
    
    // Get bounds of first sector
    Rect bounds = m_sectors[0]->getBounds();
    
    // Expand bounds to include all sectors
    for (size_t i = 1; i < m_sectors.size(); ++i) {
        Rect sectorBounds = m_sectors[i]->getBounds();
        
        // Update bounds
        double minX = std::min(bounds.x, sectorBounds.x);
        double minY = std::min(bounds.y, sectorBounds.y);
        double maxX = std::max(bounds.x + bounds.width, sectorBounds.x + sectorBounds.width);
        double maxY = std::max(bounds.y + bounds.height, sectorBounds.y + sectorBounds.height);
        
        bounds = Rect(minX, minY, maxX - minX, maxY - minY);
    }
    
    return bounds;
}

bool Map::loadMapV1(std::ifstream& file)
{
    // Read map name
    uint8_t nameLength;
    file.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
    
    if (nameLength > 0) {
        std::vector<char> nameBuffer(nameLength + 1, 0);
        file.read(nameBuffer.data(), nameLength);
        m_name = nameBuffer.data();
    }
    
    // Read sector count
    uint32_t sectorCount;
    file.read(reinterpret_cast<char*>(&sectorCount), sizeof(sectorCount));
    
    // Read sectors
    for (uint32_t i = 0; i < sectorCount; ++i) {
        auto sector = std::make_shared<Sector>();
        
        // Read sector properties
        uint32_t type;
        double floorHeight, ceilingHeight;
        uint32_t floorTexture, ceilingTexture;
        double lightLevel;
        
        file.read(reinterpret_cast<char*>(&type), sizeof(type));
        file.read(reinterpret_cast<char*>(&floorHeight), sizeof(floorHeight));
        file.read(reinterpret_cast<char*>(&ceilingHeight), sizeof(ceilingHeight));
        file.read(reinterpret_cast<char*>(&floorTexture), sizeof(floorTexture));
        file.read(reinterpret_cast<char*>(&ceilingTexture), sizeof(ceilingTexture));
        file.read(reinterpret_cast<char*>(&lightLevel), sizeof(lightLevel));
        
        sector->setType(static_cast<SectorType>(type));
        sector->setFloorHeight(floorHeight);
        sector->setCeilingHeight(ceilingHeight);
        sector->setFloorTextureId(floorTexture);
        sector->setCeilingTextureId(ceilingTexture);
        sector->setLightLevel(lightLevel);
        
        // Read wall count
        uint32_t wallCount;
        file.read(reinterpret_cast<char*>(&wallCount), sizeof(wallCount));
        
        // Read walls
        for (uint32_t j = 0; j < wallCount; ++j) {
            double startX, startY, endX, endY;
            uint32_t textureId, type;
            
            file.read(reinterpret_cast<char*>(&startX), sizeof(startX));
            file.read(reinterpret_cast<char*>(&startY), sizeof(startY));
            file.read(reinterpret_cast<char*>(&endX), sizeof(endX));
            file.read(reinterpret_cast<char*>(&endY), sizeof(endY));
            file.read(reinterpret_cast<char*>(&textureId), sizeof(textureId));
            file.read(reinterpret_cast<char*>(&type), sizeof(type));
            
            auto wall = std::make_shared<Wall>(
                Vec2(startX, startY),
                Vec2(endX, endY),
                textureId,
                static_cast<WallType>(type)
            );
            
            // Read optional properties
            if (wall->getType() == WallType::PORTAL) {
                uint32_t adjoiningSectorIndex;
                file.read(reinterpret_cast<char*>(&adjoiningSectorIndex), sizeof(adjoiningSectorIndex));
                
                // Store adjoining sector index to resolve later
                // In a real implementation, we'd need to resolve these after loading all sectors
            }
            
            sector->addWall(wall);
        }
        
        addSector(sector);
    }
    
    // Build BSP tree
    buildBSPTree();
    
    return true;
}

std::vector<Wall> Map::generateDoomMap(const Vec2& playerPosition) const
{
    // Create a proper DOOM-like map layout
    std::vector<Wall> walls;
    
    // Player starting position is the reference point
    Vec2 pos = playerPosition;
    
    // Create a series of interconnected rooms with more complex architecture
    
    // ========================
    // STARTING ROOM (Central)
    // ========================
    
    // Define the central room where player starts - INCREASED SIZE
    double roomSize = 24.0;  // Increased from 12.0 (doubled again)
    double wallHeight = 8.0;  // Doubled
    
    // Starting room walls (slightly offset from player start position)
    // Use texture 1 (metal panels) for the starting room
    walls.emplace_back(Vec2(pos.x - roomSize, pos.y - roomSize), Vec2(pos.x + roomSize, pos.y - roomSize), 1); // North wall
    walls.emplace_back(Vec2(pos.x + roomSize, pos.y - roomSize), Vec2(pos.x + roomSize, pos.y + roomSize), 1); // East wall
    walls.emplace_back(Vec2(pos.x + roomSize, pos.y + roomSize), Vec2(pos.x - roomSize, pos.y + roomSize), 1); // South wall
    walls.emplace_back(Vec2(pos.x - roomSize, pos.y + roomSize), Vec2(pos.x - roomSize, pos.y - roomSize), 1); // West wall
    
    // ========================
    // NORTH CORRIDOR & ROOMS
    // ========================
    
    // North corridor - WIDENED
    double corridorWidth = 10.0;  // Increased from 5.0 (doubled)
    walls.emplace_back(Vec2(pos.x - corridorWidth, pos.y - roomSize), Vec2(pos.x - corridorWidth, pos.y - roomSize - 16), 0); // Left wall
    walls.emplace_back(Vec2(pos.x + corridorWidth, pos.y - roomSize), Vec2(pos.x + corridorWidth, pos.y - roomSize - 16), 0); // Right wall
    
    // North room (stone texture with blood - texture 0)
    double northRoomY = pos.y - roomSize - 32;  // Further away for better visibility (doubled)
    
    // Create north room with a circular design (octagonal approximation)
    double northRadius = roomSize + 8;  // Larger radius (doubled)
    int numSegments = 8;
    Vec2 northCenter(pos.x, northRoomY);
    
    for (int i = 0; i < numSegments; i++) {
        double angle1 = i * 2.0 * 3.14159265358979323846 / numSegments;
        double angle2 = ((i + 1) % numSegments) * 2.0 * 3.14159265358979323846 / numSegments;
        
        Vec2 p1 = northCenter + Vec2(northRadius * cos(angle1), northRadius * sin(angle1));
        Vec2 p2 = northCenter + Vec2(northRadius * cos(angle2), northRadius * sin(angle2));
        
        // Skip the segment where the corridor connects
        if (!(i == 6 || i == 7)) {
            walls.emplace_back(p1, p2, 0);
        }
    }
    
    // Connect corridor to the octagonal room
    walls.emplace_back(
        Vec2(pos.x - corridorWidth, pos.y - roomSize - 16),
        Vec2(northCenter.x - northRadius * cos(7 * 2.0 * 3.14159265358979323846 / numSegments), 
             northCenter.y - northRadius * sin(7 * 2.0 * 3.14159265358979323846 / numSegments)), 0);
             
    walls.emplace_back(
        Vec2(pos.x + corridorWidth, pos.y - roomSize - 16),
        Vec2(northCenter.x - northRadius * cos(6 * 2.0 * 3.14159265358979323846 / numSegments), 
             northCenter.y - northRadius * sin(6 * 2.0 * 3.14159265358979323846 / numSegments)), 0);
    
    // Add pillars in north room (arranged in a circle) - MADE FEWER AND SMALLER
    double pillarSize = 1.6;  // Larger pillars (doubled)
    double pillarRadius = northRadius * 0.6;
    int numPillars = 4;  // Fewer pillars
    
    for (int i = 0; i < numPillars; i++) {
        double angle = i * 2.0 * 3.14159265358979323846 / numPillars;
        Vec2 pillarCenter = northCenter + Vec2(pillarRadius * cos(angle), pillarRadius * sin(angle));
        
        walls.emplace_back(Vec2(pillarCenter.x - pillarSize, pillarCenter.y - pillarSize), 
                            Vec2(pillarCenter.x + pillarSize, pillarCenter.y - pillarSize), 3);
        walls.emplace_back(Vec2(pillarCenter.x + pillarSize, pillarCenter.y - pillarSize), 
                            Vec2(pillarCenter.x + pillarSize, pillarCenter.y + pillarSize), 3);
        walls.emplace_back(Vec2(pillarCenter.x + pillarSize, pillarCenter.y + pillarSize), 
                            Vec2(pillarCenter.x - pillarSize, pillarCenter.y + pillarSize), 3);
        walls.emplace_back(Vec2(pillarCenter.x - pillarSize, pillarCenter.y + pillarSize), 
                            Vec2(pillarCenter.x - pillarSize, pillarCenter.y - pillarSize), 3);
    }
    
    // ========================
    // EAST CORRIDOR & ROOM
    // ========================
    
    // East corridor - WIDENED
    walls.emplace_back(Vec2(pos.x + roomSize, pos.y - corridorWidth), Vec2(pos.x + roomSize + 16, pos.y - corridorWidth), 2); // North wall
    walls.emplace_back(Vec2(pos.x + roomSize, pos.y + corridorWidth), Vec2(pos.x + roomSize + 16, pos.y + corridorWidth), 2); // South wall
    
    // East room (flesh texture - texture 2) - maze-like structure with WIDER PATHS
    double eastRoomX = pos.x + roomSize + 32;  // Further away for better visibility (doubled)
    double mazeSize = roomSize + 8;  // Larger maze (doubled)
    
    // Main room boundaries
    walls.emplace_back(Vec2(eastRoomX - mazeSize, pos.y - mazeSize), Vec2(eastRoomX + mazeSize, pos.y - mazeSize), 2); // North wall
    walls.emplace_back(Vec2(eastRoomX + mazeSize, pos.y - mazeSize), Vec2(eastRoomX + mazeSize, pos.y + mazeSize), 2); // East wall
    walls.emplace_back(Vec2(eastRoomX + mazeSize, pos.y + mazeSize), Vec2(eastRoomX - mazeSize, pos.y + mazeSize), 2); // South wall
    walls.emplace_back(Vec2(eastRoomX - mazeSize, pos.y + mazeSize), Vec2(eastRoomX - mazeSize, pos.y + corridorWidth), 2); // West wall segment
    walls.emplace_back(Vec2(eastRoomX - mazeSize, pos.y - corridorWidth), Vec2(eastRoomX - mazeSize, pos.y - mazeSize), 2); // West wall segment
    
    // Connect corridor to main room
    walls.emplace_back(Vec2(pos.x + roomSize + 16, pos.y - corridorWidth), Vec2(eastRoomX - mazeSize, pos.y - corridorWidth), 2);
    walls.emplace_back(Vec2(pos.x + roomSize + 16, pos.y + corridorWidth), Vec2(eastRoomX - mazeSize, pos.y + corridorWidth), 2);
    
    // Create a SIMPLIFIED maze-like structure with wider paths
    double mazeUnit = mazeSize / 2.5;  // Wider units
    
    // Horizontal maze segments - FEWER WALLS
    walls.emplace_back(Vec2(eastRoomX - mazeSize + mazeUnit, pos.y - mazeSize + mazeUnit), 
                        Vec2(eastRoomX + mazeSize - mazeUnit, pos.y - mazeSize + mazeUnit), 2);
                        
    walls.emplace_back(Vec2(eastRoomX, pos.y), 
                        Vec2(eastRoomX + mazeSize - mazeUnit, pos.y), 2);
    
    // Vertical maze segments - FEWER WALLS
    walls.emplace_back(Vec2(eastRoomX - mazeSize + mazeUnit, pos.y - mazeSize), 
                        Vec2(eastRoomX - mazeSize + mazeUnit, pos.y), 2);
                        
    walls.emplace_back(Vec2(eastRoomX + mazeSize - mazeUnit, pos.y - mazeSize), 
                        Vec2(eastRoomX + mazeSize - mazeUnit, pos.y + mazeSize), 2);
    
    // ========================
    // SOUTH CORRIDOR & ROOM
    // ========================
    
    // South corridor - WIDENED
    walls.emplace_back(Vec2(pos.x - corridorWidth, pos.y + roomSize), Vec2(pos.x - corridorWidth, pos.y + roomSize + 16), 3); // Left wall
    walls.emplace_back(Vec2(pos.x + corridorWidth, pos.y + roomSize), Vec2(pos.x + corridorWidth, pos.y + roomSize + 16), 3); // Right wall
    
    // South room (hellish metal with runes - texture 3) - create a star chamber
    double southRoomY = pos.y + roomSize + 32;  // Further away for better visibility (doubled)
    double starRadius = roomSize + 8;  // Larger radius (doubled)
    Vec2 southCenter(pos.x, southRoomY);
    
    // Create star-shaped room with 5 points
    int numPoints = 5;
    double innerRadius = starRadius * 0.6;  // Less dramatic star points for easier navigation
    
    std::vector<Vec2> starPoints;
    for (int i = 0; i < numPoints * 2; i++) {
        double angle = i * 3.14159265358979323846 / numPoints;
        double radius = (i % 2 == 0) ? starRadius : innerRadius;
        starPoints.push_back(southCenter + Vec2(radius * cos(angle), radius * sin(angle)));
    }
    
    // Create star walls, but leave an opening for the corridor
    for (int i = 0; i < numPoints * 2; i++) {
        int nextIdx = (i + 1) % (numPoints * 2);
        
        // Skip the segment where the corridor connects (top of the star)
        if (!(i == 9 && nextIdx == 0) && !(i == 0 && nextIdx == 1)) {
            walls.emplace_back(starPoints[i], starPoints[nextIdx], 3);
        }
    }
    
    // Connect corridor to star room
    walls.emplace_back(Vec2(pos.x - corridorWidth, pos.y + roomSize + 16), starPoints[9], 3);
    walls.emplace_back(Vec2(pos.x + corridorWidth, pos.y + roomSize + 16), starPoints[0], 3);
    
    // Add a pentagram in the center of the south room
    double pentRadius = starRadius * 0.4;  // Larger pentagram
    for (int i = 0; i < 5; i++) {
        double angle1 = i * 2.0 * 3.14159265358979323846 / 5.0;
        double angle2 = ((i + 2) % 5) * 2.0 * 3.14159265358979323846 / 5.0;
        
        Vec2 p1(southCenter.x + pentRadius * cos(angle1), southCenter.y + pentRadius * sin(angle1));
        Vec2 p2(southCenter.x + pentRadius * cos(angle2), southCenter.y + pentRadius * sin(angle2));
        
        walls.emplace_back(p1, p2, 3);
    }
    
    // ========================
    // WEST CORRIDOR & ROOM
    // ========================
    
    // West corridor - WIDENED
    walls.emplace_back(Vec2(pos.x - roomSize, pos.y - corridorWidth), Vec2(pos.x - roomSize - 16, pos.y - corridorWidth), 1); // North wall
    walls.emplace_back(Vec2(pos.x - roomSize, pos.y + corridorWidth), Vec2(pos.x - roomSize - 16, pos.y + corridorWidth), 1); // South wall
    
    // West room (metal panels - texture 1) - create a room with columns but FEWER and SMALLER
    double westRoomX = pos.x - roomSize - 32;  // Further away for better visibility (doubled)
    double westRoomSize = roomSize + 8;  // Larger room (doubled)
    
    // Main room boundaries
    walls.emplace_back(Vec2(westRoomX - westRoomSize, pos.y - westRoomSize), Vec2(westRoomX + westRoomSize, pos.y - westRoomSize), 1); // North wall
    walls.emplace_back(Vec2(westRoomX + westRoomSize, pos.y - westRoomSize), Vec2(westRoomX + westRoomSize, pos.y - corridorWidth), 1); // East wall segment
    walls.emplace_back(Vec2(westRoomX + westRoomSize, pos.y + corridorWidth), Vec2(westRoomX + westRoomSize, pos.y + westRoomSize), 1); // East wall segment
    walls.emplace_back(Vec2(westRoomX + westRoomSize, pos.y + westRoomSize), Vec2(westRoomX - westRoomSize, pos.y + westRoomSize), 1); // South wall
    walls.emplace_back(Vec2(westRoomX - westRoomSize, pos.y + westRoomSize), Vec2(westRoomX - westRoomSize, pos.y - westRoomSize), 1); // West wall
    
    // Connect corridor to main room
    walls.emplace_back(Vec2(pos.x - roomSize - 16, pos.y - corridorWidth), Vec2(westRoomX + westRoomSize, pos.y - corridorWidth), 1);
    walls.emplace_back(Vec2(pos.x - roomSize - 16, pos.y + corridorWidth), Vec2(westRoomX + westRoomSize, pos.y + corridorWidth), 1);
    
    // Create FEWER columns in a grid pattern
    int numRows = 2;
    int numCols = 2;
    double columnSize = 1.6;  // Larger columns (doubled)
    double spaceX = westRoomSize * 1.8 / (numCols + 1);
    double spaceY = westRoomSize * 1.8 / (numRows + 1);
    
    for (int row = 0; row < numRows; row++) {
        for (int col = 0; col < numCols; col++) {
            // Calculate column center
            Vec2 columnCenter(
                westRoomX - westRoomSize + spaceX * (col + 1),
                pos.y - westRoomSize + spaceY * (row + 1)
            );
            
            // Draw column (4 walls making a square)
            walls.emplace_back(Vec2(columnCenter.x - columnSize, columnCenter.y - columnSize), 
                                Vec2(columnCenter.x + columnSize, columnCenter.y - columnSize), 1);
            walls.emplace_back(Vec2(columnCenter.x + columnSize, columnCenter.y - columnSize), 
                                Vec2(columnCenter.x + columnSize, columnCenter.y + columnSize), 1);
            walls.emplace_back(Vec2(columnCenter.x + columnSize, columnCenter.y + columnSize), 
                                Vec2(columnCenter.x - columnSize, columnCenter.y + columnSize), 1);
            walls.emplace_back(Vec2(columnCenter.x - columnSize, columnCenter.y + columnSize), 
                                Vec2(columnCenter.x - columnSize, columnCenter.y - columnSize), 1);
        }
    }
    
    // ========================
    // ADDITIONAL DETAILS FOR CENTRAL ROOM - SIMPLIFIED FOR BETTER NAVIGATION
    // ========================
    
    // Create a more visible central platform in the starting room
    double platformSize = 6.0;  // Larger platform (doubled)
    Vec2 platCenter = pos;
    
    walls.emplace_back(Vec2(platCenter.x - platformSize, platCenter.y - platformSize), 
                        Vec2(platCenter.x + platformSize, platCenter.y - platformSize), 0);
    walls.emplace_back(Vec2(platCenter.x + platformSize, platCenter.y - platformSize), 
                        Vec2(platCenter.x + platformSize, platCenter.y + platformSize), 0);
    walls.emplace_back(Vec2(platCenter.x + platformSize, platCenter.y + platformSize), 
                        Vec2(platCenter.x - platformSize, platCenter.y + platformSize), 0);
    walls.emplace_back(Vec2(platCenter.x - platformSize, platCenter.y + platformSize), 
                        Vec2(platCenter.x - platformSize, platCenter.y - platformSize), 0);
    
    // Add visible markers (simple short walls) pointing to each corridor
    double markerSize = 4.0;  // Doubled
    double markerDistance = roomSize * 0.6;
    
    // North marker (pointing to north corridor)
    walls.emplace_back(Vec2(pos.x - markerSize, pos.y - markerDistance), 
                       Vec2(pos.x, pos.y - markerDistance - markerSize), 2);
    walls.emplace_back(Vec2(pos.x, pos.y - markerDistance - markerSize), 
                       Vec2(pos.x + markerSize, pos.y - markerDistance), 2);
    
    // East marker (pointing to east corridor)
    walls.emplace_back(Vec2(pos.x + markerDistance, pos.y - markerSize), 
                       Vec2(pos.x + markerDistance + markerSize, pos.y), 2);
    walls.emplace_back(Vec2(pos.x + markerDistance + markerSize, pos.y), 
                       Vec2(pos.x + markerDistance, pos.y + markerSize), 2);
    
    // South marker (pointing to south corridor)
    walls.emplace_back(Vec2(pos.x - markerSize, pos.y + markerDistance), 
                       Vec2(pos.x, pos.y + markerDistance + markerSize), 2);
    walls.emplace_back(Vec2(pos.x, pos.y + markerDistance + markerSize), 
                       Vec2(pos.x + markerSize, pos.y + markerDistance), 2);
    
    // West marker (pointing to west corridor)
    walls.emplace_back(Vec2(pos.x - markerDistance, pos.y - markerSize), 
                       Vec2(pos.x - markerDistance - markerSize, pos.y), 2);
    walls.emplace_back(Vec2(pos.x - markerDistance - markerSize, pos.y), 
                       Vec2(pos.x - markerDistance, pos.y + markerSize), 2);
    
    return walls;
} 