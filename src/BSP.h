#ifndef BSP_H
#define BSP_H

#include "Common.h"
#include <vector>
#include <memory>

// Forward declaration
class Map;

// BSP Node - internal node in the BSP tree
class BSPNode {
public:
    // Constructors
    BSPNode();
    ~BSPNode();
    
    // Partition line (not a wall, just used for splitting)
    double partitionX1, partitionY1;
    double partitionX2, partitionY2;
    
    // Child nodes (nullptr for leaf nodes)
    std::unique_ptr<BSPNode> frontChild;
    std::unique_ptr<BSPNode> backChild;
    
    // For leaf nodes only:
    bool isLeaf;
    int leafId;  // Index into SubSector array
    
    // Bounding box for this node and its children
    Rect boundingBox;
    
    // Determine which side of the partition line a point is on
    Side pointOnSide(double x, double y) const;
    
    // Determine which side of the partition line a line is on
    Side lineOnSide(double x1, double y1, double x2, double y2) const;
    
    // Check if a ray intersects this node
    bool isRayIntersectingBox(const Vec2& start, const Vec2& dir) const;
    
    // Does this node potentially contain a point
    bool containsPoint(double x, double y) const;
};

// BSP Builder - creates a BSP tree from a set of lines
class BSPBuilder {
public:
    BSPBuilder();
    ~BSPBuilder();
    
    // Build a BSP tree from a map
    std::unique_ptr<BSPNode> buildTree(const Map& map);
    
private:
    // Internal methods
    std::unique_ptr<BSPNode> buildTreeRecursive(
        const std::vector<int>& lineIndices, 
        const Map& map, 
        int depth);
        
    // Find the best partition line from a set of lines
    int findBestSplitter(const std::vector<int>& lineIndices, const Map& map);
    
    // Split a set of lines by a partition line
    void splitLines(
        const std::vector<int>& lineIndices,
        int splitterIndex,
        const Map& map,
        std::vector<int>& frontLines,
        std::vector<int>& backLines);
        
    // Calculate the cost of using a line as a splitter
    float calculateSplitCost(
        int lineIndex, 
        const std::vector<int>& lineIndices, 
        const Map& map);
        
    // Create a bounding box for a set of lines
    Rect calculateBoundingBox(
        const std::vector<int>& lineIndices,
        const Map& map);
        
    // Get the line endpoints
    void getLineEndpoints(
        int lineIndex, 
        const Map& map, 
        double& x1, double& y1, double& x2, double& y2);
};

#endif // BSP_H 