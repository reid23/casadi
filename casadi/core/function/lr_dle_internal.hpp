/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef CASADI_LR_DLE_INTERNAL_HPP
#define CASADI_LR_DLE_INTERNAL_HPP

#include "lr_dle_solver.hpp"
#include "lr_dle_internal.hpp"
#include "function_internal.hpp"
#include "plugin_interface.hpp"

/// \cond INTERNAL

namespace casadi {

  /// Structure specification of a DLE
  enum LrDleStruct {
    /// The matrix A
    LR_DLE_STRUCT_A,
    /// The matrix V
    LR_DLE_STRUCT_V,
    /// The matrix C (defaults to unity)
    LR_DLE_STRUCT_C,
    /// H matrix: horizontal stack of all Hi
    LR_DLE_STRUCT_H,
    LR_DLE_STRUCT_NUM};

  /** \brief Internal storage for LrDleSolver related data

      @copydoc DLE_doc
     \author Joris Gillis
      \date 2014
  */
  class CASADI_EXPORT
  LrDleInternal : public FunctionInternal,
                  public PluginInterface<LrDleInternal> {
  public:
    /** \brief  Constructor
     *  \param st \structargument{Dle}
     */
    LrDleInternal(const std::map<std::string, Sparsity>& st, int nrhs=1, bool transp=false);

    /** \brief  Destructor */
    virtual ~LrDleInternal()=0;

    /** \brief  Create a new solver */
    virtual LrDleInternal* create(const std::map<std::string, Sparsity>& st) const = 0;

    /** \brief  Print solver statistics */
    virtual void printStats(std::ostream &stream) const {}

    /** \brief  evaluate */
    virtual void evaluate()=0;

    /** \brief  Initialize */
    virtual void init();

    /// Problem structure
    std::vector<Sparsity> st_;

    /// Sparsity of A
    Sparsity A_;

    /// Sparsity of V
    Sparsity V_;

    /// Sparsity of C
    Sparsity C_;

    /// Sparsity of H
    Sparsity H_;

    /// Flag if C is given
    bool with_C_;

    /// Flag if H is given
    bool with_H_;

    /// Assume positive definiteness of P_i
    bool pos_def_;

    /// Throw an error when system is unstable
    bool error_unstable_;

    /// Margin for instability detection
    double eps_unstable_;

    /// Number of right hand sides
    int nrhs_;

    /// Tranpose the system?
    bool transp_;

    // Creator function for internal class
    typedef LrDleInternal* (*Creator)(const std::map<std::string, Sparsity>& st);

    // No static functions exposed
    struct Exposed{ };

    /// Collection of solvers
    static std::map<std::string, Plugin> solvers_;

    /// Infix
    static const std::string infix_;

    /// Short name
    static std::string shortname() { return "lrdle";}

    /// List of columnsizes of Hi
    std::vector<int> Hs_;


    std::vector<DMatrix> Hv_;
    std::vector<int> Hi_;

    std::vector<DMatrix> Pv_;
    std::vector<int> Pi_;

    /// Get the resulting sparsity
    static Sparsity getSparsity(const std::map<std::string, Sparsity>& st,
      const std::vector<int> &Hs=std::vector<int>());

  };

} // namespace casadi
/// \endcond
#endif // CASADI_LR_DLE_INTERNAL_HPP
