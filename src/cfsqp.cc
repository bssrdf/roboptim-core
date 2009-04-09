// Copyright (C) 2009 by Thomas Moulard, FIXME.
//
// This file is part of the liboptimization.
//
// liboptimization is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// liboptimization is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with liboptimization.  If not, see <http://www.gnu.org/licenses/>.


/**
 * \brief Implementation of the CFSQP module.
 */
#include <limits>
#include <cfsqpusr.h>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "liboptimization/cfsqp.hh"
#include "liboptimization/function.hh"
#include "liboptimization/util.hh"

namespace optimization
{
  typedef void (*fct_t) (int, int, double*, double*, void*);
  typedef void (*grad_t) (int, int, double*, double*, fct_t, void*);

  namespace detail
  {
    /// \internal
    /// Evaluate a constraint.
    class EvalConstraintVisitor : public boost::static_visitor<Function::value_type>
    {
    public:
      explicit EvalConstraintVisitor (const Function::vector_t& x, int j)
        : x_ (x),
          j_ (j)
      {}

      template <typename C>
      Function::value_type
      operator () (const C* c)
      {
        BOOST_STATIC_ASSERT((boost::is_convertible<C*, Function*>::value));

        Function::value_type res = (*c) (x_);

        if (j_ % 2 == 0)
          // g(x) >= b, -g(x) + b <= 0
          res = -res + c->bound.first;
        else
          // g(x) <= b, g(x) - b <= 0
          res = res - c->bound.second;
        return res;
      }

    private:
      const Function::vector_t& x_;
      int j_;
    };

    /// \internal
    /// Evaluate a constraint's gradient.
    class EvalGradientConstraintVisitor : public boost::static_visitor<Function::vector_t>
    {
    public:
      explicit EvalGradientConstraintVisitor (const Function::vector_t& x)
        : x_ (x)
      {}

      template <typename C>
      Function::vector_t
      operator () (const C* c)
      {
        BOOST_STATIC_ASSERT((boost::is_convertible<C*, Function*>::value));

        return c->gradient (x_);
      }

    private:
      const Function::vector_t& x_;
    };


    /// CFSQP objective function.
    void obj (int nparam, int j , double* x, double* fj, void* cd)
    {
      assert (cd);
      CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      Function::vector_t x_ (nparam);
      array_to_vector (x_, x);
      *fj = solver->problem ().function () (x_);
    }

    /// CFSQP constraints function.
    void constr (int nparam, int j,
                 double* x, double* gj, void* cd)
    {
      assert (cd);
      CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      Function::vector_t x_ (nparam);
      array_to_vector (x_, x);

      int j_ = (j < 2) ? 0 : (j - 1)/2;

      EvalConstraintVisitor v (x_, j);
      *gj = boost::apply_visitor
        (v, solver->problem ().constraints () [j_]);
    }

    /// CFSQP objective function gradient.
    void gradob (int nparam, int j,
                 double* x, double* gradf, fct_t dummy, void* cd)
    {
      assert (cd);
      assert (j == 1);

      CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      Function::vector_t x_ (nparam);
      array_to_vector (x_, x);
      DerivableFunction::gradient_t grad = solver->problem ().function ().gradient (x_);
      vector_to_array (gradf, grad);
    }

    /// CFSQP constraints function gradient.
    void gradcn (int nparam, int j,
                 double* x, double* gradgj, fct_t dummy, void* cd)
    {
      assert (cd);

      CFSQPSolver* solver = static_cast<CFSQPSolver*> (cd);

      Function::vector_t x_ (nparam);
      array_to_vector (x_, x);

      int j_ = (j < 2) ? 0 : (j - 1)/2;

      EvalGradientConstraintVisitor v (x_);

      DerivableFunction::gradient_t grad =
        boost::apply_visitor (v, solver->problem ().constraints ()[j_]);
      vector_to_array (gradgj, grad);
    }

  }

  CFSQPSolver::CFSQPSolver (const problem_t& pb, int iprint) throw ()
    : parent_t (pb),
      iprint_ (iprint)
  {
  }

  CFSQPSolver::~CFSQPSolver () throw ()
  {
  }

  void
  CFSQPSolver::initialize_bounds (double* bl, double* bu) const throw ()
  {
    typedef Function::bounds_t::const_iterator citer_t;

    Function::size_type i = 0;
    for (citer_t it = problem ().function ().argBounds.begin ();
         it != problem ().function ().argBounds.end (); ++it)
      {
        bl[i] = (*it).first, bu[i] = (*it).second;
        ++i;
      }
  }

  void
  CFSQPSolver::solve () throw ()
  {
    int nparam = problem ().function ().n;
    int nf = 1; //FIXME: only one objective function.
    int nfsr = 0;
    int nineqn = 2 * problem ().constraints ().size ();
    int nineq = 2 * problem ().constraints ().size ();
    int neqn = 0;
    int neq = 0;
    int ncsrl = 0;
    int ncsrn = 0;
    int mesh_pts[1];
    int mode = 100;
    int miter = 500;
    int inform = 0;
    double bignd = std::numeric_limits<Function::value_type>::infinity ();
    double eps = 1e-8;
    double epseqn = 1e-8;
    double udelta = 1e-8;
    double bl[nparam];
    double bu[nparam];
    double x[nparam];
    double f[1];
    double g[2 * problem ().constraints ().size ()];
    double lambda[nparam + 1 + 2 * problem ().constraints ().size ()];
    fct_t obj = detail::obj;
    fct_t constr = detail::constr;
    grad_t gradob = detail::gradob;
    grad_t gradcn = detail::gradcn;

    // Initialize bounds.
    initialize_bounds (bl, bu);

    // Copy starting point.
    if (!!problem ().startingPoint ())
      detail::vector_to_array (x, *problem ().startingPoint ());

    cfsqp (nparam, nf, nfsr, nineqn, nineq, neqn, neq, ncsrl,  ncsrn,
           mesh_pts, mode,  iprint_, miter, &inform, bignd, eps, epseqn,
           udelta, bl, bu, x, f, g, lambda,
           obj, constr, gradob, gradcn, this);

    if (inform == 0)
      {
        Function::vector_t x_ (nparam);
        detail::array_to_vector (x_, x);
        result_ = x_;
      }
    else
      result_ = SolverError ("CFSQP has failed.");
  }

} // end of namespace optimization
