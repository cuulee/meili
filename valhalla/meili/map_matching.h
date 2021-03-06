// -*- mode: c++ -*-
#ifndef MMP_MAP_MATCHING_H_
#define MMP_MAP_MATCHING_H_
#include <algorithm>

#include <valhalla/midgard/pointll.h>
#include <valhalla/baldr/pathlocation.h>
#include <valhalla/sif/edgelabel.h>
#include <valhalla/sif/costconstants.h>

#include <valhalla/meili/candidate.h>
#include <valhalla/meili/measurement.h>
#include <valhalla/meili/candidate_search.h>
#include <valhalla/meili/viterbi_search.h>
#include <valhalla/meili/routing.h>
#include <valhalla/meili/match_result.h>


namespace valhalla{
namespace meili {


class State
{
 public:
  State(const StateId id,
        const Time time,
        const Candidate& candidate);

  StateId id() const
  { return id_; }

  Time time() const
  { return time_; }

  const Candidate& candidate() const
  { return candidate_; }

  bool routed() const
  { return labelset_ != nullptr; }

  void route(const std::vector<const State*>& states,
             baldr::GraphReader& graphreader,
             float max_route_distance,
             const midgard::DistanceApproximator& approximator,
             float search_radius,
             sif::cost_ptr_t costing,
             std::shared_ptr<const sif::EdgeLabel> edgelabel,
             const float turn_cost_table[181]) const;

  const Label* last_label(const State& state) const;

  RoutePathIterator RouteBegin(const State& state) const
  {
    const auto it = label_idx_.find(state.id());
    if (it != label_idx_.end()) {
      return RoutePathIterator(labelset_.get(), it->second);
    }
    return RoutePathIterator(labelset_.get());
  }

  RoutePathIterator RouteEnd() const
  { return RoutePathIterator(labelset_.get()); }

 private:
  const StateId id_;

  const Time time_;

  const Candidate candidate_;

  mutable std::shared_ptr<LabelSet> labelset_;

  mutable std::unordered_map<StateId, uint32_t> label_idx_;
};


class MapMatching: public ViterbiSearch<State>
{
 public:
  MapMatching(baldr::GraphReader& graphreader,
              const sif::cost_ptr_t* mode_costing,
              const sif::TravelMode mode,
              float sigma_z,
              float beta,
              float breakage_distance,
              float max_route_distance_factor,
              float turn_penalty_factor);

  MapMatching(baldr::GraphReader& graphreader,
              const sif::cost_ptr_t* mode_costing,
              const sif::TravelMode mode,
              const boost::property_tree::ptree& config);

  virtual ~MapMatching();

  void Clear();

  baldr::GraphReader& graphreader() const
  { return graphreader_; }

  sif::cost_ptr_t costing() const
  { return mode_costing_[static_cast<size_t>(mode_)]; }

  const std::vector<const State*>&
  states(Time time) const
  { return states_[time]; }

  const Measurement& measurement(Time time) const
  { return measurements_[time]; }

  const Measurement& measurement(const State& state) const
  { return measurements_[state.time()]; }

  std::vector<Measurement>::size_type size() const
  { return measurements_.size(); }

  template <typename candidate_iterator_t>
  Time AppendState(const Measurement& measurement,
                   candidate_iterator_t begin,
                   candidate_iterator_t end);

 protected:
  virtual float MaxRouteDistance(const State& left, const State& right) const;

  float TransitionCost(const State& left, const State& right) const override;

  float EmissionCost(const State& state) const override;

  double CostSofar(double prev_costsofar, float transition_cost, float emission_cost) const override;

 private:

  baldr::GraphReader& graphreader_;

  const sif::cost_ptr_t* mode_costing_;

  const sif::TravelMode mode_;

  std::vector<Measurement> measurements_;

  std::vector<std::vector<const State*>> states_;

  float sigma_z_;
  double inv_double_sq_sigma_z_;  // equals to 1.f / (sigma_z_ * sigma_z_ * 2.f)

  float beta_;
  float inv_beta_;  // equals to 1.f / beta_

  float breakage_distance_;

  float max_route_distance_factor_;

  float turn_penalty_factor_;

  // Cost for each degree in [0, 180]
  float turn_cost_table_[181];
};


struct EdgeSegment
{
  EdgeSegment(baldr::GraphId the_edgeid,
              float the_source = 0.f,
              float the_target = 1.f);

  std::vector<midgard::PointLL> Shape(baldr::GraphReader& graphreader) const;

  bool Adjoined(baldr::GraphReader& graphreader, const EdgeSegment& other) const;

  // TODO make them private
  baldr::GraphId edgeid;

  float source;

  float target;
};


// A facade that connects everything
class MapMatcher final
{
 public:
  MapMatcher(const boost::property_tree::ptree&,
             baldr::GraphReader&,
             CandidateGridQuery&,
             const sif::cost_ptr_t*,
             sif::TravelMode);

  ~MapMatcher();

  baldr::GraphReader& graphreader()
  { return graphreader_; }

  const CandidateQuery& rangequery()
  { return rangequery_; }

  sif::TravelMode travelmode() const
  { return travelmode_; }

  const boost::property_tree::ptree& config() const
  { return config_; }

  const MapMatching& mapmatching() const
  { return mapmatching_; }

  std::vector<MatchResult>
  OfflineMatch(const std::vector<Measurement>&);

 private:
  boost::property_tree::ptree config_;

  baldr::GraphReader& graphreader_;

  CandidateGridQuery& rangequery_;

  const sif::cost_ptr_t* mode_costing_;

  sif::TravelMode travelmode_;

  MapMatching mapmatching_;
};


constexpr size_t kModeCostingCount = 8;


class MapMatcherFactory final
{
public:
  MapMatcherFactory(const boost::property_tree::ptree&);

  ~MapMatcherFactory();

  baldr::GraphReader& graphreader()
  { return graphreader_; }

  CandidateQuery& rangequery()
  { return rangequery_; }

  sif::TravelMode NameToTravelMode(const std::string&);

  const std::string& TravelModeToName(sif::TravelMode);

  MapMatcher* Create(sif::TravelMode travelmode)
  { return Create(travelmode, boost::property_tree::ptree()); }

  MapMatcher* Create(const std::string& name)
  { return Create(NameToTravelMode(name), boost::property_tree::ptree()); }

  MapMatcher* Create(const std::string& name,
                     const boost::property_tree::ptree& preferences)
  { return Create(NameToTravelMode(name), preferences); }

  MapMatcher* Create(const boost::property_tree::ptree&);

  MapMatcher* Create(sif::TravelMode, const boost::property_tree::ptree&);

  boost::property_tree::ptree
  MergeConfig(const std::string&, const boost::property_tree::ptree&);

  boost::property_tree::ptree&
  MergeConfig(const std::string&, boost::property_tree::ptree&);

  void ClearFullCache();

  void ClearCache();

private:
  typedef sif::cost_ptr_t (*factory_function_t)(const boost::property_tree::ptree&);

  boost::property_tree::ptree config_;

  baldr::GraphReader graphreader_;

  sif::cost_ptr_t mode_costing_[kModeCostingCount];

  std::string mode_name_[kModeCostingCount];

  CandidateGridQuery rangequery_;

  float max_grid_cache_size_;

  size_t register_costing(const std::string&, factory_function_t, const boost::property_tree::ptree&);

  sif::cost_ptr_t* init_costings(const boost::property_tree::ptree&);
};


template <typename segment_iterator_t>
std::string
RouteToString(baldr::GraphReader& graphreader,
              segment_iterator_t segment_begin,
              segment_iterator_t segment_end,
              const baldr::GraphTile*& tile);

// Validate a route. It check if all edge segments of the route are
// valid and successive, and no loop
template <typename segment_iterator_t>
bool ValidateRoute(baldr::GraphReader& graphreader,
                   segment_iterator_t segment_begin,
                   segment_iterator_t segment_end,
                   const baldr::GraphTile*& tile);

template <typename segment_iterator_t>
void MergeRoute(std::vector<EdgeSegment>& route,
                segment_iterator_t segment_begin,
                segment_iterator_t segment_end);

template <typename match_iterator_t>
std::vector<EdgeSegment>
ConstructRoute(baldr::GraphReader& graphreader,
               const MapMatcher& mapmatcher,
               match_iterator_t begin,
               match_iterator_t end);

inline float local_tile_size(const baldr::GraphReader& graphreader)
{
  const auto& tile_hierarchy = graphreader.GetTileHierarchy();
  const auto& tiles = tile_hierarchy.levels().rbegin()->second.tiles;
  return tiles.TileSize();
}

}
}
#endif // MMP_MAP_MATCHING_H_
