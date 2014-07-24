// Copyright 2013-2014 Jean Charles Régin / Guillaume Perez
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <list>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "base/map_util.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/vector_map.h"

namespace operations_research {
namespace {

#define MEMTABLETOMDDDEBUGNON

/**
* Creation of MDD, from Yap & al, "An MDD-based Generalized Arc Consistency
* Algorithm for Positive and Negative Table Constraints and Some Global
* Constraints"
*/

//MDD_compression
class MDD {
  int num_var_;
  bool state_;
  std::vector<MDD*> childs_;
  //id for the VMREC algorithm
  int id;
  //for the eventual DFS ect (needed)
  bool visited;

 public:
  MDD(int nb_values, int num_var, int nb_instance)
      : state_(false), num_var_(num_var), id(nb_instance), visited(false) {
    for (int i = 0; i < nb_values; i++) {
      childs_.push_back(nullptr);
    }
  }

  ~MDD() {}

  MDD* operator[](int n) { return childs_[n]; }

  void set(int n, MDD* mdd) { childs_[n] = mdd; }

  int size() { return childs_.size(); }

  bool getState() { return state_; }
  void setState(bool state) { state_ = state; }

  int getNumVar() { return num_var_; }
  int getID() { return id; }

  void setID(int id_) { id = id_; }

  bool isVisited() { return visited; }
  void setVisited(bool v) { visited = v; }
};

class MDD_Factory {
  class VMREC {
    VectorMap<int> mdd_;
    std::vector<VMREC*> vmr_;
    MDD* value_;

   public:
    VMREC(MDD* value = nullptr) : value_(value), mdd_(), vmr_() {}

    ~VMREC() { STLDeleteElements(&vmr_); }

    VMREC* next(MDD* mdd) {
      if (mdd == nullptr) {
        if (mdd_.Contains(0)) {
          return vmr_[mdd_.Index(0)];
        }
        return nullptr;
      }
      if (mdd_.Contains(mdd->getID())) {
        return vmr_[mdd_.Index(mdd->getID())];
      }
      return nullptr;
    }

    void addMdd(MDD* mdd) {
      if (mdd == nullptr) {
        mdd_.Add(0);
        vmr_.push_back(new VMREC());
      } else {
        mdd_.Add(mdd->getID());
        vmr_.push_back(new VMREC());
      }
    }

    void setValue(MDD* value) { value_ = value; }

    MDD* getValue() { return value_; }

  };

  std::vector<VMREC*> g_;
  std::vector<int> NumDifferentValuesInColumn;
  MDD* finalEdge_;
  int nb_instance;

 public:
  std::vector<VectorMap<int64> > vm_;
  MDD_Factory() : nb_instance(1) {}

  ~MDD_Factory() { STLDeleteElements(&g_); }

  int getNbInstance() { return nb_instance; }

  //the first-one with table unsorted
  MDD* mddify(const IntTupleSet& table) {

#ifdef MEMTABLETOMDDDEBUG
    for (int i = 0; i < table.NumTuples(); i++) {
      for (int j = 0; j < table.Arity(); j++) {
        std::cout << table.Value(i, j) << " ";
      }
      std::cout << "\n";
    }
#endif

    finalEdge_ = new MDD(0, table.Arity(), nb_instance++);
    finalEdge_->setState(true);

    for (int i = 0; i < table.Arity(); i++) {
      vm_.push_back(VectorMap<int64>());
      g_.push_back(new VMREC());
      NumDifferentValuesInColumn.push_back(table.NumDifferentValuesInColumn(i));
    }
    MDD* Troot = new MDD(NumDifferentValuesInColumn[0], 0, nb_instance++);
    MDD* T = nullptr;
    for (int i = 0; i < table.NumTuples(); ++i) {
      T = Troot;
      for (int j = 0; j < table.Arity(); j++) {
        if (!vm_[j].Contains(table.Value(i, j))) {
          vm_[j].Add(table.Value(i, j));
        }
        const int index = vm_[j].Index(table.Value(i, j));
        if ((*T)[index] == nullptr) {
          if (j + 1 < table.Arity()) {
            T->set(index, new MDD(NumDifferentValuesInColumn[j + 1], j + 1,
                                  nb_instance++));
          } else {  //last
            T->set(index, finalEdge_);
          }

        }
        T = (*T)[index];
      }
    }
    return mddReduce(Troot);

  }

  //
  MDD* mddReduce(MDD* T) {
    if (T == nullptr) {
      return nullptr;
    }

    if (T->getState()) {
      return T;
    }
    int i;

    //        for (i = 0; i < T->size(); i++) {
    //            if ((*T)[i] != nullptr) {
    //                if (!(*T)[i]->getState()) {
    //                    break;
    //                }
    //            }
    //        }
    //        if (!(i < T->size())) {
    //            return T;
    //        }

    std::vector<MDD*> B(T->size(), nullptr);
    bool BEmpty = true;
    for (i = 0; i < T->size(); i++) {
      MDD* G = mddReduce((*T)[i]);
      if (G != nullptr) {
        B[i] = G;
        BEmpty = false;
      }
    }

    if (BEmpty) {
      delete (T);
      return nullptr;
    }

    MDD* G = new MDD(T->size(), T->getNumVar(), nb_instance++);
    for (int i = 0; i < T->size(); i++) {
      G->set(i, B[i]);
    }
    delete (T);
    MDD* GP = gContains(G);
    if (GP != nullptr) {  //exist a G' identical to G...
      delete (G);
      return GP;
    } else {
      //add G to the global set of MDD
      gAdd(G);
      return G;
    }
  }

  MDD* gContains(MDD* T) {
    VMREC* tmp = g_[T->getNumVar()];

    for (int i = 0; i < T->size(); i++) {
      tmp = tmp->next((*T)[i]);
      if (tmp == nullptr) {
        return nullptr;
      }
    }
    return tmp->getValue();
  }

  void gAdd(MDD* T) {
    VMREC* tmp = g_[T->getNumVar()];
    VMREC* tmp2 = tmp;
    for (int i = 0; i < T->size(); i++) {
      tmp2 = tmp;
      tmp = tmp->next((*T)[i]);
      if (tmp == nullptr) {
        tmp2->addMdd((*T)[i]);
        tmp = tmp2->next((*T)[i]);
      }
    }
    tmp->setValue(T);
  }

  MDD* ReCount(MDD* T) {
    std::vector<MDD*>* v1, *v2, *tmp;
    int nb = 0;

    std::vector<bool> visited(getNbInstance());
    v1 = new std::vector<MDD*>();
    v2 = new std::vector<MDD*>();
    v1->push_back(T);
    while (v1->size() > 0) {
      tmp = v2;
      v2 = v1;
      v1 = tmp;
      while (v2->size()) {
        v2->back()->setID(nb++);
        for (int i = 0; i < v2->back()->size(); ++i) {
          MDD* fils = (*v2->back())[i];
          if (fils != nullptr) {
            if (!visited[fils->getID()]) {
              visited[fils->getID()] = true;
              v1->push_back((*v2->back())[i]);
            }
          }

        }
        v2->pop_back();
      }
    }

    nb_instance = nb;
    return T;
  }

};

class Sparse_Set_Rev : public NumericalRev<int> {
 public:
  //size is the size of sparse
  Sparse_Set_Rev(int size_, int* sparse_, int* dense_)
      : NumericalRev<int>(0), sparse(sparse_), dense(dense_), size(size_) {
    for (int i = 0; i < size; ++i) {
      sparse[i] = -1;
      // dense_[i] = -1;
    }

  }

  ~Sparse_Set_Rev() {
    delete sparse;
    delete dense;
  }

  int operator[](int i) { return dense[i]; }

  bool isMember(int k) {
    int a = sparse[k];
    if (a < this->Value() && a >= 0 && dense[a] == k) {
      return true;
    }
    return false;
  }

  void addMember(int k, Solver* const s) {
    int a = sparse[k];
    int b = this->Value();
    if (a >= b || a < 0 || dense[a] != k) {
      sparse[k] = b;
      dense[b] = k;
      this->Incr(s);
    }
  }
  void remove(int k, Solver* const s) {
    int sk = sparse[k];
    int sL = this->Value() - 1;
    dense[sk] = dense[sL];
    sparse[dense[sk]] = sk;
    sparse[k] = sL;
    dense[sL] = k;
    this->Decr(s);
  }

  void clear(Solver* const solver) { this->SetValue(solver, 0); }

  int nbValues() { return this->Value(); }

  void restore(int k, Solver* const s) {
    int sk = sparse[k];
    int sL = this->Value();
    dense[sk] = dense[sL];
    sparse[dense[sk]] = sk;
    sparse[k] = sL;
    dense[sL] = k;
    this->Incr(s);
  }

 private:
  int* sparse;
  int* dense;
  int size;
};

class Sparse_Set {
 public:
  Sparse_Set(int size_, int* sparse_, int* dense_)
      : sparse(sparse_), dense(dense_), value(0), size(size_) {
    for (int i = 0; i < size; ++i) {
      sparse[i] = -1;
      dense_[i] = -1;
    }

  }

  ~Sparse_Set() {
    delete sparse;
    delete dense;
  }

  int operator[](int i) { return dense[i]; }

  bool isMember(int k) {
    int a = sparse[k];
    if (a < value && a >= 0 && dense[a] == k) {
      return true;
    }
    return false;
  }

  void addMember(int k) {
    // int a = sparse[k];
    // int b = value;
    // if (a >= b || a < 0  || dense[a] != k)
    // {
    //   sparse[k] = b;
    //   dense[b] = k;
    //   value++;
    // }
    sparse[k] = value++;
    dense[sparse[k]] = k;
  }
  void remove(int k) {
    int a = sparse[k];
    int b = value - 1;
    dense[a] = dense[b];
    value--;
  }

  void clear() { value = 0; }

  int nbValues() { return value; }

 private:
  int* sparse;
  int* dense;
  int size;
  int value;

};

class MyMDD {
  class Edge {
   public:
    Edge(int value, int start, int end, int id)
        : value_(value), in_the_list_(0), id_(id), start_(start), end_(end) {}

    void setInTheList(int state) { in_the_list_ = state; }
    inline int getInTheList() { return in_the_list_; }

    inline int getValue() { return value_; }

    inline int getStart() { return start_; }

    void setStart(int start) { start_ = start; }

    inline int getEnd() { return end_; }

    void setEnd(int end) { end_ = end; }

   private:
    int start_;
    int end_;
    //bool to know if a Edge is already in the delete liste
    int in_the_list_;
    int id_;

    int value_;
  };

  class Node {
   public:
    Node(int var, int* shared_in, int* shared_out, int nb_in, int nb_out,
         int number_of_edges)
        : var_(var),
          in_(nb_in, shared_in, number_of_edges),
          out_(nb_out, shared_out, number_of_edges),
          visited(0) {}

    inline int getNumberOfEdgeIn() { return in_.Size(); }
    inline int getNumberOfEdgeOut() { return out_.Size(); }
    inline int getEdgeIn(int index) { return in_.Element(index); }
    inline int getEdgeOut(int index) { return out_.Element(index); }

    inline int getVariable() { return var_; }

    void removeEdgeIn(int edge, Solver* solver) { in_.Remove(solver, edge); }
    void removeEdgeOut(int edge, Solver* solver) { out_.Remove(solver, edge); }

    void InsertEdgeIn(int edge, Solver* solver) { in_.Insert(solver, edge); }
    void InsertEdgeOut(int edge, Solver* solver) { out_.Insert(solver, edge); }

    //used for the reset
    void ClearEdgeIn(Solver* solver) { in_.Clear(solver); }

    void ClearEdgeOut(Solver* solver) { out_.Clear(solver); }

    void RestoreEdgeIn(int edge, Solver* solver) { in_.Restore(solver, edge); }

    void RestoreEdgeOut(int edge, Solver* solver) {
      out_.Restore(solver, edge);
    }

    inline bool isVisited() { return visited.Value(); }

    void setVisited(Solver* solver) { visited.Incr(solver); }

   private:
    RevIntSet<int> in_;
    RevIntSet<int> out_;
    NumericalRev<int> visited;

    int var_;
  };

 public:

  MyMDD(const IntTupleSet& tuples, Solver* solver)
      : vm_(),
        number_of_edges_by_value(),
        edges_(),
        nodes_(),
        shared_in_(nullptr),
        shared_out_(nullptr),
        shared_nodes(),
        number_of_edge(0),
        nodes_lvl(),
        sizeBeforeReset(),
        edges_lvl() {
    //Construct the MDD
    MDD_Factory mf;
    MDD* mdd = mf.mddify(tuples);

    //
    std::list<MDD*> tmp;
    number_of_edge = 0;

    //creation of the convertion table
    for (int var = 0; var < mf.vm_.size(); var++) {
      vm_.push_back(VectorMap<int64>());
      number_of_edges_by_value.push_back(std::vector<int>());
      for (int val = 0; val < mf.vm_[var].size(); val++) {
        vm_[var].Add(mf.vm_[var][val]);
        number_of_edges_by_value[var].push_back(0);

      }

    }

    //will count how many arc are intering in a node
    std::vector<int> nb_in(mf.getNbInstance(), 0);
    //will count how many arc will get out from a node
    std::vector<int> nb_out(mf.getNbInstance(), 0);

    std::vector<int> num_var(mf.getNbInstance(), -1);

    std::vector<int> indice;

    std::vector<int> nb_nodes_lvl(tuples.Arity() + 1, 0);

    tmp.push_back(mdd);

    num_var[mdd->getID()] = indice.size();
    indice.push_back(mdd->getNumVar());
    mdd->setVisited(true);

    while (tmp.size() > 0) {

      mdd = tmp.front();
      tmp.pop_front();
      //count the number of node at each lvl
      nb_nodes_lvl[mdd->getNumVar()]++;
      for (int value = 0; value < mdd->size(); value++) {
        if ((*mdd)[value] != nullptr) {

          if (!(*mdd)[value]->isVisited()) {

            num_var[(*mdd)[value]->getID()] = indice.size();
            indice.push_back((*mdd)[value]->getNumVar());

            tmp.push_back((*mdd)[value]);
            (*mdd)[value]->setVisited(true);
          }

          edges_.push_back(
              new Edge(value, num_var[mdd->getID()],
                       num_var[(*mdd)[value]->getID()], number_of_edge++));

          nb_in[num_var[(*mdd)[value]->getID()]]++;
          nb_out[num_var[mdd->getID()]]++;

          number_of_edges_by_value[mdd->getNumVar()][value]++;

        }
      }

    }

    shared_in_ = new int[edges_.size()];
    shared_out_ = new int[edges_.size()];
    shared_nodes = new int[indice.size() + 1];

    for (int lvl = 0; lvl < tuples.Arity() + 1; ++lvl) {
      nodes_lvl.push_back(new Sparse_Set_Rev(indice.size(), shared_nodes,
                                             new int[nb_nodes_lvl[lvl]]));

      sizeBeforeReset.push_back(0);

      edges_lvl.push_back(new NumericalRev<int>(0));
    }

    for (int n = 0; n < indice.size(); n++) {
      nodes_.push_back(new Node(indice[n], shared_in_, shared_out_, nb_in[n],
                                nb_out[n], number_of_edge));

      //add the nodes to the correct set
      nodes_lvl[indice[n]]->addMember(n, solver);
    }

    for (int edge = 0; edge < edges_.size(); edge++) {
      nodes_[edges_[edge]->getStart()]->InsertEdgeOut(edge, solver);
      edges_lvl[nodes_[edges_[edge]->getStart()]->getVariable()]->Incr(solver);
      nodes_[edges_[edge]->getEnd()]->InsertEdgeIn(edge, solver);
    }

    delete mdd;

  }

  ~MyMDD() {
    delete shared_in_;
    delete shared_out_;
    delete shared_nodes;

    for (int i = 0; i < edges_.size(); ++i) {
      if (edges_[i] != nullptr) {
        delete edges_[i];
      }
    }
    for (int i = 0; i < nodes_.size(); ++i) {
      if (nodes_[i] != nullptr) {
        delete nodes_[i];
      }
    }

  }

  inline int getIndexVal(int index, int64 val) { return vm_[index].Index(val); }

  inline bool containValIndex(int index, int64 val) {
    return vm_[index].Contains(val);
  }

  inline int64 getValForIndex(int index, int val) {
    return vm_[index].Element(val);
  }

  /**
  * Here the functions for the MDD4 Cst.
  */

  void getRemovedEdge(int edge, std::vector<int>& deleteListDown,
                      std::vector<int>& deleteListUp, Solver* solver) {
    getRemovedEdgeUp(edge, deleteListUp, solver);
    getRemovedEdgeDown(edge, deleteListDown, solver);
  }
  void getRemovedEdgeUp(int edge, std::vector<int>& deleteList,
                        Solver* solver) {
    const int e_start = edges_[edge]->getStart();
    if (!nodes_[e_start]->isVisited() &&
        nodes_[e_start]->getNumberOfEdgeOut() == 0) {
      nodes_[e_start]->setVisited(solver);
      int i = nodes_[e_start]->getNumberOfEdgeIn() - 1;
      while (i > -1) {
        deleteList.push_back(nodes_[e_start]->getEdgeIn(i));
        removeEdgeUp(nodes_[e_start]->getEdgeIn(i--), solver);
      }

    }

  }
  void getRemovedEdgeDown(int edge, std::vector<int>& deleteList,
                          Solver* solver) {

    const int e_end = edges_[edge]->getEnd();
    if (!nodes_[e_end]->isVisited() &&
        nodes_[e_end]->getNumberOfEdgeIn() == 0) {
      nodes_[e_end]->setVisited(solver);
      int i = nodes_[e_end]->getNumberOfEdgeOut() - 1;
      while (i > -1) {
        deleteList.push_back(nodes_[e_end]->getEdgeOut(i));
        removeEdgeDown(nodes_[e_end]->getEdgeOut(i--), solver);
      }
    }

  }

  void removeEdge(int edge, Solver* solver) {
    nodes_[edges_[edge]->getStart()]->removeEdgeOut(edge, solver);
    nodes_[edges_[edge]->getEnd()]->removeEdgeIn(edge, solver);

  }
  void removeEdgeUp(int edge, Solver* solver) {
    nodes_[edges_[edge]->getStart()]->removeEdgeOut(edge, solver);
    // nodes_[edges_[edge]->getEnd()]->removeEdgeIn(edge, solver);

  }
  void removeEdgeDown(int edge, Solver* solver) {
    // nodes_[edges_[edge]->getStart()]->removeEdgeOut(edge, solver);
    nodes_[edges_[edge]->getEnd()]->removeEdgeIn(edge, solver);

  }

  /**
  * Here the functions for the MDD4-R Cst.
  */
  void resetDeleteEdge(int edge, Solver* solver, int& cptUp, int& cptDown) {
    resetDeleteEdgeUp(edge, solver, cptUp);
    resetDeleteEdgeDown(edge, solver, cptDown);
  }

  void resetDeleteEdgeUp(int edge, Solver* solver, int& cptUp) {
    removeEdgeUp(edge, solver);
    const int e_start = edges_[edge]->getStart();
    if (nodes_[e_start]->getNumberOfEdgeOut() == 0) {
      cptUp += nodes_[e_start]->getNumberOfEdgeIn();
      nodes_lvl[nodes_[e_start]->getVariable()]->remove(e_start, solver);
    }
  }

  void resetDeleteEdgeDown(int edge, Solver* solver, int& cptDown) {
    removeEdgeDown(edge, solver);
    const int e_end = edges_[edge]->getEnd();
    if (nodes_[e_end]->getNumberOfEdgeIn() == 0) {
      cptDown += nodes_[e_end]->getNumberOfEdgeOut();
      nodes_lvl[nodes_[e_end]->getVariable()]->remove(e_end, solver);
    }
  }

  void resetRestoreEdge(int edge, Solver* solver, int& cptUp, int& cptDown) {
    resetRestoreEdgeUp(edge, solver, cptUp);
    resetRestoreEdgeDown(edge, solver, cptDown);
  }
  void resetRestoreEdgeUp(int edge, Solver* solver, int& cptUp) {
    const int e_start = edges_[edge]->getStart();
    if (!nodes_lvl[nodes_[e_start]->getVariable()]->isMember(e_start)) {
      //we clear the node to delete the deleted edges
      nodes_[e_start]->ClearEdgeOut(solver);

      nodes_lvl[nodes_[e_start]->getVariable()]->restore(e_start, solver);
      cptUp += nodes_[e_start]->getNumberOfEdgeIn();
    }
    //and we restore only still valid edges
    nodes_[e_start]->RestoreEdgeOut(edge, solver);
  }
  void resetRestoreEdgeDown(int edge, Solver* solver, int& cptDown) {
    const int e_end = edges_[edge]->getEnd();
    if (!nodes_lvl[nodes_[e_end]->getVariable()]->isMember(e_end)) {
      nodes_[e_end]->ClearEdgeIn(solver);

      nodes_lvl[nodes_[e_end]->getVariable()]->restore(e_end, solver);
      cptDown += nodes_[e_end]->getNumberOfEdgeOut();
    }

    nodes_[e_end]->RestoreEdgeIn(edge, solver);
  }

  void clearNodesSet(int lvl, Solver* solver) {
    sizeBeforeReset[lvl] = nodes_lvl[lvl]->nbValues();
    nodes_lvl[lvl]->clear(solver);
  }

  void saveNodesSet(int lvl, Solver* solver) {
    sizeBeforeReset[lvl] = nodes_lvl[lvl]->nbValues();
  }

  void resetGetEdgesToKeepUp(int lvl, std::vector<int>& edges) {
    edges.clear();
    for (int i = 0; i < nodes_lvl[lvl]->nbValues(); ++i) {
      const int n = (*nodes_lvl[lvl])[i];
      for (int e = 0; e < nodes_[n]->getNumberOfEdgeIn(); ++e) {
        edges.push_back(nodes_[n]->getEdgeIn(e));
      }
    }
  }
  void resetGetEdgesToDeleteUp(int lvl, std::vector<int>& edges) {
    edges.clear();
    for (int i = nodes_lvl[lvl]->nbValues(); i < sizeBeforeReset[lvl]; ++i) {
      const int n = (*nodes_lvl[lvl])[i];
      for (int e = 0; e < nodes_[n]->getNumberOfEdgeIn(); ++e) {
        edges.push_back(nodes_[n]->getEdgeIn(e));
      }
    }
  }

  void resetGetEdgesToKeepDown(int lvl, std::vector<int>& edges) {
    edges.clear();
    for (int i = 0; i < nodes_lvl[lvl]->nbValues(); ++i) {
      const int n = (*nodes_lvl[lvl])[i];
      for (int e = 0; e < nodes_[n]->getNumberOfEdgeOut(); ++e) {
        edges.push_back(nodes_[n]->getEdgeOut(e));
      }
    }
  }
  void resetGetEdgesToDeleteDown(int lvl, std::vector<int>& edges) {
    edges.clear();
    for (int i = nodes_lvl[lvl]->nbValues(); i < sizeBeforeReset[lvl]; ++i) {
      const int n = (*nodes_lvl[lvl])[i];
      for (int e = 0; e < nodes_[n]->getNumberOfEdgeOut(); ++e) {
        edges.push_back(nodes_[n]->getEdgeOut(e));
      }
    }
  }

  /**
  * First version of the Reset.
  */

  //will be use during the reset, to get only once the edges

  void getStillValidEdges(int edge, std::vector<int>& ListDown,
                          std::vector<int>& ListUp, Solver* solver) {

    if (nodes_[edges_[edge]->getStart()]->getNumberOfEdgeIn() > 0) {
      int i = nodes_[edges_[edge]->getStart()]->getNumberOfEdgeIn() - 1;
      while (i > -1) {
        ListUp.push_back(nodes_[edges_[edge]->getStart()]->getEdgeIn(i--));
      }
      nodes_[edges_[edge]->getStart()]->ClearEdgeIn(solver);
      nodes_[edges_[edge]->getStart()]->ClearEdgeOut(solver);
    }

    if (nodes_[edges_[edge]->getEnd()]->getNumberOfEdgeOut() > 0) {
      int i = nodes_[edges_[edge]->getEnd()]->getNumberOfEdgeOut() - 1;
      while (i > -1) {
        ListDown.push_back(nodes_[edges_[edge]->getEnd()]->getEdgeOut(i--));
      }
      nodes_[edges_[edge]->getEnd()]->ClearEdgeOut(solver);
      nodes_[edges_[edge]->getEnd()]->ClearEdgeIn(solver);
    }

  }

  void getStillValidEdgesUp(int edge, std::vector<int>& ListUp,
                            Solver* solver) {

    if (nodes_[edges_[edge]->getStart()]->getNumberOfEdgeIn() > 0) {
      int i = nodes_[edges_[edge]->getStart()]->getNumberOfEdgeIn() - 1;
      while (i > -1) {
        ListUp.push_back(nodes_[edges_[edge]->getStart()]->getEdgeIn(i--));
      }
      nodes_[edges_[edge]->getStart()]->ClearEdgeIn(solver);
      nodes_[edges_[edge]->getStart()]->ClearEdgeOut(solver);
    }

  }

  void getStillValidEdgesDown(int edge, std::vector<int>& ListDown,
                              Solver* solver) {

    if (nodes_[edges_[edge]->getEnd()]->getNumberOfEdgeOut() > 0) {
      int i = nodes_[edges_[edge]->getEnd()]->getNumberOfEdgeOut() - 1;
      while (i > -1) {
        ListDown.push_back(nodes_[edges_[edge]->getEnd()]->getEdgeOut(i--));
      }
      nodes_[edges_[edge]->getEnd()]->ClearEdgeOut(solver);
      nodes_[edges_[edge]->getEnd()]->ClearEdgeIn(solver);
    }

  }

  void restoreEdge(int edge, Solver* solver) {
    nodes_[edges_[edge]->getEnd()]->RestoreEdgeIn(edge, solver);
    nodes_[edges_[edge]->getStart()]->RestoreEdgeOut(edge, solver);
  }
  void restoreEdgeUp(int edge, Solver* solver) {
    nodes_[edges_[edge]->getEnd()]->RestoreEdgeIn(edge, solver);
  }

  void restoreEdgeDown(int edge, Solver* solver) {
    nodes_[edges_[edge]->getStart()]->RestoreEdgeOut(edge, solver);
  }

  void setInTheList(int index, int state) {
    edges_[index]->setInTheList(state);
  }
  int getInTheList(int index) { return edges_[index]->getInTheList(); }

  int getVarForEdge(int edge) {
    return nodes_[edges_[edge]->getStart()]->getVariable();
  }
  int getValueForEdge(int edge) { return edges_[edge]->getValue(); }

  int getNumberOfEdge() { return edges_.size(); }

  int getNumberOfNode() { return nodes_.size(); }

  int getNumberOfEdgesLvl(int lvl) { return edges_lvl[lvl]->Value(); }
  void setNumberOfEdgesLvl(int lvl, int newValue, Solver* solver) {
    edges_lvl[lvl]->SetValue(solver, newValue);
  }

  std::vector<VectorMap<int64> > vm_;
  std::vector<std::vector<int> > number_of_edges_by_value;

 private:
  std::vector<Edge*> edges_;
  std::vector<Node*> nodes_;

  int* shared_in_;
  int* shared_out_;
  int* shared_nodes;

  int number_of_edge;

  std::vector<Sparse_Set_Rev*> nodes_lvl;
  std::vector<int> sizeBeforeReset;

  std::vector<NumericalRev<int>*> edges_lvl;
};

class MddTableVar {
 public:
  MddTableVar(Solver* const solver, IntVar* var, int index,
              int number_of_different_value, int* shared_positions_edges,
              int number_of_edges, std::vector<int>& number_of_edges_by_value,
              MyMDD& mdd)
      : solver_(solver),
        index_(index),
        edges_per_value_(number_of_different_value),
        active_values_(number_of_different_value),
        var_(var),
        domain_iterator_(var->MakeDomainIterator(true)),
        delta_domain_iterator_(var->MakeHoleIterator(true)),
        mdd_(mdd) {
    for (int value_index = 0; value_index < edges_per_value_.size();
         value_index++) {
      edges_per_value_[value_index] =
          new RevIntSet<int>(number_of_edges_by_value[value_index],
                             shared_positions_edges, number_of_edges);
      active_values_.Insert(solver_, value_index);
    }
  }

  ~MddTableVar() { STLDeleteElements(&edges_per_value_); }

  IntVar* Variable() const { return var_; }

  void CollectEdgesToRemove(std::vector<int>& delta,
                            std::vector<int>& edges_to_remove) {
    edges_to_remove.clear();
    for (int k = 0; k < delta.size(); k++) {
      const int value = delta[k];
      for (int edge_index = 0; edge_index < edges_per_value_[value]->Size();
           edge_index++) {
        const int edge = edges_per_value_[value]->Element(edge_index);
        edges_to_remove.push_back(edge);
      }
    }

  }
  void CollectEdgesToKeep(std::vector<int>& edges_to_keep) {
    edges_to_keep.clear();
    for (domain_iterator_->Init(); domain_iterator_->Ok();
         domain_iterator_->Next()) {
      const int value = mdd_.getIndexVal(index_, domain_iterator_->Value());
      for (int edge_index = 0; edge_index < edges_per_value_[value]->Size();
           edge_index++) {
        const int edge = edges_per_value_[value]->Element(edge_index);
        edges_to_keep.push_back(edge);
      }
    }

  }

  int getNumberOfEdgeToRemove(std::vector<int>& delta) {
    int total = 0;
    for (int k = 0; k < delta.size(); k++) {
      const int value = delta[k];
      total += edges_per_value_[value]->Size();
    }
    return total;
  }

  void removeEdgeForValue(int value, int edge) {
    edges_per_value_[value]->Remove(solver_, edge);
    if (edges_per_value_[value]->Size() == 0) {
      var_->RemoveValue(mdd_.getValForIndex(index_, value));
      active_values_.Remove(solver_, value);
    }
  }

  void ComputeDeltaDomain(std::vector<int>* delta) {
    delta->clear();
    // we iterate the delta of the variable
    //
    // ATTENTION code for or-tools: the delta iterator does not
    // include the values between oldmin and min and the values
    // between max and oldmax.
    //
    // therefore we decompose the iteration into 3 parts
    // - from oldmin to min
    // - for the deleted values between min and max
    // - from max to oldmax

    // First iteration: from old_min to min.
    const int64 old_min_domain = var_->OldMin();
    const int64 min_domain = var_->Min();
    const int64 max_domain = var_->Max();
    for (int64 val = old_min_domain; val < min_domain; ++val) {
      const int index = mdd_.getIndexVal(index_, val);
      if (index != -1) {
        delta->push_back(index);
      }
    }
    // Second iteration: "delta" domain iteration.
    IntVarIterator* const it = delta_domain_iterator_;
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 value = it->Value();
      if (value > min_domain && value < max_domain) {
        const int index = mdd_.getIndexVal(index_, value);
        if (index != -1) {
          delta->push_back(index);
        }
      }
    }
    // Third iteration: from max to old_max.
    const int64 old_max_domain = var_->OldMax();
    for (int64 val = max_domain + 1; val <= old_max_domain; ++val) {
      const int index = mdd_.getIndexVal(index_, val);
      if (index != -1) {
        delta->push_back(index);
      }
    }
  }

  void addEdge(int value, int edge) {
    edges_per_value_[value]->Insert(solver_, edge);
  }

  void ClearSupport() {
    for (int k = 0; k < active_values_.Size(); k++) {
      edges_per_value_[active_values_.Element(k)]->Clear(solver_);
    }
  }

  void RestoreEdge(int value, int edge) {
    edges_per_value_[value]->Restore(solver_, edge);
  }

  void RemoveUnsuportedValue() {
    int count = 0;
    for (int k = active_values_.Size() - 1; k >= 0; k--) {
      const int64 value_index = active_values_.Element(k);
      if (edges_per_value_[value_index]->Size() == 0) {
        active_values_.Remove(solver_, value_index);
        count++;
      }
    }
    // Removed values have been inserted after the last active value.
    for (int k = 0; k < count; ++k) {
      const int64 value_index = active_values_.RemovedElement(k);
      var_->RemoveValue(mdd_.getValForIndex(index_, value_index));
    }
  }

  int GetNbActiveValues() { return var_->Size(); }

  void DeleteDeltaValues(std::vector<int>& delta) {
    for (int k = 0; k < delta.size(); k++) {
      const int value = delta[k];
      //protect of multiple proceeding ...
      if (edges_per_value_[value]->Size() > 0) {
        active_values_.Remove(solver_, value);
        edges_per_value_[value]->Clear(solver_);
      }

    }

  }

  void DeleteValuesNotBelongTheMDD() {

    //maybe no need of the table?
    std::vector<int64> toDel;
    for (domain_iterator_->Init(); domain_iterator_->Ok();
         domain_iterator_->Next()) {
      const int64 val = domain_iterator_->Value();
      if (!mdd_.containValIndex(index_, val)) {
        toDel.push_back(val);
      }
    }

    for (int i = 0; i < toDel.size(); ++i) {
      var_->RemoveValue(toDel[i]);
    }
  }

 private:
  Solver* solver_;
  std::vector<RevIntSet<int>*> edges_per_value_;
  RevIntSet<int> active_values_;
  IntVar* const var_;
  IntVarIterator* const domain_iterator_;
  IntVarIterator* const delta_domain_iterator_;

  int index_;
  MyMDD& mdd_;
};

class Ac4MddTableConstraint : public Constraint {
 public:

  Ac4MddTableConstraint(Solver* const solver, const IntTupleSet& tuples,
                        const std::vector<IntVar*>& vars)
      : Constraint(solver),
        original_vars_(vars),
        vars_(tuples.Arity()),
        delta_of_value_indices_(0),
        num_variables_(tuples.Arity()),
        solver_(solver),
        mdd_(tuples, solver_),
        up(new std::vector<int>()),
        down(new std::vector<int>()),
        edges(new std::vector<int>()),
        tmp(nullptr),
        num_tuples_(tuples.NumTuples()),
        tuple_set_(tuples) {
    shared_positions_edges_ = new int[mdd_.getNumberOfEdge()];
    for (int var_index = 0; var_index < tuples.Arity(); var_index++) {
      vars_[var_index] = new MddTableVar(
          solver, vars[var_index], var_index, mdd_.vm_[var_index].size(),
          shared_positions_edges_, mdd_.getNumberOfEdge(),
          mdd_.number_of_edges_by_value[var_index], mdd_);

    }

  }

  ~Ac4MddTableConstraint() {
    STLDeleteElements(&vars_);  // delete all elements of a vector
    delete shared_positions_edges_;
  }

  void Post() {
    for (int var_index = 0; var_index < num_variables_; var_index++) {
      Demon* const demon = MakeConstraintDemon1(
          solver(), this, &Ac4MddTableConstraint::FilterOneVariable,
          "FilterOneVariable", var_index);
      vars_[var_index]->Variable()->WhenDomain(demon);
    }
  }

  void InitialPropagate() {

    //insert edges in correct set
    int number_of_edge = mdd_.getNumberOfEdge();
    for (int edge = 0; edge < number_of_edge; edge++) {
      vars_[mdd_.getVarForEdge(edge)]
          ->addEdge(mdd_.getValueForEdge(edge), edge);
    }

    for (int var_index = 0; var_index < num_variables_; var_index++) {
      vars_[var_index]->DeleteValuesNotBelongTheMDD();
    }

    for (int var_index = 0; var_index < num_variables_; var_index++) {
      FilterOneVariable(var_index);
    }

  }

  void FilterOneVariable(int var_index) {

    MddTableVar* const var = vars_[var_index];
    var->ComputeDeltaDomain(&delta_of_value_indices_);

    int cptUp = 0;
    int cptDown = 0;

    //last action in this direction
    bool ResetUp = false;
    bool ResetDown = false;

    //index of the next var process
    int lvlUp = var_index;
    int lvlDown = var_index + 1;

    int nbToRemove = var->getNumberOfEdgeToRemove(delta_of_value_indices_);

    int nbToKeep = mdd_.getNumberOfEdgesLvl(var_index) - nbToRemove;

    mdd_.setNumberOfEdgesLvl(var_index, nbToKeep, solver_);

    if (nbToRemove < nbToKeep)  // we should do the normal deletation
        {

      mdd_.saveNodesSet(lvlDown, solver_);
      mdd_.saveNodesSet(lvlUp, solver_);

      var->CollectEdgesToRemove(delta_of_value_indices_, edges_to_remove_);

      //delete them from the MDD and from the current support_list
      for (int edge_indice = 0; edge_indice < edges_to_remove_.size();
           ++edge_indice) {
        const int edge = edges_to_remove_[edge_indice];
        //delete in the MDD
        mdd_.resetDeleteEdge(edge, solver_, cptUp, cptDown);

      }

    } else {  //we should reset
      ResetUp = true;
      ResetDown = true;

      var->CollectEdgesToKeep(edges_to_remove_);
      var->ClearSupport();

      mdd_.clearNodesSet(lvlDown, solver_);
      mdd_.clearNodesSet(lvlUp, solver_);

      for (int edge_indice = 0; edge_indice < edges_to_remove_.size();
           ++edge_indice) {
        const int edge = edges_to_remove_[edge_indice];
        //restoration in the MDD
        mdd_.resetRestoreEdge(edge, solver_, cptUp, cptDown);
        //restoration in the support
        vars_[var_index]->RestoreEdge(mdd_.getValueForEdge(edge), edge);

      }
      var->RemoveUnsuportedValue();

    }

    var->DeleteDeltaValues(delta_of_value_indices_);

    while (cptUp > 0) {
      lvlUp--;

      if (ResetUp)  //if we was in reset mode
          {
        nbToRemove = mdd_.getNumberOfEdgesLvl(lvlUp) - cptUp;
        nbToKeep = cptUp;
        cptUp = 0;
      } else {
        nbToRemove = cptUp;
        nbToKeep = mdd_.getNumberOfEdgesLvl(lvlUp) - cptUp;
        cptUp = 0;
      }

      mdd_.setNumberOfEdgesLvl(lvlUp, nbToKeep, solver_);

      if (nbToRemove < nbToKeep)  //we should delete
          {
        mdd_.saveNodesSet(lvlUp, solver_);
        ResetUp = false;
        mdd_.resetGetEdgesToDeleteUp(lvlUp + 1, edges_to_remove_);

        for (int edge_indice = 0; edge_indice < edges_to_remove_.size();
             ++edge_indice) {
          const int edge = edges_to_remove_[edge_indice];
          //delete in the MDD
          mdd_.resetDeleteEdgeUp(edge, solver_, cptUp);
          //delete in the support
          vars_[lvlUp]->removeEdgeForValue(mdd_.getValueForEdge(edge), edge);
        }

      } else {  //we should reset
        ResetUp = true;
        //on reset le niveau dans lequel on va regarder
        mdd_.clearNodesSet(lvlUp, solver_);
        //on recupere les edge du niveau précédent
        mdd_.resetGetEdgesToKeepUp(lvlUp + 1, edges_to_remove_);

        vars_[lvlUp]->ClearSupport();

        for (int edge_indice = 0; edge_indice < edges_to_remove_.size();
             ++edge_indice) {
          const int edge = edges_to_remove_[edge_indice];
          //restoration in the MDD
          mdd_.resetRestoreEdgeUp(edge, solver_, cptUp);
          //restoration in the support
          vars_[lvlUp]->RestoreEdge(mdd_.getValueForEdge(edge), edge);

        }
        vars_[lvlUp]->RemoveUnsuportedValue();
      }

    }

    while (cptDown > 0) {

      if (ResetDown)  //if we was in reset mode
          {
        nbToRemove = mdd_.getNumberOfEdgesLvl(lvlDown) - cptDown;
        nbToKeep = cptDown;
        cptDown = 0;
      } else {
        nbToRemove = cptDown;
        nbToKeep = mdd_.getNumberOfEdgesLvl(lvlDown) - cptDown;
        cptDown = 0;
      }

      mdd_.setNumberOfEdgesLvl(lvlDown, nbToKeep, solver_);

      if (nbToRemove < nbToKeep)  //we should delete
          {
        mdd_.saveNodesSet(lvlDown + 1, solver_);
        ResetDown = false;
        mdd_.resetGetEdgesToDeleteDown(lvlDown, edges_to_remove_);

        for (int edge_indice = 0; edge_indice < edges_to_remove_.size();
             ++edge_indice) {
          const int edge = edges_to_remove_[edge_indice];
          //delete in the MDD
          mdd_.resetDeleteEdgeDown(edge, solver_, cptDown);
          //delete in the support
          vars_[lvlDown]->removeEdgeForValue(mdd_.getValueForEdge(edge), edge);
        }

      } else {  //we should reset
        ResetDown = true;
        mdd_.clearNodesSet(lvlDown + 1, solver_);

        mdd_.resetGetEdgesToKeepDown(lvlDown, edges_to_remove_);

        vars_[lvlDown]->ClearSupport();

        for (int edge_indice = 0; edge_indice < edges_to_remove_.size();
             ++edge_indice) {
          const int edge = edges_to_remove_[edge_indice];
          //restoration in the MDD
          mdd_.resetRestoreEdgeDown(edge, solver_, cptDown);
          //restoration in the support
          vars_[lvlDown]->RestoreEdge(mdd_.getValueForEdge(edge), edge);

        }
        vars_[lvlDown]->RemoveUnsuportedValue();
      }
      lvlDown++;

    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("AllowedAssignments(arity = %d, tuple_count = %d)",
                        num_variables_, num_tuples_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kAllowedAssignments, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               original_vars_);
    visitor->VisitIntegerMatrixArgument(ModelVisitor::kTuplesArgument,
                                        tuple_set_);
    visitor->EndVisitConstraint(ModelVisitor::kAllowedAssignments, this);
  }

 private:
  Solver* solver_;
  std::vector<IntVar*> original_vars_;
  // Variables of the constraint.
  std::vector<MddTableVar*> vars_;
  // Number of variables.
  const int num_variables_;
  const int num_tuples_;
  // Temporary storage for delta of one variable.
  std::vector<int> delta_of_value_indices_;
  //MDD
  MyMDD mdd_;
  //temporary storage for deleted edges
  std::vector<int> edges_to_remove_;
  //set of tuple for the visitor
  const IntTupleSet tuple_set_;

  int* shared_positions_edges_;

  //for the reset
  std::vector<int>* up;
  std::vector<int>* down;
  std::vector<int>* edges;
  std::vector<int>* tmp;

};
}  //namespace

// External API.
Constraint* BuildAc4MddResetTableConstraint(Solver* const solver,
                                            const IntTupleSet& tuples,
                                            const std::vector<IntVar*>& vars) {
  return solver->RevAlloc(new Ac4MddTableConstraint(solver, tuples, vars));
}

}  // namespace operations_research
