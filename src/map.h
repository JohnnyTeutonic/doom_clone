#ifndef MAP_H
#define MAP_H

#include "utils.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <utility>
#include <optional>

// Forward declarations
class BSPNode;
class Renderer;
class Engine;
class BSPTree;
class Sector;
class Wall;
class TextureManager;

// Vertex - a point in 2D space
struct Vertex {
    double x, y;
    
    Vertex() : x(0), y(0) {}
    Vertex(double _x, double _y) : x(_x), y(_y) {}
    
    // Convert to Vec2
    Vec2 toVec2() const { return Vec2(x, y); }
    
    // Create from Vec2
    static Vertex fromVec2(const Vec2& vec) { return Vertex(vec.x, vec.y); }
};

// Define missing types
struct Thing {
    double x, y;        // Position
    double angle;       // Direction angle in radians
    int type;           // Type of thing
    uint32_t flags;     // BitFlags
    int id;             // Unique ID
    
    Thing() : x(0), y(0), angle(0), type(0), flags(0), id(0) {}
    Thing(double _x, double _y, double _angle, int _type) 
        : x(_x), y(_y), angle(_angle), type(_type), flags(0), id(0) {}
};

struct SubSector {
    int sectorId;               // The sector this subsector belongs to
    std::vector<int> lineIds;   // Indices of lines in this subsector
};

// LineDefFlags - special properties for line segments
enum class LineDefFlags : uint32_t {
    NONE = 0,
    BLOCKS_PLAYER = 1 << 0,
    BLOCKS_MONSTERS = 1 << 1,
    TWO_SIDED = 1 << 2,
    UPPER_UNPEGGED = 1 << 3,
    LOWER_UNPEGGED = 1 << 4,
    SECRET = 1 << 5,
    BLOCKS_SOUND = 1 << 6,
    NEVER_AUTOMAP = 1 << 7,
    ALWAYS_AUTOMAP = 1 << 8
};

// LineDef - a line segment with properties
struct LineDef {
    int startVertex; // Index into vertex array
    int endVertex;   // Index into vertex array
    int frontSector; // Index to sector on front side (-1 if none)
    int backSector;  // Index to sector on back side (-1 if none)
    
    // Textured surfaces on this line
    int frontUpperTexture;
    int frontMiddleTexture; // For single-sided walls
    int frontLowerTexture;
    
    int backUpperTexture;
    int backMiddleTexture;
    int backLowerTexture;
    
    uint32_t flags;  // BitFlags for line properties
    int specialType; // Special action type
    
    LineDef() : 
        startVertex(-1), 
        endVertex(-1), 
        frontSector(-1), 
        backSector(-1),
        frontUpperTexture(-1),
        frontMiddleTexture(-1),
        frontLowerTexture(-1),
        backUpperTexture(-1),
        backMiddleTexture(-1),
        backLowerTexture(-1),
        flags(0),
        specialType(0) {}
    
    // Check if line is two-sided (portal between sectors)
    bool isTwoSided() const { 
        return frontSector != -1 && backSector != -1; 
    }
    
    // Check if line blocks movement
    bool blocksMovement() const {
        return (flags & static_cast<uint32_t>(LineDefFlags::BLOCKS_PLAYER)) != 0;
    }
    
    // Add a flag
    void addFlag(LineDefFlags flag) {
        flags |= static_cast<uint32_t>(flag);
    }
    
    // Check if line has a specific flag
    bool hasFlag(LineDefFlags flag) const {
        return (flags & static_cast<uint32_t>(flag)) != 0;
    }
};

// Enums
enum class WallType {
    NORMAL,         // Standard wall
    PORTAL,         // Portal to another sector (door, window)
    TRANSPARENT,    // Transparent wall (glass)
    ANIMATED        // Animated wall
};

enum class SectorType {
    NORMAL,         // Standard sector
    DOOR,           // Door sector
    ELEVATOR,       // Elevator sector
    DAMAGE,         // Damage sector (lava, etc.)
    SECRET          // Secret sector
};

enum class FloorCeilingType {
    NORMAL,         // Standard floor/ceiling
    ANIMATED,       // Animated texture
    SKY,            // Sky texture (ceiling only)
    WATER,          // Water surface
    TRANSPARENT     // Transparent (can see through to other sectors)
};

// Wall represents a line segment in the map
class Wall {
public:
    Wall(const Vec2& start, const Vec2& end, int textureId = -1, WallType type = WallType::NORMAL);
    
    // Getters
    const Vec2& getStart() const { return m_start; }
    const Vec2& getEnd() const { return m_end; }
    Vec2 getDirection() const { return m_end - m_start; }
    Vec2 getNormal() const;
    double getLength() const { return (m_end - m_start).length(); }
    int getTextureId() const { return m_textureId; }
    WallType getType() const { return m_type; }
    double getHeight() const { return m_height; }
    double getBottomOffset() const { return m_bottomOffset; }
    Sector* getAdjoiningSector() const { return m_adjoiningSector; }
    bool isPortal() const { return m_type == WallType::PORTAL; }
    bool isTransparent() const { return m_type == WallType::TRANSPARENT || isPortal(); }
    
    // Setters
    void setTextureId(int id) { m_textureId = id; }
    void setType(WallType type) { m_type = type; }
    void setHeight(double height) { m_height = height; }
    void setBottomOffset(double offset) { m_bottomOffset = offset; }
    void setAdjoiningSector(Sector* sector) { m_adjoiningSector = sector; }
    
    // Check if point is on the wall
    bool containsPoint(const Vec2& point, double tolerance = EPSILON) const;
    
    // Check which side of the wall a point is on
    // Returns > 0 for front side, < 0 for back side, 0 for on the wall
    double pointSide(const Vec2& point) const;
    
    // Check if a line segment intersects with this wall
    bool intersects(const LineSegment& line, Vec2& intersection) const;
    
private:
    Vec2 m_start;                   // Start point
    Vec2 m_end;                     // End point
    int m_textureId;                // Texture ID
    WallType m_type;                // Wall type
    double m_height;                // Wall height (if different from sector)
    double m_bottomOffset;          // Bottom offset from sector floor
    Sector* m_adjoiningSector;      // Adjoining sector (for portals)
};

// Sector represents a convex or concave polygon in the map
class Sector {
public:
    Sector(SectorType type = SectorType::NORMAL);
    
    // Add a wall to the sector
    void addWall(std::shared_ptr<Wall> wall);
    
    // Getters
    const std::vector<std::shared_ptr<Wall>>& getWalls() const { return m_walls; }
    double getFloorHeight() const { return m_floorHeight; }
    double getCeilingHeight() const { return m_ceilingHeight; }
    int getFloorTextureId() const { return m_floorTextureId; }
    int getCeilingTextureId() const { return m_ceilingTextureId; }
    SectorType getType() const { return m_type; }
    FloorCeilingType getFloorType() const { return m_floorType; }
    FloorCeilingType getCeilingType() const { return m_ceilingType; }
    double getLightLevel() const { return m_lightLevel; }
    int getId() const { return m_id; }
    
    // Setters
    void setFloorHeight(double height) { m_floorHeight = height; }
    void setCeilingHeight(double height) { m_ceilingHeight = height; }
    void setFloorTextureId(int id) { m_floorTextureId = id; }
    void setCeilingTextureId(int id) { m_ceilingTextureId = id; }
    void setType(SectorType type) { m_type = type; }
    void setFloorType(FloorCeilingType type) { m_floorType = type; }
    void setCeilingType(FloorCeilingType type) { m_ceilingType = type; }
    void setLightLevel(double level) { m_lightLevel = clamp(level, 0.0, 1.0); }
    
    // Check if point is inside the sector
    bool containsPoint(const Vec2& point) const;
    
    // Get sector bounds
    Rect getBounds() const;
    
private:
    std::vector<std::shared_ptr<Wall>> m_walls;     // Walls defining the sector
    double m_floorHeight;                           // Floor height
    double m_ceilingHeight;                         // Ceiling height
    int m_floorTextureId;                           // Floor texture ID
    int m_ceilingTextureId;                         // Ceiling texture ID
    SectorType m_type;                              // Sector type
    FloorCeilingType m_floorType;                   // Floor type
    FloorCeilingType m_ceilingType;                 // Ceiling type
    double m_lightLevel;                            // Light level (0.0 - 1.0)
    int m_id;                                       // Unique sector ID
    
    static int s_nextId;                            // Next available sector ID
};

// BSP Node for the BSP tree
class BSPNode {
public:
    BSPNode();
    ~BSPNode();
    
    // Build node from a list of walls
    bool build(const std::vector<std::shared_ptr<Wall>>& walls);
    
    // Check which side of the splitting plane a point is on
    // Returns > 0 for front side, < 0 for back side, 0 for on the plane
    double pointSide(const Vec2& point) const;
    
    // Traverse the BSP tree and collect visible walls
    void traverse(const Vec2& viewpoint, std::vector<std::shared_ptr<Wall>>& visibleWalls) const;
    
private:
    std::shared_ptr<Wall> m_splitter;                // Wall used as splitter
    std::unique_ptr<BSPNode> m_frontChild;           // Front child node
    std::unique_ptr<BSPNode> m_backChild;            // Back child node
    std::vector<std::shared_ptr<Wall>> m_coplanarWalls; // Walls coplanar with the splitter
    
    // Helper function to split a wall against the splitter
    std::pair<std::shared_ptr<Wall>, std::shared_ptr<Wall>> splitWall(
        const std::shared_ptr<Wall>& wall) const;
    
    // Helper function to choose a good splitter
    std::shared_ptr<Wall> chooseSplitter(const std::vector<std::shared_ptr<Wall>>& walls) const;
};

// BSP Tree for efficient rendering and collision detection
class BSPTree {
public:
    BSPTree();
    
    // Build the BSP tree from a list of sectors
    bool build(const std::vector<std::shared_ptr<Sector>>& sectors);
    
    // Get visible walls from a viewpoint
    std::vector<std::shared_ptr<Wall>> getVisibleWalls(const Vec2& viewpoint) const;
    
    // Find the sector containing a point
    std::shared_ptr<Sector> findSectorContainingPoint(const Vec2& point) const;
    
private:
    std::unique_ptr<BSPNode> m_root;                  // Root node
    std::vector<std::shared_ptr<Sector>> m_sectors;   // All sectors in the map
    
    // Helper function to extract all walls from sectors
    std::vector<std::shared_ptr<Wall>> extractWalls(
        const std::vector<std::shared_ptr<Sector>>& sectors) const;
};

// Map represents the entire game map
class Map {
public:
    Map();
    ~Map();
    
    // Load map from file
    bool loadFromFile(const std::string& filename);
    
    // Save map to file
    bool saveToFile(const std::string& filename) const;
    
    // Add a sector to the map
    void addSector(std::shared_ptr<Sector> sector);
    
    // Get all sectors
    const std::vector<std::shared_ptr<Sector>>& getSectors() const { return m_sectors; }
    
    // Build the BSP tree
    bool buildBSPTree();
    
    // Get visible walls from a viewpoint
    std::vector<std::shared_ptr<Wall>> getVisibleWalls(const Vec2& viewpoint) const;
    
    // Find the sector containing a point
    std::shared_ptr<Sector> findSectorContainingPoint(const Vec2& point) const;
    
    // Set texture manager
    void setTextureManager(TextureManager* textureManager) { m_textureManager = textureManager; }
    
    // Get texture manager
    TextureManager* getTextureManager() const { return m_textureManager; }
    
    // Get map name
    const std::string& getName() const { return m_name; }
    
    // Set map name
    void setName(const std::string& name) { m_name = name; }
    
    // Get map bounds
    Rect getBounds() const;
    
private:
    std::string m_name;                               // Map name
    std::vector<std::shared_ptr<Sector>> m_sectors;   // All sectors in the map
    std::unique_ptr<BSPTree> m_bspTree;               // BSP tree for rendering
    TextureManager* m_textureManager;                 // Texture manager
    
    // Helper functions for loading/saving
    bool loadMapV1(std::ifstream& file);  // Load version 1 map format
};

#endif // MAP_H 