/* Copyright (c) 2014. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "surf_routing_cluster.hpp"

#ifndef SURF_ROUTING_CLUSTER_FAT_TREE_HPP_
#define SURF_ROUTING_CLUSTER_FAT_TREE_HPP_


/* The class AsClusterFatTree describes PGFT, as introduced by Eitan Zahavi
 * in "D-Mod-K Routing Providing Non-Blocking Traffic for Shift Permutations
 * on Real Life Fat Trees" (2010). RLFT are PGFT with some restrictions to 
 * address real world constraints, which are not currently enforced (but it 
 * should certainly be checked for)
 */


class FatTreeNode {
public:
  int id; // ID as given by the user, should be unique
  int level; // The 0th level represents the leafs of the PGFT
  int position; // Position in the level
  
  /* We can see the sizes sum of the two following vectors as the device 
   * ports number. If we use the notations used in Zahavi's paper, 
   * children.size() = m_level and parents.size() = w_(level+1)
   * 
   */
  std::vector<FatTreeNode*> children;  // m, apply from lvl 0 to levels - 1 
  std::vector<FatTreeNode*> parents; // w, apply from lvl 1 to levels
  FatTreeNode(int id, int level=-1, int position=-1);
};

class FatTreeLink {
public:
  unsigned int ports;
  std::vector<s_sg_platf_link_cbarg_t> linksUp; // From source to destination
  std::vector<s_sg_platf_link_cbarg_t> linksDown; // From destination to source
  /* As it is symetric, it might as well be first / second instead
   * of source / destination
   */
  FatTreeNode *source; 
  FatTreeNode *destination;
  //FatTreeLink(FatTreeNode *source, FatTreeNode *destination, unsigned int ports = 0);
};

class AsClusterFatTree : public AsCluster {
public:
  AsClusterFatTree();
  ~AsClusterFatTree();
  virtual void getRouteAndLatency(RoutingEdgePtr src, RoutingEdgePtr dst,
                                  sg_platf_route_cbarg_t into,
                                  double *latency) const;
  // virtual void getRouteAndLatency(const int src, const int dst,
  //                                 std::vector<NetworkLink> *route,
  //                                 double *latency) const;
  virtual void create_links(sg_platf_cluster_cbarg_t cluster);
  void parse_specific_arguments(sg_platf_cluster_cbarg_t cluster);
  void addNodes(std::vector<int> const& id);
  void generateDotFile(const string& filename = "fatTree.dot") const;

protected:
  //description of a PGFT (TODO : better doc)
  int levels;
  std::vector<int> lowerLevelNodesNumber;
  std::vector<int> upperLevelNodesNumber;
  std::vector<int> lowerLevelPortsNumber;
  
  std::vector<FatTreeNode*> nodes;
  std::map<std::pair<int,int>, FatTreeLink*> links;
  std::vector<int> nodesByLevel;

  void addLink(FatTreeNode *parent, FatTreeNode *child);
  void getLevelPosition(const int level, int &position, int &size);
};
#endif