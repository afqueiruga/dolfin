// Copyright (C) 2012 Chris Richardson
//
// This file is part of DOLFIN.
//
// DOLFIN is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DOLFIN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.
//
//
// First Added: 2013-01-02
// Last Changed: 2013-01-17

#ifndef __PARALLEL_REFINEMENT_H
#define __PARALLEL_REFINEMENT_H

#include <unordered_map>
#include <vector>

namespace dolfin
{

  // Forward declarations
  class Mesh;
  template<typename T> class EdgeFunction;
  template<typename T> class MeshFunction;

  class ParallelRefinement
  {
  public:

    /// ParallelRefinement encapsulates two main features:
    /// a distributed EdgeFunction, which can be updated
    /// across processes, and storage for local mesh data, 
    /// which can be used to construct the new Mesh
    ParallelRefinement(const Mesh& mesh);

    /// Destructor
    ~ParallelRefinement();

    /// Original mesh associated with this refinement
    const Mesh& mesh() const
    { return _mesh; }

    /// Return marked status of edge
    bool is_marked(std::size_t edge_index) const;

    /// Mark edge by index
    void mark(std::size_t edge_index);

    /// Mark all edges in mesh
    void mark_all();

    /// Mark all edges incident on entities indicated by refinement
    /// marker
    void mark(const MeshFunction<bool>& refinement_marker);

    /// Mark all incident edges of an entity
    void mark(const MeshEntity& cell);

    /// Return list of marked edges incident on this MeshEntity -
    /// usually a cell
    std::vector<std::size_t> marked_edge_list(const MeshEntity& cell) const;

    /// Transfer marked edges between processes
    void update_logical_edgefunction();

    /// Add new vertex for each marked edge, and create
    /// new_vertex_coordinates and global_edge->new_vertex mapping.
    /// Communicate new vertices with MPI to all affected processes.
    void create_new_vertices();

    /// Mapping of old edge (to be removed) to new global vertex
    /// number. Useful for forming new topology
    const std::map<std::size_t, std::size_t>& edge_to_new_vertex() const;

    /// Add a new cell to the list in 3D or 2D
    void new_cell(const Cell& cell);
    void new_cell(std::size_t i0, std::size_t i1, std::size_t i2,
                  std::size_t i3);
    void new_cell(std::size_t i0, std::size_t i1, std::size_t i2);
    void new_cell(const std::vector<std::size_t>& idx);

    /// Use vertex and topology data to partition new mesh across processes
    void partition(Mesh& new_mesh, bool redistribute) const;

    /// Build local mesh from internal data when not running in parallel
    void build_local(Mesh& new_mesh) const;

  private:

    // Mesh reference
    const Mesh& _mesh;

    // Shared edges between processes. In 2D, vector size is 1
    std::unordered_map<unsigned int, std::vector<std::pair<unsigned int,
      unsigned int> > > shared_edges;

    // Mapping from old local edge index to new global vertex, needed
    // to create new topology
    std::map<std::size_t, std::size_t> local_edge_to_new_vertex;

    // New storage for all coordinates when creating new vertices
    std::vector<double> new_vertex_coordinates;

    // New storage for all cells when creating new topology
    std::vector<std::size_t> new_cell_topology;

    // Management of marked edges
    std::vector<bool> marked_edges;

    // Reorder vertices into global order for partitioning
    void reorder_vertices_by_global_indices(std::vector<double>& vertex_coords,
                           const std::size_t gdim,
                           const std::vector<std::size_t>& global_indices);
  };

}

#endif
