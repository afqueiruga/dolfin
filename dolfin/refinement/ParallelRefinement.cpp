// Copyright (C) 2013 Chris Richardson
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
// Last Changed: 2013-05-12

#include <map>
#include <unordered_map>
#include <vector>
#include <boost/multi_array.hpp>
#include <dolfin/common/MPI.h>
#include <dolfin/common/Timer.h>
#include <dolfin/common/types.h>
#include <dolfin/mesh/Cell.h>
#include <dolfin/mesh/DistributedMeshTools.h>
#include <dolfin/mesh/Edge.h>
#include <dolfin/mesh/LocalMeshData.h>
#include <dolfin/mesh/Mesh.h>
#include <dolfin/mesh/MeshEditor.h>
#include <dolfin/mesh/MeshEntityIterator.h>
#include <dolfin/mesh/MeshPartitioning.h>
#include <dolfin/mesh/Vertex.h>

#include "ParallelRefinement.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
ParallelRefinement::ParallelRefinement(const Mesh& mesh) : _mesh(mesh),
  shared_edges(DistributedMeshTools::compute_shared_entities(_mesh, 1)),
  marked_edges(mesh.num_edges(), false)
{
  // Do nothing
}
//-----------------------------------------------------------------------------
ParallelRefinement::~ParallelRefinement()
{
  // Do nothing
}
//-----------------------------------------------------------------------------
bool ParallelRefinement::is_marked(std::size_t edge_index) const
{
  dolfin_assert(edge_index < _mesh.num_edges());
  return marked_edges[edge_index];
}
//-----------------------------------------------------------------------------
void ParallelRefinement::mark(std::size_t edge_index)
{
  dolfin_assert(edge_index < _mesh.num_edges());
  marked_edges[edge_index] = true;
}
//-----------------------------------------------------------------------------
void ParallelRefinement::mark_all()
{
  marked_edges.assign(_mesh.num_edges(), true);
}
//-----------------------------------------------------------------------------
const std::map<std::size_t, std::size_t>&
ParallelRefinement::edge_to_new_vertex() const
{
  return local_edge_to_new_vertex;
}
//-----------------------------------------------------------------------------
void ParallelRefinement::mark(const MeshEntity& cell)
{
  for (EdgeIterator edge(cell); !edge.end(); ++edge)
    marked_edges[edge->index()] = true;
}
//-----------------------------------------------------------------------------
void ParallelRefinement::mark(const MeshFunction<bool>& refinement_marker)
{
  // Special case for EdgeFunction because EdgeIterator(Edge) will get all
  // connected edge-edge entities otherwise
  if (refinement_marker.dim() == 1)
  {
    for (EdgeIterator e(_mesh); !e.end(); ++e)
      marked_edges[e->index()] = refinement_marker[*e];
    return;
  }

  for (MeshEntityIterator cell(_mesh, refinement_marker.dim()); !cell.end();
       ++cell)
  {
    if (refinement_marker[*cell])
    {
      for (EdgeIterator edge(*cell); !edge.end(); ++edge)
        marked_edges[edge->index()] = true;
    }
  }
}
//-----------------------------------------------------------------------------
std::vector<std::size_t> ParallelRefinement::marked_edge_list(const MeshEntity& cell) const
{
  std::vector<std::size_t> result;

  std::size_t i = 0;
  for (EdgeIterator edge(cell); !edge.end(); ++edge)
  {
    if (marked_edges[edge->index()])
      result.push_back(i);
    ++i;
  }
  return result;
}
//-----------------------------------------------------------------------------
void ParallelRefinement::update_logical_edgefunction()
{
  const std::size_t num_processes = MPI::size(_mesh.mpi_comm());

  // Create a list of edges on this process that are 'true' and copy
  // to remote sharing processes
  std::vector<std::vector<std::size_t> > values_to_send(num_processes);
  for (auto sh_edge = shared_edges.begin(); sh_edge != shared_edges.end();
       sh_edge++)
  {
    const std::size_t local_index = sh_edge->first;
    if (marked_edges[local_index] == true)
    {
      for (auto proc_edge = sh_edge->second.begin();
           proc_edge != sh_edge->second.end();
           ++proc_edge)
      {
        values_to_send[proc_edge->first].push_back(proc_edge->second);
      }
    }
  }

  std::vector<std::vector<std::size_t> > received_values;
  MPI::all_to_all(_mesh.mpi_comm(), values_to_send, received_values);

  // Flatten received values and set EdgeFunction true at each index
  // received
  for (auto r = received_values.begin();
       r != received_values.end();
       ++r)
  {
    for (auto local_index = r->begin();
         local_index != r->end();
         ++local_index)
    {
      marked_edges[*local_index] = true;
    }
  }
}
//-----------------------------------------------------------------------------
void ParallelRefinement::create_new_vertices()
{
  // Take marked_edges and use to create new vertices

  const std::size_t num_processes = MPI::size(_mesh.mpi_comm());
  const std::size_t process_number = MPI::rank(_mesh.mpi_comm());

  // Copy over existing mesh vertices
  new_vertex_coordinates = _mesh.coordinates();

  // Tally up unshared marked edges, and shared marked edges which are
  // owned on this process.  Index them sequentially from zero.
  const std::size_t gdim = _mesh.geometry().dim();
  std::size_t n = 0;
  for (std::size_t local_i = 0 ; local_i < _mesh.num_edges(); ++local_i)
  {
    if (marked_edges[local_i] == true)
    {
      // Assume this edge is owned locally
      bool owner = true;

      // If shared, check that this is true
      auto shared_edge_i = shared_edges.find(local_i);
      if (shared_edge_i != shared_edges.end())
      {
        // check if any other sharing process has a lower rank
        for (auto proc_edge = shared_edge_i->second.begin();
             proc_edge != shared_edge_i->second.end();
             ++proc_edge)
        {
          if (proc_edge->first < process_number)
            owner = false;
        }
      }

      // If it is still believed to be owned on this process, add to
      // list
      if (owner)
      {
        const Point& midpoint = Edge(_mesh, local_i).midpoint();
        for (std::size_t j = 0; j < gdim; ++j)
          new_vertex_coordinates.push_back(midpoint[j]);
        local_edge_to_new_vertex[local_i] = n++;
      }
    }
  }

  // Calculate global range for new local vertices
  const std::size_t num_new_vertices = n;
  const std::size_t global_offset
    = MPI::global_offset(_mesh.mpi_comm(), num_new_vertices, true)
    + _mesh.size_global(0);

  // If they are shared, then the new global vertex index needs to be
  // sent off-process.  Add offset to map, and collect up any shared
  // new vertices that need to send the new index off-process
  std::vector<std::vector<std::size_t> > values_to_send(num_processes);
  for (auto local_edge = local_edge_to_new_vertex.begin();
       local_edge != local_edge_to_new_vertex.end();
       ++local_edge)
  {
    // Add global_offset to map, to get new global index of new
    // vertices
    local_edge->second += global_offset;

    const std::size_t local_i = local_edge->first;
    //shared, but locally owned : remote owned are not in list.
    auto shared_edge_i = shared_edges.find(local_i);
    if (shared_edge_i != shared_edges.end())
    {
      for (auto remote_process_edge = shared_edges[local_i].begin();
           remote_process_edge != shared_edges[local_i].end();
           ++remote_process_edge)
      {
        const std::size_t remote_proc_num = remote_process_edge->first;
        // send mapping from remote local edge index to new global vertex index
        values_to_send[remote_proc_num].push_back(remote_process_edge->second);
        values_to_send[remote_proc_num].push_back(local_edge->second);
      }
    }
  }

  // Send new vertex indices to remote processes and receive
  std::vector<std::vector<std::size_t> > received_values(num_processes);
  MPI::all_to_all(_mesh.mpi_comm(), values_to_send, received_values);

  // Flatten and add received remote global vertex indices to map
  for (auto p = received_values.begin(); p != received_values.end(); ++p)
    for (auto q = p->begin(); q != p->end(); q += 2)
      local_edge_to_new_vertex[*q] = *(q + 1);

  // Attach global indices to each vertex, old and new, and sort
  // them across processes into this order

  std::vector<std::size_t> global_indices(_mesh.topology().global_indices(0));
  for (std::size_t i = 0; i < num_new_vertices; i++)
    global_indices.push_back(i + global_offset);

  reorder_vertices_by_global_indices(new_vertex_coordinates,
                                     _mesh.geometry().dim(), global_indices);
}
//-----------------------------------------------------------------------------
void ParallelRefinement::reorder_vertices_by_global_indices(
                                 std::vector<double>& vertex_coords,
                                 const std::size_t gdim,
                                 const std::vector<std::size_t>& global_indices)
{
  // This is needed to interface with MeshPartitioning/LocalMeshData,
  // which expects the vertices in global order.  This is inefficient,
  // and needs to be addressed in MeshPartitioning.cpp where they are
  // redistributed again.

  Timer t("Parallel Refine: reorder vertices");
  // FIXME: be more efficient with MPI

  dolfin_assert(gdim*global_indices.size() == vertex_coords.size());

  boost::multi_array_ref<double, 2> vertex_array(vertex_coords.data(),
                      boost::extents[vertex_coords.size()/gdim][gdim]);

  // Calculate size of overall global vector by finding max index value
  // anywhere
  const std::size_t global_vector_size
    = MPI::max(_mesh.mpi_comm(), *std::max_element(global_indices.begin(),
                                                   global_indices.end())) + 1;

  // Send unwanted values off process
  const std::size_t num_processes = MPI::size(_mesh.mpi_comm());
  std::vector<std::vector<std::size_t> > values_to_send0(num_processes);
  std::vector<std::vector<double> > values_to_send1(num_processes);

  // Go through local vector and append value to the appropriate list
  // to send to correct process
  for (std::size_t i = 0; i < vertex_array.shape()[0] ; ++i)
  {
    const std::size_t global_i = global_indices[i];
    const std::size_t process_i
      = MPI::index_owner(_mesh.mpi_comm(), global_i, global_vector_size);
    values_to_send0[process_i].push_back(global_i);
    values_to_send0[process_i].push_back(vertex_array[i].shape()[0]);
    values_to_send0[process_i].push_back(values_to_send1[process_i].size());
    values_to_send1[process_i].insert(values_to_send1[process_i].end(),
                                      vertex_array[i].begin(),
                                      vertex_array[i].end());
  }

  // Redistribute the values to the appropriate process - including
  // self All values are "in the air" at this point, so local vector
  // can be cleared
  std::vector<std::vector<std::size_t> > received_values0;
  std::vector<std::vector<double> > received_values1;
  MPI::all_to_all(_mesh.mpi_comm(), values_to_send0, received_values0);
  MPI::all_to_all(_mesh.mpi_comm(), values_to_send1, received_values1);

  // When receiving, just go through all received values and place
  // them in the local partition of the global vector.
  const std::pair<std::size_t, std::size_t> range
    = MPI::local_range(_mesh.mpi_comm(), global_vector_size);
  vertex_coords.resize((range.second - range.first)*gdim);
  boost::multi_array_ref<double, 2>
    new_vertex_array(vertex_coords.data(),
                     boost::extents[range.second - range.first][gdim]);
  for (std::size_t p = 0; p < received_values0.size(); ++p)
  {
    const std::vector<std::size_t>& received_global_data0 = received_values0[p];
    const std::vector<double>& received_global_data1 = received_values1[p];
    for (std::size_t j = 0; j < received_global_data0.size(); j += 3)
    {
      const std::size_t global_i = received_global_data0[j];
      const std::size_t num_vals = received_global_data0[j + 1];
      const std::size_t offset = received_global_data0[j + 2];
      dolfin_assert(global_i >= range.first && global_i < range.second);
      std::copy(received_global_data1.begin() + offset,
                received_global_data1.begin() + offset + num_vals,
                new_vertex_array[global_i - range.first].begin());
    }
  }
}
//-----------------------------------------------------------------------------
void ParallelRefinement::build_local(Mesh& new_mesh) const
{
  MeshEditor ed;
  const std::size_t tdim = _mesh.topology().dim();
  const std::size_t gdim = _mesh.geometry().dim();
  dolfin_assert(new_vertex_coordinates.size()%gdim == 0);
  const std::size_t num_vertices = new_vertex_coordinates.size()/gdim;

  const std::size_t num_cell_vertices = tdim + 1;
  dolfin_assert(new_cell_topology.size()%num_cell_vertices == 0);
  const std::size_t num_cells = new_cell_topology.size()/num_cell_vertices;

  ed.open(new_mesh, tdim, gdim);
  ed.init_vertices(num_vertices);
  std::size_t i = 0;
  for (auto p = new_vertex_coordinates.begin(); p != new_vertex_coordinates.end();
       p += gdim)
  {
    std::vector<double> vertex(p, p + gdim);
    ed.add_vertex(i, vertex);
    ++i;
  }

  ed.init_cells(num_cells);
  i = 0;
  std::vector<std::size_t> cell(num_cell_vertices);
  for (auto p = new_cell_topology.begin(); p != new_cell_topology.end();
       p += num_cell_vertices)
  {
    std::copy(p, p + num_cell_vertices, cell.begin());
    ed.add_cell(i, cell);
    ++i;
  }
  ed.close();

}
//-----------------------------------------------------------------------------
void ParallelRefinement::partition(Mesh& new_mesh, bool redistribute) const
{
  LocalMeshData mesh_data(new_mesh.mpi_comm());
  mesh_data.tdim = _mesh.topology().dim();
  const std::size_t gdim = _mesh.geometry().dim();
  mesh_data.gdim = gdim;
  mesh_data.num_vertices_per_cell = mesh_data.tdim + 1;

  // Copy data to LocalMeshData structures
  const std::size_t num_local_cells
    = new_cell_topology.size()/mesh_data.num_vertices_per_cell;
  mesh_data.num_global_cells = MPI::sum(_mesh.mpi_comm(), num_local_cells);
  mesh_data.global_cell_indices.resize(num_local_cells);
  const std::size_t idx_global_offset
    = MPI::global_offset(_mesh.mpi_comm(), num_local_cells, true);
  for (std::size_t i = 0; i < num_local_cells ; i++)
    mesh_data.global_cell_indices[i] = idx_global_offset + i;

  mesh_data.cell_vertices.resize(boost::extents[num_local_cells][mesh_data.num_vertices_per_cell]);
  std::copy(new_cell_topology.begin(), new_cell_topology.end(),
            mesh_data.cell_vertices.data());

  const std::size_t num_local_vertices = new_vertex_coordinates.size()/gdim;
  mesh_data.num_global_vertices = MPI::sum(_mesh.mpi_comm(),
                                           num_local_vertices);
  mesh_data.vertex_coordinates.resize(boost::extents[num_local_vertices][gdim]);
  std::copy(new_vertex_coordinates.begin(), new_vertex_coordinates.end(),
            mesh_data.vertex_coordinates.data());

  mesh_data.vertex_indices.resize(num_local_vertices);
  const std::size_t vertex_global_offset
    = MPI::global_offset(_mesh.mpi_comm(), num_local_vertices, true);
  for (std::size_t i = 0; i < num_local_vertices ; i++)
    mesh_data.vertex_indices[i] = vertex_global_offset + i;

  if (!redistribute)
  {
    // FIXME: broken by ghost mesh
    // Set owning process rank to this process rank
    mesh_data.cell_partition.assign(mesh_data.global_cell_indices.size(),
                                    MPI::rank(_mesh.mpi_comm()));
  }

  MeshPartitioning::build_distributed_mesh(new_mesh, mesh_data);
}
//-----------------------------------------------------------------------------
void ParallelRefinement::new_cell(const Cell& cell)
{
  for (VertexIterator v(cell); !v.end(); ++v)
    new_cell_topology.push_back(v->global_index());
}
//-----------------------------------------------------------------------------
void ParallelRefinement::new_cell(const std::size_t i0, const std::size_t i1,
                                  const std::size_t i2, const std::size_t i3)
{
  new_cell_topology.push_back(i0);
  new_cell_topology.push_back(i1);
  new_cell_topology.push_back(i2);
  new_cell_topology.push_back(i3);
}
//-----------------------------------------------------------------------------
void ParallelRefinement::new_cell(const std::size_t i0, const std::size_t i1,
                                  const std::size_t i2)
{
  new_cell_topology.push_back(i0);
  new_cell_topology.push_back(i1);
  new_cell_topology.push_back(i2);
}
//-----------------------------------------------------------------------------
void ParallelRefinement::new_cell(const std::vector<std::size_t>& idx)
{
  new_cell_topology.insert(new_cell_topology.end(), idx.begin(), idx.end());
}
//-----------------------------------------------------------------------------
