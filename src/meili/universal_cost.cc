#include <valhalla/midgard/logging.h>
#include <valhalla/sif/dynamiccost.h>

#include "meili/universal_cost.h"

namespace valhalla {

namespace meili {


class UniversalCost : public sif::DynamicCost {
 public:
  UniversalCost(const boost::property_tree::ptree& pt)
      : DynamicCost(pt, kUniversalTravelMode) {}

  bool Allowed(const baldr::DirectedEdge* edge,
               const sif::EdgeLabel& pred,
               const baldr::GraphTile*& tile,
               const baldr::GraphId& edgeid) const
  {
    // Disable transit lines
    if (edge->IsTransitLine()) {
      return false;
    }
    return true;
  }

  bool AllowedReverse(const baldr::DirectedEdge* edge,
                      const sif::EdgeLabel& pred,
                      const baldr::DirectedEdge* opp_edge,
                      const baldr::GraphTile*& tile,
                      const baldr::GraphId& edgeid) const
  { return true; }

  bool Allowed(const baldr::NodeInfo* node) const
  { return true; }

  sif::Cost EdgeCost(const baldr::DirectedEdge* edge,
                     const uint32_t density) const
  {
    float length = edge->length();
    return { length, length };
  }

  // Disable astar
  float AStarCostFactor() const
  { return 0.f; }

  virtual const sif::EdgeFilter GetEdgeFilter() const {
    //throw back a lambda that checks the access for this type of costing
    return [](const baldr::DirectedEdge* edge){
      // Disable transit lines
      return edge->IsTransitLine();
    };
  }

  virtual const sif::NodeFilter GetNodeFilter() const {
    //throw back a lambda that checks the access for this type of costing
    return [](const baldr::NodeInfo* node){
      // Do not filter any nodes
      return false;
    };
  }
};


sif::cost_ptr_t CreateUniversalCost(const boost::property_tree::ptree& config)
{ return std::make_shared<UniversalCost>(config); }

}

}
