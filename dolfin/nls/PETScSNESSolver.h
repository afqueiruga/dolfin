// Copyright (C) 2012 Patrick E. Farrell
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
// Modified by Corrado Maurini, 2013.
//
// First added:  2012-10-13
// Last changed: 2013-11-21

#ifndef __PETSC_SNES_SOLVER_H
#define __PETSC_SNES_SOLVER_H

#ifdef ENABLE_PETSC_SNES

#include <map>
#include <petscsnes.h>
#include <memory>
#include <dolfin/nls/NewtonSolver.h>
#include <dolfin/parameter/Parameters.h>
#include <dolfin/la/PETScObject.h>
#include <dolfin/la/PETScVector.h>

namespace dolfin
{

  /// Forward declarations
  class PETScVector;

  /// This class implements methods for solving nonlinear systems
  /// via PETSc's SNES interface. It includes line search and trust
  /// region techniques for globalising the convergence of the
  /// nonlinear iteration.

  class PETScSNESSolver : public PETScObject
  {
  public:

    /// Create SNES solver for a particular method
    PETScSNESSolver(std::string nls_type="default");

    /// Destructor
    virtual ~PETScSNESSolver();

    /// Solve a nonlinear variational inequality with bound constraints
    ///
    /// *Arguments*
    ///     nonlinear_function (_NonlinearProblem_)
    ///         The nonlinear problem.
    ///     x (_GenericVector_)
    ///         The vector.
    ///     lb (_GenericVector_)
    ///         The lower bound.
    ///     ub (_GenericVector_)
    ///         The upper bound.
    ///
    /// *Returns*
    ///     std::pair<std::size_t, bool>
    ///         Pair of number of Newton iterations, and whether
    ///         iteration converged)
    std::pair<std::size_t, bool> solve(NonlinearProblem& nonlinear_problem,
                                       GenericVector& x,
                                       const GenericVector& lb,
                                       const GenericVector& ub);

    /// Solve abstract nonlinear problem :math:`F(x) = 0` for given
    /// :math:`F` and Jacobian :math:`\dfrac{\partial F}{\partial x}`.
    ///
    /// *Arguments*
    ///     nonlinear_function (_NonlinearProblem_)
    ///         The nonlinear problem.
    ///     x (_GenericVector_)
    ///         The vector.
    ///
    /// *Returns*
    ///     std::pair<std::size_t, bool>
    ///         Pair of number of Newton iterations, and whether
    ///         iteration converged)
    std::pair<std::size_t, bool> solve(NonlinearProblem& nonlinear_function,
                                       GenericVector& x);

    /// Set up the SNES object, but don't do anything yet, in case the
    /// user wants to access the SNES object directly
    void init(NonlinearProblem& nonlinear_problem, GenericVector& x);

    /// Return a list of available solver methods
    static std::vector<std::pair<std::string, std::string> > methods();

    /// Default parameter values
    static Parameters default_parameters();

    Parameters parameters;

    /// Return PETSc SNES pointer
    SNES snes() const
    { return _snes; }

  private:
    struct snes_ctx_t
    {
      // Constructor
      snes_ctx_t()
      {
        x = NULL;
        nonlinear_problem = NULL;
        xl = NULL;
        xu = NULL;
        f_tmp = NULL;
      } 

      // Destructor
      ~snes_ctx_t()
      {
        if (f_tmp)
          VecDestroy(&f_tmp);
      }
      
      NonlinearProblem* nonlinear_problem;
      PETScVector* x;
      Vec f_tmp;
      const PETScVector* xl;
      const PETScVector* xu;
    };

    // PETSc solver pointer
    SNES _snes;

    // Initialize SNES solver
    void init(const std::string& method);

    // Update the linear solver parameters
    void set_linear_solver_parameters();

    // Available solvers
    static const std::map<std::string,
      std::pair<std::string, const SNESType> > _methods;

    // The callback for PETSc to compute F, the nonlinear residual
    static PetscErrorCode FormFunction(SNES snes, Vec x, Vec f, void* ctx);

    // The callback for PETSc to compute A, the Jacobian
    #if PETSC_VERSION_MAJOR == 3 && PETSC_VERSION_MINOR <= 4
    static PetscErrorCode FormJacobian(SNES snes, Vec x, Mat* A, Mat* B,
                                       MatStructure* flag, void* ctx);
    #else
    static PetscErrorCode FormJacobian(SNES snes, Vec x, Mat A, Mat B,
                                       void* ctx);
    #endif

    static PetscErrorCode FormObjective(SNES snes, Vec x, PetscReal* out, void* ctx);

    // Set the bounds on the problem from the parameters, if desired
    // Here, x is passed in as a model vector from which we make our
    // Vecs that tell PETSc the bounds if the "sign" parameter is
    // used.
    void set_bounds(GenericVector& x);

    // Check if the problem is a variational inequality
    bool is_vi() const;

    // Upper and lower bounds for bound-constrained solvers
    std::shared_ptr<const PETScVector> lb;
    std::shared_ptr<const PETScVector> ub;

    // Flag to indicate if explicit bounds are set
    bool has_explicit_bounds;

    // SNES context
    struct snes_ctx_t _snes_ctx;
  };

}

#endif

#endif
