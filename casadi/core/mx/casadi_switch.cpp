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


#include "casadi_switch.hpp"
#include "../function/function_internal.hpp"
#include "../std_vector_tools.hpp"
#include "../mx/mx_tools.hpp"
#include "../matrix/matrix_tools.hpp"

using namespace std;

namespace casadi {

  Switch::Switch(const MX& ind, const std::vector<MX>& arg,
                 const std::vector<Function>& f, const Function& f_def)
    : f_(f), f_def_(f_def) {

    // Consitency check
    casadi_assert(!f_.empty());

    // Make sure ind scalar and dense (constructor should not have been called if sparse)
    casadi_assert(ind.isScalar(true));

    // Get input and output sparsities
    int num_in = -1, num_out=-1;
    for (int k=0; k<=f_.size(); ++k) {
      const Function& fk = getFunction(k);
      if (fk.isNull()) continue;
      fk.assertInit();
      if (num_in<0) {
        // Number of inputs and outputs
        num_in=fk.getNumInputs();
        num_out=fk.getNumOutputs();
        // Output sparsity
        sp_out_.resize(num_out);
        for (int i=0; i<num_out; ++i) sp_out_[i] = fk.output(i).sparsity();
        // Input sparsity
        sp_in_.resize(num_in);
        for (int i=0; i<num_in; ++i) sp_in_[i] = fk.input(i).sparsity();
      } else {
        // Assert matching number of inputs and outputs
        casadi_assert(num_in==fk.getNumInputs());
        casadi_assert(num_out==fk.getNumOutputs());
        // Intersect with output sparsity
        for (int i=0; i<num_out; ++i) {
          sp_out_[i] = sp_out_[i].patternIntersection(fk.output(i).sparsity());
        }
        // Intersect with input sparsity
        for (int i=0; i<num_in; ++i) {
          sp_in_[i] = sp_in_[i].patternIntersection(fk.input(i).sparsity());
        }
      }
    }

    // Illegal to pass only "null" functions
    casadi_assert_message(num_in>=0, "All functions are null");

    // Check number of arguments
    casadi_assert_message(arg.size()==num_in, "Argument list length (" << arg.size()
                          << ") does not match number of inputs (" << num_in << ")");

    // Create arguments of the right dimensions and sparsity
    vector<MX> arg1(num_in+1);
    arg1[0] = ind;
    for (int i=0; i<num_in; ++i) {
      arg1[i+1] = projectArg(arg[i], sp_in_[i], i);
    }
    setDependencies(arg1);
    setSparsity(Sparsity::scalar());
  }

  Switch* Switch::clone() const {
    return new Switch(*this);
  }

  void Switch::evalD(cp_double* arg, p_double* res,
                           int* itmp, double* rtmp) {
    // Get conditional
    int k = static_cast<int>(*arg[0]);

    // Get the function to be evaluated
    const Function& fk = getFunction(k);

    // Input buffers
    for (int i=0; i<sp_in_.size(); ++i) {
      const Sparsity& s = fk.input(i).sparsity();
      casadi_assert_message(s==sp_in_[i], "Not implemented");
    }

    // Output buffers
    for (int i=0; i<sp_out_.size(); ++i) {
      const Sparsity& s = fk.output(i).sparsity();
      casadi_assert_message(s==sp_out_[i], "Not implemented");
    }

    // Evaluate case FIXME: evalD should be a const method
    const_cast<Function&>(fk)->evalD(arg+1, res, itmp, rtmp);
  }

  int Switch::nout() const {
    return sp_out_.size();
  }

  const Sparsity& Switch::sparsity(int oind) const {
    return sp_out_.at(oind);
  }

  void Switch::evalMX(const vector<MX>& arg, vector<MX>& res) {
    vector<MX> arg1(arg.begin()+1, arg.end());
    res = conditional2(arg.at(0), arg1, f_, f_def_);
  }

  void Switch::evalFwd(const vector<vector<MX> >& fseed,
                       vector<vector<MX> >& fsens) {
    // Number of directional derivatives
    int nfwd = fseed.size();

    // Number inputs and outputs
    int n_in = ndep();
    int n_out = nout();

    // Create derivative functions
    vector<Function> der(f_.size());
    for (int k=0; k<f_.size(); ++k) {
      if (!f_[k].isNull()) der[k] = f_[k].derForward(nfwd);
    }

    // Default case
    Function der_def;
    if (!f_def_.isNull()) der_def = f_def_.derForward(nfwd);

    // Branch index
    MX c = dep(0);

    // Inputs to derivative functions
    vector<MX> v;
    v.reserve((n_in-1) + n_out + (n_in-1)*nfwd);
    for (int i=1; i<n_in; ++i) v.push_back(dep(i));
    for (int i=0; i<n_out; ++i) v.push_back(getOutput(i));
    for (int d=0; d<nfwd; ++d) {
      for (int i=1; i<n_in; ++i) v.push_back(fseed[d].at(i));
    }

    // Conditional call
    v = conditional2(c, v, der, der_def);
    vector<MX>::const_iterator v_it = v.begin();

    // Collect forward sensitivities
    for (int d=0; d<nfwd; ++d) {
      fsens[d].resize(n_out);
      for (int i=0; i<n_out; ++i) {
        fsens[d][i] = *v_it++;
      }
    }
    casadi_assert(v_it==v.end());
  }

  void Switch::evalAdj(const vector<vector<MX> >& aseed,
                     vector<vector<MX> >& asens) {
    // Number of directional derivatives
    int nadj = aseed.size();

    // Number inputs and outputs
    int n_in = ndep();
    int n_out = nout();

    // Create derivative functions
    vector<Function> der(f_.size());
    for (int k=0; k<f_.size(); ++k) {
      if (!f_[k].isNull()) der[k] = f_[k].derReverse(nadj);
    }

    // Default case
    Function der_def;
    if (!f_def_.isNull()) der_def = f_def_.derReverse(nadj);

    // Branch index
    MX c = dep(0);

    // Inputs to derivative functions
    vector<MX> v;
    v.reserve((n_in-1) + n_out + n_out*nadj);
    for (int i=1; i<n_in; ++i) v.push_back(dep(i));
    for (int i=0; i<n_out; ++i) v.push_back(getOutput(i));
    for (int d=0; d<nadj; ++d) {
      for (int i=0; i<n_out; ++i) v.push_back(aseed[d].at(i));
    }

    // Conditional call
    v = conditional2(c, v, der, der_def);
    vector<MX>::const_iterator v_it = v.begin();

    // Collect adjoint sensitivities
    for (int d=0; d<nadj; ++d) {
      for (int i=1; i<n_in; ++i) {
        asens[d].at(i) += *v_it++;
      }
    }
    casadi_assert(v_it==v.end());
  }

  void Switch::deepCopyMembers(std::map<SharedObjectNode*, SharedObject>& already_copied) {
    MXNode::deepCopyMembers(already_copied);
    for (int k=0; k<=f_.size(); ++k) {
      Function& fk = const_cast<Function&>(getFunction(k)); // FIXME or remove function
      fk = deepcopy(fk, already_copied);
    }
  }

  void Switch::generate(const vector<int>& arg, const vector<int>& res,
                        CodeGenerator& g) const {
    // Put in a separate scope to avoid name collisions
    g.body << "  {" << endl;

    // Input and output arrays
    generateIO(arg, res, g, 1);

    // Codegen as if-else
    bool if_else = f_.size()==1;

    // Codegen condition
    g.body << "    " << (if_else ? "if" : "switch")  << " ((int)"
           << g.workel(arg[0], dep(0).nnz()) << ") {" << endl;

    // Loop over cases/functions
    for (int k=0; k<=f_.size(); ++k) {

      // For if,  reverse order
      int k1 = if_else ? 1-k : k;

      if (!if_else) {
        // Codegen cases
        if (k1<f_.size()) {
          g.body << "    case " << k1 << ":" << endl;
        } else {
          g.body << "    default:" << endl;
        }
      } else if (k1==0) {
        // Else
        g.body << "    } else {" << endl;
      }

      // Get the function:
      const Function& fk = getFunction(k1);
      if (fk.isNull()) {
        g.body << "      return 1;" << endl;
      } else {
        // Get the index of the function
        int f = g.getDependency(fk);

        // Call function
        g.body << "      if (f" << f << "(arg1, res1, iw, w)) return 1;" << endl;
        if (!if_else)
          g.body << "      break;" << endl;
      }
    }

    // End switch/else
    g.body << "    }" << endl;

    // Finalize the function call
    g.body << "  }" << endl;
  }

  void Switch::nTmp(size_t& ni, size_t& nr) {
    ni=0;
    nr=0;
    for (int k=0; k<=f_.size(); ++k) {
      const Function& fk = getFunction(k);
      if (fk.isNull()) continue;

      // Get local work vector sizes
      size_t ni_k, nr_k;
      fk.nTmp(ni_k, nr_k);

      // Add size for input buffers
      for (int i=0; i<sp_in_.size(); ++i) {
        const Sparsity& s = fk.input(i).sparsity();
        if (s!=sp_in_[i]) nr_k += s.nnz();
      }

      // Add size for output buffers
      for (int i=0; i<sp_out_.size(); ++i) {
        const Sparsity& s = fk.output(i).sparsity();
        if (s!=sp_out_[i]) nr_k += s.nnz();
      }

      // Find the largest
      ni = max(ni, ni_k);
      nr = max(nr, nr_k);
    }
  }

  const Function& Switch::getFunction(int i) const {
    casadi_assert(i>=0 && i<=f_.size());
    if (i<f_.size()) {
      return f_[i];
    } else {
      return f_def_;
    }
  }

  std::string Switch::printArg(const std::vector<std::string>& arg) {
    stringstream ss;
    ss << "[";
    for (int i=1; i<arg.size(); ++i) {
      if (i>1) ss << ", ";
      ss << arg[i];
    }
    ss << "]";
    return ss.str();
  }

  std::string Switch::print(const std::vector<std::string>& arg) const {
    stringstream ss;
    if (f_.size()==1) {
      // Print as if-then-else
      ss << "if_then_else(" << arg[0] << ", " << printArg(arg) << ", "
         << f_def_.getOption("name") << ", " << f_[0].getOption("name") << ")";
    } else {
      // Print generic
      ss << "conditional2(" << arg[0] << ", " << printArg(arg) << ", [";
      for (int k=0; k<f_.size(); ++k) {
        if (k!=0) ss << ", ";
        ss << f_[k].getOption("name");
      }
      ss << "], " << f_def_.getOption("name") << ")";
    }
    return ss.str();
  }

  std::vector<MX> Switch::create(const MX& ind, const std::vector<MX>& arg,
                                 const std::vector<Function>& f, const Function& f_def) {
    return MX::createMultipleOutput(new Switch(ind, arg, f, f_def));
  }

} // namespace casadi
