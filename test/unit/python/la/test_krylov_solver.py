#!/usr/bin/env py.test

"""Unit tests for the KrylovSolver interface"""

# Copyright (C) 2012 Johan Hake
#
# This file is part of DOLFIN.
#
# DOLFIN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# DOLFIN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.
#
# Modified by Anders Logg 2012
#
# First added:  2012-02-21
# Last changed: 2014-05-30

from dolfin import *
import pytest
from dolfin_utils.test import skip_if_not_PETSc

@skip_if_not_PETSc
def test_krylov_solver():
    "Test PETScKrylovSolver"

    # Assemble system
    mesh = UnitSquareMesh(32, 32)
    V = FunctionSpace(mesh, 'CG', 1)
    bc = DirichletBC(V, Constant(0.0), lambda x, on_boundary: on_boundary)
    u = TrialFunction(V)
    v = TestFunction(V)
    u1 = Function(V)

    # Forms
    a, L = inner(grad(u), grad(v))*dx, Constant(1.0)*v*dx
    a_L = inner(grad(u1), grad(v))*dx

    # Assemble linear algebra objects
    A = assemble(a)
    b = assemble(L)
    bc.apply(A, b)

    # Get solution vector
    tmp = Function(V)
    #x = tmp.vector()

    # Solve first using direct solver
    #solve(A, x, b, "lu")

    #direct_norm = x.norm("l2")

    # Get solution vector
    #x_petsc = as_backend_type(x)

    # With simple interface
    #solver = PETScKrylovSolver("gmres", prec)
    #solver.solve(A, x_petsc, as_backend_type(b))
    #assert round(x_petsc.norm("l2") - direct_norm, 5) == 0

    # With PETScPreconditioner interface
    #solver = PETScKrylovSolver("gmres", PETScPreconditioner(prec))
    #solver.solve(A, x_petsc, as_backend_type(b))
    #assert round(x_petsc.norm("l2") - direct_norm, 5) == 0
