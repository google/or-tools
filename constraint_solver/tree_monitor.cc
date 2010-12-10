// Copyright 2010 Google
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

#include <limits>
#include "base/concise_iterator.h"
#include "base/stl_util-inl.h"
#include "constraint_solver/constraint_solver.h"
#include <iostream>
#include <fstream>
#include "util/xml_helper.h"

namespace operations_research {

class XmlHelper;

class TreeNode;

// The TreeMonitor may be attached to a search to obtain an output in CPViz
// format (http://sourceforge.net/projects/cpviz/). It produces both the
// Tree XML file as well as the Visualization XML. CPViz can then be used
// to obtain an overview of the search and to gain an insight into the decision
// phase.
// While TreeMonitor collects information during the runtime of a search, the
// output is only done after the search completes.
// The TreeMonitor output is optimized for output using the Viz tool included
// in CPViz. A dummy node is automatically added as a search root to allow a
// change of the root variable during the search, as CPViz currently does
// not support this.
class TreeMonitor: public SearchMonitor {
 public:
  typedef hash_map<string, IntVar const*> IntVarMap;

  TreeMonitor(Solver* const solver, const IntVar* const* vars, int size,
              string const& filename_tree, string const& filename_visualizer);

  TreeMonitor(Solver* const solver, const IntVar* const* vars, int size,
              string* const tree_xml, string* const visualization_xml);

  ~TreeMonitor();

  // Callback for the beginning of the search.
  virtual void EnterSearch();

  // Callback called after each decision, but before any variables are changed.
  // The decision is empty if a solution has been reached.
  virtual void EndNextDecision(DecisionBuilder* const decision_builder,
                               Decision* const decision);

  // Callback for the end of the search.
  virtual void ExitSearch();

  // Returns the XML of the current tree.
  virtual string DebugString() const;

  // Generates and returns the Tree XML file for CPVIZ.
  string GenerateTreeXML() const;

  // Generates and returns the Visualization XML file for CPVIZ.
  string GenerateVisualizationXML() const;

  // Callback called to indicate that the solver goes up one level in the
  // search tree. This is also used to restart the search at a parent node
  // after a solution is found.
  virtual void RefuteDecision(Decision* const decision);

 private:
  // Strips the additional descriptions from IntVar and returns the
  // original name.
  static string BaseName(string const& name);

  // Registers vars and sets Min and Max accordingly.
  void Init(const IntVar* const* vars, int size);

  TreeNode* current_node_;
  const string filename_tree_;
  const string filename_visualizer_;
  int id_counter_;
  string last_variable_;
  int64 min_;
  int64 max_;
  scoped_ptr<TreeNode> root_node_;
  hash_map<string, int64> last_value_;
  int search_level_;
  IntVarMap vars_;
  string* const tree_xml_;
  string* const visualization_xml_;
};

SearchMonitor* Solver::MakeTreeMonitorString(const IntVar* const* vars,
                                             int size, string* const tree_xml,
                                             string* const visualization_xml) {
  return RevAlloc(new TreeMonitor(this, vars, size, tree_xml,
                                  visualization_xml));
}

SearchMonitor* Solver::MakeTreeMonitorString(const vector<IntVar*>& vars,
                                             string* const tree_xml,
                                             string* const visualization_xml) {
  return RevAlloc(new TreeMonitor(this, vars.data(), vars.size(), tree_xml,
                                  visualization_xml));
}

SearchMonitor* Solver::MakeTreeMonitor(const IntVar* const* vars, int size,
                                       string const& file_tree,
                                       string const& file_visualization) {
  return RevAlloc(new TreeMonitor(this, vars, size, file_tree,
                                  file_visualization));
}

SearchMonitor* Solver::MakeTreeMonitor(const vector<IntVar*>& vars,
                                       string const& file_tree,
                                       string const& file_visualization) {
  return RevAlloc(new TreeMonitor(this, vars.data(), vars.size(), file_tree,
                                  file_visualization));
}

// Represents a node in the decision phase. Can either be the root node, a
// successful attempt, a failure or a solution.
class TreeNode {
 public:
  typedef hash_map<string, vector<int64> > DomainMap;
  enum TreeNodeType { ROOT, TRY, FAIL, SOLUTION };

  TreeNode(TreeNode* parent, int id)
    : id_(id),
      name_(""),
      node_type_(TRY),
      parent_(parent) {}

  ~TreeNode() {
    STLDeleteElements(&children_);
  }

  // Gets the value of a decision's branch.
  int64 branch_value(int branch) const { return branch_values_[branch]; }

  // Returns a pointer to the domain of all variables.
  const DomainMap& domain() const { return domain_; }

  // Sets the domain for all variables.
  void SetDomain(TreeMonitor::IntVarMap const& vars) {
    domain_.clear();

    for (ConstIter<TreeMonitor::IntVarMap> it(vars); !it.at_end(); ++it) {
      vector<int64> domain;

      scoped_ptr<IntVarIterator> intvar_it(
          it->second->MakeDomainIterator(false));

      for (intvar_it->Init(); intvar_it->Ok(); intvar_it->Next()) {
        domain.push_back(intvar_it->Value());
      }

      domain_[it->first] = domain;
    }
  }

  // Sets the domain for all variables.
  void SetDomain(DomainMap const& domain) { domain_ = domain; }

  // Returns the ID of the current node
  int id() const { return id_; }

  // Returns the name of the variable of the current decision.
  string const& name() const { return name_; }

  // Sets the name of the variable for the current decision.
  void set_name(string const& name) { name_ = name; }

  // Gets the node type.
  TreeNodeType node_type() const { return node_type_; }

  // Sets the node type.
  void set_node_type(TreeNodeType node_type) { node_type_ = node_type; }

  // Returns the parent node or NULL if node has no parent.
  TreeNode* parent() const { return parent_; }

  // Return the first child or NULL if it does not exist
  TreeNode const* FirstChild() const {
    return children_.size() ? children_[0] : NULL;
  }

  // Checks whether the provided domain matches the domain of the node.
  // Disregards changes of the currently active variable.
  bool DomainEquals(TreeMonitor::IntVarMap const& vars) {
    for (ConstIter<TreeMonitor::IntVarMap> it(vars); !it.at_end(); ++it) {
      // Do not check changes in the current variable, as we want to skip
      // a possible change of the decision variable to see if other variables
      // have changed.
      if (it->first == name_) {
        continue;
      }

      int counter = 0;
      scoped_ptr<IntVarIterator> intvar_it(
          it->second->MakeDomainIterator(false));

      for (intvar_it->Init(); intvar_it->Ok(); intvar_it->Next()) {
        if (domain_[it->first][counter++] != intvar_it->Value()) {
          return false;
        }
      }

      if (counter != (domain_[it->first]).size()) {
        return false;
      }
    }
    return true;
  }

  // Adds a new child, initializes it and returns the corresponding pointer.
  bool AddChild(int id, string name, hash_map<string, int64> const& last_value,
                TreeMonitor::IntVarMap const& vars, TreeNode** child) {
    CHECK_NOTNULL(child);

    for (int i = 0; i < children_.size(); ++i) {
      // Reuse the branch if the domains match.
      if (children_[i]->DomainEquals(vars)) {
        *child = children_[i];
        return false;
      }
    }

    TreeNode* tree_node(new TreeNode(this, id));
    tree_node->set_name(name);
    tree_node->SetDomain(vars);
    children_.push_back(tree_node);
    branch_values_.push_back(last_value.find(name_)->second);
    *child = tree_node;
    return true;
  }

  // Starting at this node, prints the complete Visualization XML for cpviz.
  void GenerateVisualizationXML(XmlHelper* const visualization_writer) {
    CHECK_NOTNULL(visualization_writer);

    // The root node referes to the imaginary tree node '-1'.
    const int kRootTreeNodeId = -1;
    // There currently is only support for one visualizer.
    const int kVisualizerState = 1;

    visualization_writer->StartElement("state");
    visualization_writer->AddAttribute("id", id_);
    visualization_writer->AddAttribute("tree_node",
                                       id_ ? id_ : kRootTreeNodeId);
    visualization_writer->StartElement("visualizer_state");
    visualization_writer->AddAttribute("id", kVisualizerState);

    const DomainMap domain = parent_ ?  parent_->domain() : domain_;

    for (ConstIter<DomainMap> it(domain); !it.at_end(); ++it) {
      vector<int64> current = it->second;
      visualization_writer->StartElement(current.size() == 1 ?
                                         "integer" :
                                         "dvar");
      visualization_writer->AddAttribute("index", it->first);

      if (current.size() > 1
          && current.size() == (current.back() - current[0] + 1)) {
        // Use %d .. %d format.
        visualization_writer->AddAttribute(
            "domain",
            StringPrintf("%" GG_LL_FORMAT "d .. %" GG_LL_FORMAT "d",
                         current[0], current.back()));
      } else {
        // Use list of integers
        string domain;

        for (int j = 0; j < current.size(); ++j) {
          StringAppendF(&domain, " %" GG_LL_FORMAT "d", current[j]);
        }

        visualization_writer->AddAttribute(current.size() == 1 ?
                                           "value" :
                                           "domain", domain.substr(1));
      }

      visualization_writer->EndElement();  // dvar or integer
    }

    if (node_type_ == FAIL) {
      visualization_writer->StartElement("failed");
      visualization_writer->AddAttribute("index", name_);
      visualization_writer->AddAttribute(
          "value",
          StringPrintf("%" GG_LL_FORMAT "d", parent_->branch_value(0)));
      visualization_writer->EndElement();  // failed
    } else if (node_type_ == TRY) {
      visualization_writer->StartElement("focus");
      visualization_writer->AddAttribute("index", name_);
      visualization_writer->EndElement();  // focus
    }

    visualization_writer->EndElement();  // visualizer_state
    visualization_writer->EndElement();  // state

    for (int i = 0; i < children_.size(); ++i) {
      children_[i]->GenerateVisualizationXML(visualization_writer);
    }
  }

  // Starting at this node, prints the complete Tree XML for cpviz.
  void GenerateTreeXML(XmlHelper* const tree_writer) {
    CHECK_NOTNULL(tree_writer);

    // The solution element is preceeded by a try element.
    const char* kElementName[] = { "root", "try", "fail", "try" };

    if (node_type_ == ROOT) {
      tree_writer->StartElement(kElementName[node_type_]);
      tree_writer->AddAttribute("id", id_);
      tree_writer->EndElement();
    }

    for (int i = 0; i < children_.size(); ++i) {
      TreeNode* child = children_[i];
      tree_writer->StartElement(kElementName[child->node_type_]);
      tree_writer->AddAttribute("id", child->id_);
      tree_writer->AddAttribute("parent", id());
      tree_writer->AddAttribute("name", name());

      if (name().empty()) {
        tree_writer->AddAttribute("size", "0");
        tree_writer->AddAttribute("value", "0");
      } else {
        // Use the original size of the first child if available
        const DomainMap domain = parent_ && parent_->children_.size() ?
                                 parent_->children_[0]->domain() :
                                 domain_;
        tree_writer->AddAttribute(
            "size",  StringPrintf("%zu", FindOrDie(domain, name_).size()));
        tree_writer->AddAttribute("value", StringPrintf("%" GG_LL_FORMAT "d",
                                                        branch_values_[i]));
      }

      tree_writer->EndElement();

      if (child->node_type_ == SOLUTION) {
        // CPVIZ requires an additional node to indicate success.
        tree_writer->StartElement("succ");
        tree_writer->AddAttribute("id", child->id_);
        tree_writer->EndElement();
      }

      children_[i]->GenerateTreeXML(tree_writer);
    }
  }

 private:
  vector<int64> branch_values_;
  vector<TreeNode*> children_;
  DomainMap domain_;
  const int id_;
  string name_;
  TreeNodeType node_type_;
  TreeNode* const parent_;
};

TreeMonitor::TreeMonitor(Solver* const solver, const IntVar* const* vars,
                         int size, string const& filename_tree,
                         string const& filename_visualizer)
    : SearchMonitor(solver),
      current_node_(NULL),
      filename_tree_(filename_tree),
      filename_visualizer_(filename_visualizer),
      root_node_(NULL),
      search_level_(0),
      tree_xml_(NULL),
      visualization_xml_(NULL) {
  CHECK_NOTNULL(solver);
  CHECK_NOTNULL(vars);

  Init(vars, size);
}

TreeMonitor::TreeMonitor(Solver* const solver, const IntVar* const* vars,
                         int size, string* const tree_xml,
                         string* const visualization_xml)
    : SearchMonitor(solver),
      current_node_(NULL),
      filename_tree_(""),
      filename_visualizer_(""),
      root_node_(NULL),
      search_level_(0),
      tree_xml_(tree_xml),
      visualization_xml_(visualization_xml) {
  CHECK_NOTNULL(solver);
  CHECK_NOTNULL(vars);
  CHECK_NOTNULL(tree_xml);
  CHECK_NOTNULL(visualization_xml);

  Init(vars, size);
}

TreeMonitor::~TreeMonitor() {}

void TreeMonitor::Init(const IntVar* const* vars, int size) {
  min_ = std::numeric_limits<int64>::max();
  max_ = std::numeric_limits<int64>::min();

  // Obtain min and max information from variables.
  for (int i = 0; i < size; ++i) {
    min_ = std::min(min_, vars[i]->Min());
    max_ = std::max(max_, vars[i]->Max());

    string name = BaseName(vars[i]->name());

    if (name.empty()) {
      name = StringPrintf("%d", i);
    }

    vars_[name] = vars[i];
  }
}

void TreeMonitor::EnterSearch() {
  if (!root_node_.get()) {
    id_counter_ = 0;
    root_node_.reset(new TreeNode(NULL, id_counter_++));
    root_node_->set_node_type(TreeNode::ROOT);
    root_node_->SetDomain(vars_);
    current_node_ = root_node_.get();
    last_value_[""] = 0;  // The root node's value is always zero.
  }

  ++search_level_;

  VLOG(1) << "Current search level " << search_level_;
}

void TreeMonitor::EndNextDecision(DecisionBuilder* const decision_builder,
                                  Decision* const decision) {
  const string kDomainStartToken = "(";
  const string kDomainEndToken = ") == ";

  if (decision) {
    string value_str;

    // Extract the required data from the DebugString, as there is no obvious
    // way to extract the name and the value of the variable affected by the
    // decision.
    // TODO(user): Find better solution.
    // Debug string is "[Name(Domain) == Value]".
    string debug_string = decision->DebugString();

    // Get the name of the variable.
    size_t pos = debug_string.find(kDomainStartToken);
    if (pos != string::npos) {
      last_variable_ = debug_string.substr(1, pos - 1);
    }

    // Get the value of the variable.
    pos = debug_string.find(kDomainEndToken);

    if (pos != string::npos) {
      string value_str = debug_string.substr(pos + 5,
                                             debug_string.length() - pos - 6);

      last_value_[last_variable_]  = atol(value_str.c_str());
    }
  }

  if (current_node_->AddChild(id_counter_, last_variable_, last_value_, vars_,
                              &current_node_)) {
    ++id_counter_;
  }

  if (!decision) {
    current_node_->set_node_type(TreeNode::SOLUTION);
  }
}

string TreeMonitor::BaseName(string const& name) {
  // Some IntVar desciptors return "Var(Name(DebugString))".
  // Extract the name.
  int start = name.find('(');

  if (start != string::npos) {
    int end = name.find('(', start + 1);

    if (end != string::npos) {
      return name.substr(start+1, end-start-1);
    }
  }

  return name;
}

void TreeMonitor::RefuteDecision(Decision* const decision) {
  // Called when the solver goes up one level in the tree and undos a
  // change in the tree. As we have added multiple levels for both 'fail'
  // and 'success', we have to go up two levels in some cases.
  CHECK_NOTNULL(decision);

  if (current_node_->node_type() == TreeNode::SOLUTION) {
    // Solver calls RefuteDecision even on success if it looks for
    // more than one solution.
    // Just go back to the previous decision.
    current_node_ = current_node_->parent();
  } else if (current_node_->node_type() == TreeNode::TRY
             && current_node_->id() == id_counter_ -1) {
    // Add an extra node in case of a failure, so we can see the failed
    // decision.
    current_node_->set_node_type(TreeNode::TRY);

    if (current_node_->AddChild(id_counter_, current_node_->parent()->name(),
                                last_value_, vars_, &current_node_)) {
      ++id_counter_;
    }

    current_node_->set_node_type(TreeNode::FAIL);
    current_node_ = current_node_->parent();
  }

  current_node_ = current_node_->parent();
}

string TreeMonitor::GenerateTreeXML() const {
  XmlHelper xml_writer;
  xml_writer.StartDocument();

  xml_writer.StartElement("tree");
  xml_writer.AddAttribute("version", "1.0");
  xml_writer.AddAttribute("xmlns:xsi",
                          "http://www.w3.org/2001/XMLSchema-instance");
  xml_writer.AddAttribute("xsi:noNamespaceSchemaLocation", "tree.xsd");

  root_node_->GenerateTreeXML(&xml_writer);

  xml_writer.EndElement();  // End of element: tree
  xml_writer.EndDocument();

  return xml_writer.GetContent();
}

string TreeMonitor::GenerateVisualizationXML() const {
  XmlHelper xml_writer;
  xml_writer.StartDocument();

  xml_writer.StartElement("visualization");
  xml_writer.AddAttribute("version", "1.0");
  xml_writer.AddAttribute("xmlns:xsi",
                          "http://www.w3.org/2001/XMLSchema-instance");
  xml_writer.AddAttribute("xsi:noNamespaceSchemaLocation", "visualization.xsd");

  xml_writer.StartElement("visualizer");
  xml_writer.AddAttribute("id", 1);
  xml_writer.AddAttribute("type", "vector");
  xml_writer.AddAttribute("display", "expanded");
  xml_writer.AddAttribute("min", StringPrintf("%" GG_LL_FORMAT "d", min_));
  xml_writer.AddAttribute("max", StringPrintf("%" GG_LL_FORMAT "d", max_));
  xml_writer.EndElement();  // End of element: visualizer

  root_node_->GenerateVisualizationXML(&xml_writer);

  xml_writer.EndElement();  // End of element: visualization
  xml_writer.EndDocument();

  return xml_writer.GetContent();
}

string TreeMonitor::DebugString() const {
  return StringPrintf("TreeMonitor:\n%s", GenerateTreeXML().c_str());
}

void TreeMonitor::ExitSearch() {
  --search_level_;

  VLOG(1) << "Current search level " << search_level_;

  if (!search_level_) {
    // If search has compeleted and a filename is specified, automatically
    // output the XML to these files.
    if (!filename_tree_.empty()) {
      std::ofstream file_tree_(filename_tree_.c_str());

      if (file_tree_.is_open()) {
        file_tree_ << GenerateTreeXML().c_str();
        file_tree_.close();
      } else {
        LG << "Failed to gain write access to file: " << filename_tree_;
      }

      std::ofstream file_visualizer_(filename_visualizer_.c_str());

      if (file_visualizer_.is_open()) {
        file_visualizer_ << GenerateVisualizationXML().c_str();
        file_visualizer_.close();
      } else {
        LG << "Failed to gain write access to file: " << filename_tree_;
      }
    } else {
      *tree_xml_ = GenerateTreeXML();
      *visualization_xml_ = GenerateVisualizationXML();
    }
  }
}
}  // namespace
