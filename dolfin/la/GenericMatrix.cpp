// Copyright (C) 2010-2012 Anders Logg
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
// Modified by Mikael Mortensen 2011
//
// First added:  2010-02-23
// Last changed: 2013-02-19

#include <dolfin/common/constants.h>
#include <dolfin/common/Timer.h>
#include "GenericSparsityPattern.h"
#include "GenericLinearAlgebraFactory.h"
#include "TensorLayout.h"
#include "GenericMatrix.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
void GenericMatrix::ident_zeros()
{
  // Check size of system
  if (size(0) != size(1))
  {
    dolfin_error("GenericMatrix.cpp",
                 "ident_zeros",
                 "Matrix is not square");
  }

  std::vector<std::size_t> columns;
  std::vector<double> values;
  std::vector<dolfin::la_index> zero_rows;
  const std::pair<std::size_t, std::size_t> row_range = local_range(0);
  const std::size_t m = row_range.second - row_range.first;

  // Check which rows are zero
  for (std::size_t row = 0; row < m; row++)
  {
    // Get global row number
    int global_row = row + row_range.first;

    // Get value for row
    getrow(global_row, columns, values);

    // Get maximum value in row
    double max = 0.0;
    for (std::size_t k = 0; k < values.size(); k++)
      max = std::max(max, std::abs(values[k]));

    // Check if row is zero
    if (max < DOLFIN_EPS)
      zero_rows.push_back(global_row);
  }

  // Write a message
  log(TRACE, "Found %d zero row(s), inserting ones on the diagonal.",
      zero_rows.size());

  // Insert one on the diagonal for rows with only zeros.
  ident(zero_rows.size(), zero_rows.data());

  // Apply changes
  apply("insert");
}
//-----------------------------------------------------------------------------
void GenericMatrix::compressed(GenericMatrix& B) const
{
  Timer timer("Compress matrix");

  // Create new layout
  std::shared_ptr<TensorLayout>
    new_layout = B.factory().create_layout(2);
  dolfin_assert(new_layout);

  // Pointer to sparsity pattern
  GenericSparsityPattern* new_sparsity_pattern
    = new_layout->sparsity_pattern().get();

  // Retrieve global and local matrix info
  std::vector<std::size_t> global_dimensions(2);
  global_dimensions[0] = size(0);
  global_dimensions[1] = size(1);
  std::vector<std::pair<std::size_t, std::size_t> > local_range(2);
  local_range[0] = this->local_range(0);
  local_range[1] = this->local_range(0);

  // With the row-by-row algorithm used here there is no need for
  // inserting non_local rows and as such we can simply use a dummy
  // for off_process_owner
  std::vector<const std::vector<std::size_t>* > local_to_global(2);
  std::vector<const std::vector<int>* > off_process_owner(2);
  const std::vector<int> dummy0;
  off_process_owner[0] = &dummy0;
  off_process_owner[1] = &dummy0;
  const std::vector<std::size_t> dummy1;
  local_to_global[0] = &dummy1;
  local_to_global[1] = &dummy1;
  const std::pair<std::size_t, std::size_t> row_range = local_range[0];
  const std::size_t m = row_range.second - row_range.first;

  // Initialize layout
  new_layout->init(MPI_COMM_WORLD, global_dimensions, 1, local_range);

  // Initialize sparsity pattern
  if (new_sparsity_pattern)
  {
    new_sparsity_pattern->init(MPI_COMM_WORLD, global_dimensions, local_range,
                               local_to_global, off_process_owner, 1);
  }

  // Declare some variables used to extract matrix information
  std::vector<std::size_t> columns;
  std::vector<double> values;

  // Hold all values of local matrix
  std::vector<double> allvalues;

  // Hold column id for all values of local matrix
  std::vector<dolfin::la_index> allcolumns;

  // Hold accumulated number of cols on local matrix
  std::vector<dolfin::la_index> offset(m + 1);

  offset[0] = 0;
  std::vector<dolfin::la_index> thisrow(1);
  std::vector<dolfin::la_index> thiscolumn;
  std::vector<const std::vector<dolfin::la_index>* > dofs(2);
  dofs[0] = &thisrow;
  dofs[1] = &thiscolumn;

  // Iterate over rows
  for (std::size_t i = 0; i < m; i++)
  {
    // Get row and locate nonzeros. Store non-zero values and columns
    // for later
    const std::size_t global_row = i + row_range.first;
    getrow(global_row, columns, values);
    std::size_t count = 0;
    thiscolumn.clear();
    for (std::size_t j = 0; j < columns.size(); j++)
    {
      // Store if non-zero or diagonal entry. PETSc solvers require this
      if (std::abs(values[j]) > DOLFIN_EPS || columns[j] == global_row)
      {
        thiscolumn.push_back(columns[j]);
        allvalues.push_back(values[j]);
        allcolumns.push_back(columns[j]);
        count++;
      }
    }

    thisrow[0] = global_row;
    offset[i + 1] = offset[i] + count;

    // Build new compressed sparsity pattern
    if (new_sparsity_pattern)
      new_sparsity_pattern->insert_global(dofs);
  }

  // Finalize sparsity pattern
  if (new_sparsity_pattern)
    new_sparsity_pattern->apply();

  // Create matrix with the new layout
  B.init(*new_layout);

  // Put the values back into new compressed matrix
  for (std::size_t i = 0; i < m; i++)
  {
    const dolfin::la_index global_row = i + row_range.first;
    B.set(&allvalues[offset[i]], 1, &global_row,
        offset[i+1] - offset[i], &allcolumns[offset[i]]);
  }
  B.apply("insert");
}
//-----------------------------------------------------------------------------
