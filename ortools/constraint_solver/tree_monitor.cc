// Copyright 2010-2014 Google
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


#include <algorithm>
#include <unordered_map>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/file.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include <fstream>
#include <iostream>
#include "ortools/util/xml_helper.h"

namespace operations_research {
// String comparator that compares strings naturally, even those
// including integer numbers.
struct NaturalLess {
  bool operator()(const std::string& s1, const std::string& s2) const {
    int start = 0;
    int length = std::min(s1.length(), s2.length());

    while (start < length) {
      // Ignore common characters at the beginning.
      while (start < length && s1[start] == s2[start] &&
             (s1[start] < '0' || s1[start] > '9')) {
        ++start;
      }

      // If one std::string is the substring of another, then the shorter std::string is
      // smaller.
      if (start == length) {
        return s1.length() < s2.length();
      }

      int number_s1 = 0;
      int number_s2 = 0;

      // Extract a number if we have one.
      for (int i = start; i < s1.length() && s1[i] >= '0' && s1[i] <= '9';
           ++i) {
        number_s1 = number_s1 * 10 + (s1[i] - '0');
      }

      for (; start < s2.length() && s2[start] >= '0' && s2[start] <= '9';
           ++start) {
        number_s2 = number_s2 * 10 + (s2[start] - '0');
      }

      // Do a numerical comparison only if there are two numbers.
      if (number_s1 && number_s2) {
        // If we have similar numbers followed by other characters, we have to
        // check the rest of the std::string.
        if (number_s1 != number_s2) {
          return number_s1 < number_s2;
        }
      } else {
        return s1.compare(s2) < 0;
      }
    }

    return s1.length() < s2.length();
  }
};

bool CompareStringsUsingNaturalLess(const std::string& s1, const std::string& s2) {
  return NaturalLess()(s1, s2);
}

class XmlHelper;

namespace {
const char* kConfigXml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<configuration version=\"1.0\">\n"
    "    <tool show=\"tree\" fileroot=\"tree\" display=\"expanded\""
    " repeat=\"all\"/>\n"
    "    <tool show=\"viz\" fileroot=\"viz\" repeat=\"all\"/>\n"
    "</configuration>";


class TreeNode;

// TreeDecisionVisitor is used to gain access to the variables and values
// involved in a decision.
class TreeDecisionVisitor : public DecisionVisitor {
 public:
  TreeDecisionVisitor() {}
  ~TreeDecisionVisitor() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
    name_ = var->name();
    value_ = value;
    valid_ = true;
  }

  void VisitSplitVariableDomain(IntVar* const var, int64 value,
                                bool start_with_lower_half) override {
    name_ = var->name();
    value_ = value;
    valid_ = true;
  }

  void VisitScheduleOrPostpone(IntervalVar* const var, int64 est) override {
    name_ = var->name();
    value_ = est;
    valid_ = true;
  }

  virtual void VisitTryRankFirst(SequenceVar* const sequence, int index) {
    name_ = sequence->name();
    value_ = index;
    valid_ = true;
  }

  virtual void VisitTryRankLast(SequenceVar* const sequence, int index) {
    name_ = sequence->name();
    value_ = index;
    valid_ = true;
  }

  void VisitUnknownDecision() override { valid_ = false; }

  // Indicates whether name and value can be called.
  bool valid() { return valid_; }

  // Returns the name of the current variable.
  const std::string& name() {
    CHECK(valid_);
    return name_;
  }

  // Returns the value of the current variable.
  int64 value() {
    CHECK(valid_);
    return value_;
  }

  std::string DebugString() const override { return "TreeDecisionVisitor"; }

 private:
  std::string name_;
  int64 value_;
  bool valid_;
};

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
class TreeMonitor : public SearchMonitor {
 public:
  typedef std::unordered_map<std::string, IntVar const*> IntVarMap;

  TreeMonitor(Solver* const solver, const IntVar* const* vars, int size,
              const std::string& filename_tree, const std::string& filename_visualizer);

  TreeMonitor(Solver* const solver, const IntVar* const* vars, int size,
              std::string* const tree_xml, std::string* const visualization_xml);

  TreeMonitor(Solver* const solver, const IntVar* const* vars, int size,
              const std::string& filename_config, const std::string& filename_tree,
              const std::string& filename_visualizer);

  TreeMonitor(Solver* const solver, const IntVar* const* vars, int size,
              std::string* const config_xml, std::string* const tree_xml,
              std::string* const visualization_xml);

  ~TreeMonitor() override;

  // Callback for the beginning of the search.
  void EnterSearch() override;

  // Callback called after each decision, but before any variables are changed.
  // The decision is empty if a solution has been reached.
  void EndNextDecision(DecisionBuilder* const decision_builder,
                       Decision* const decision) override;
  // Callback for the end of the search.
  void ExitSearch() override;

  // Returns the XML of the current tree.
  std::string DebugString() const override;

  // Generates and returns the Tree XML file for CPVIZ.
  std::string GenerateTreeXML() const;

  // Generates and returns the Visualization XML file for CPVIZ.
  std::string GenerateVisualizationXML() const;

  // Callback called to indicate that the solver goes up one level in the
  // search tree. This is also used to restart the search at a parent node
  // after a solution is found.
  void RefuteDecision(Decision* const decision) override;

  // Strips characters that cause problems with CPViz from attributes
  static std::string StripSpecialCharacters(std::string attribute);

 private:
  // Registers vars and sets Min and Max accordingly.
  void Init(const IntVar* const* vars, int size);

  std::string* const config_xml_;
  TreeNode* current_node_;
  const std::string filename_config_;
  const std::string filename_tree_;
  const std::string filename_visualizer_;
  int id_counter_;
  std::string last_decision_;
  std::unordered_map<std::string, int64> last_value_;
  std::string last_variable_;
  int64 min_;
  int64 max_;
  std::unique_ptr<TreeNode> root_node_;
  int search_level_;
  std::string* const tree_xml_;
  IntVarMap vars_;
  std::string* const visualization_xml_;
};

// Represents a node in the decision phase. Can either be the root node, a
// successful attempt, a failure or a solution.
class TreeNode {
 public:
  typedef std::map<std::string, std::vector<int64>, NaturalLess> DomainMap;
  enum TreeNodeType {
    ROOT,
    TRY,
    FAIL,
    SOLUTION
  };

  TreeNode(TreeNode* parent, int id)
      : cycles_(1), id_(id), name_(""), node_type_(TRY), parent_(parent) {}

  ~TreeNode() { STLDeleteElements(&children_); }

  // Gets the value of a decision's branch.
  int64 branch_value(int branch) const { return branch_values_[branch]; }

  // Returns a pointer to the domain of all variables.
  const DomainMap& domain() const { return domain_; }

  // Sets the domain for all variables.
  void SetDomain(TreeMonitor::IntVarMap const& vars) {
    domain_.clear();

    for (const auto& it : vars) {
      std::vector<int64> domain;

      std::unique_ptr<IntVarIterator> intvar_it(
          it.second->MakeDomainIterator(false));

      for (const int64 value : InitAndGetValues(intvar_it.get())) {
        domain.push_back(value);
      }

      domain_[it.first] = domain;
    }
  }

  // Sets the domain for all variables.
  void SetDomain(DomainMap const& domain) { domain_ = domain; }

  // Returns the ID of the current node
  int id() const { return id_; }

  // Returns the name of the variable of the current decision.
  const std::string& name() const { return name_; }

  // Sets the name of the variable for the current decision.
  void set_name(const std::string& name) { name_ = name; }

  // Gets the node type.
  TreeNodeType node_type() const { return node_type_; }

  // Sets the node type.
  void set_node_type(TreeNodeType node_type) { node_type_ = node_type; }

  // Returns the parent node or nullptr if node has no parent.
  TreeNode* Parent() { return --cycles_ ? this : parent_; }

  // Adds a cycle instead of duplicate nodes.
  void AddCycle() { cycles_++; }

  // Adds a new child, initializes it and returns the corresponding pointer.
  bool AddChild(int id, const std::string& name,
                std::unordered_map<std::string, int64> const& last_value, bool is_final_node,
                TreeMonitor::IntVarMap const& vars, TreeNode** child) {
    CHECK(child != nullptr);

    if (!is_final_node) {
      for (int i = 0; i < children_.size(); ++i) {
        // Reuse existing branch if possible
        if (children_[i]->name_ == name &&
            branch_values_[i] == FindOrDie(last_value, name_)) {
          children_[i]->AddCycle();
          *child = children_[i];
          return false;
        }
      }
    }

    TreeNode* tree_node(new TreeNode(this, id));
    tree_node->set_name(name);
    tree_node->SetDomain(vars);
    children_.push_back(tree_node);
    branch_values_.push_back(FindOrDie(last_value, name_));
    *child = tree_node;

    return true;
  }

  // Starting at this node, prints the complete Visualization XML for cpviz.
  void GenerateVisualizationXML(XmlHelper* const visualization_writer) {
    CHECK(visualization_writer != nullptr);

    // There currently is only support for one visualizer.
    const int kVisualizerState = 0;

    visualization_writer->StartElement("state");
    visualization_writer->AddAttribute("id", id_);
    visualization_writer->AddAttribute("tree_node", id_);
    visualization_writer->StartElement("visualizer_state");
    visualization_writer->AddAttribute("id", kVisualizerState);

    int index = 0;
    int name = -1;

    for (const auto& it : domain_) {
      std::vector<int64> current = it.second;
      visualization_writer->StartElement(current.size() == 1 ? "integer"
                                                             : "dvar");
      visualization_writer->AddAttribute("index", ++index);

      if (it.first == name_) {
        name = index;
      }

      if (current.size() > 1 &&
          current.size() == (current.back() - current[0] + 1)) {
        // Use %d .. %d format.
        visualization_writer->AddAttribute(
            "domain", StringPrintf("%" GG_LL_FORMAT "d .. %" GG_LL_FORMAT "d",
                                   current[0], current.back()));
      } else {
        // Use list of integers
        std::string domain;

        for (int j = 0; j < current.size(); ++j) {
          StringAppendF(&domain, " %" GG_LL_FORMAT "d", current[j]);
        }

        visualization_writer->AddAttribute(
            current.size() == 1 ? "value" : "domain", domain.substr(1));
      }

      visualization_writer->EndElement();  // dvar or integer
    }

    if (node_type_ == FAIL) {
      visualization_writer->StartElement("failed");
      visualization_writer->AddAttribute("index", name);
      visualization_writer->AddAttribute("value",
                                         StrCat(parent_->branch_value(0)));
      visualization_writer->EndElement();  // failed
    } else if (node_type_ == TRY) {
      visualization_writer->StartElement("focus");
      visualization_writer->AddAttribute("index", name);
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
    CHECK(tree_writer != nullptr);

    // The solution element is preceeded by a try element.
    const char* kElementName[] = {"root", "try", "fail", "try"};

    if (node_type_ == ROOT) {
      tree_writer->StartElement(kElementName[node_type_]);
      tree_writer->AddAttribute("id", id_);
      tree_writer->EndElement();
    }

    for (int i = 0; i < children_.size(); ++i) {
      TreeNode* child = children_[i];
      tree_writer->StartElement(kElementName[child->node_type_]);
      tree_writer->AddAttribute("id", child->id_);
      tree_writer->AddAttribute("parent", id_);
      tree_writer->AddAttribute("name",
                                TreeMonitor::StripSpecialCharacters(name_));

      if (name_.empty()) {
        tree_writer->AddAttribute("size", "0");
        tree_writer->AddAttribute("value", "0");
      } else {
        // Use the original size of the first child if available
        const DomainMap& domain = parent_ && !parent_->children_.empty()
                                      ? parent_->children_[0]->domain()
                                      : domain_;

        const std::vector<int64>* const domain_values =
            FindOrNull(domain, name_);
        if (domain_values) {
          tree_writer->AddAttribute("size", StrCat(domain_values->size()));
        } else {
          tree_writer->AddAttribute("size", "unknown");
        }
        tree_writer->AddAttribute("value", StrCat(branch_values_[i]));
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
  std::vector<int64> branch_values_;
  std::vector<TreeNode*> children_;
  int cycles_;
  DomainMap domain_;
  const int id_;
  std::string name_;
  TreeNodeType node_type_;
  TreeNode* const parent_;
};

TreeMonitor::TreeMonitor(Solver* const solver, const IntVar* const* vars,
                         int size, const std::string& filename_tree,
                         const std::string& filename_visualizer)
    : SearchMonitor(solver),
      config_xml_(nullptr),
      current_node_(nullptr),
      filename_config_(""),
      filename_tree_(filename_tree),
      filename_visualizer_(filename_visualizer),
      search_level_(0),
      tree_xml_(nullptr),
      visualization_xml_(nullptr) {
  CHECK(solver != nullptr);
  CHECK(vars != nullptr);

  Init(vars, size);
}

TreeMonitor::TreeMonitor(Solver* const solver, const IntVar* const* vars,
                         int size, std::string* const tree_xml,
                         std::string* const visualization_xml)
    : SearchMonitor(solver),
      config_xml_(nullptr),
      current_node_(nullptr),
      filename_config_(""),
      filename_tree_(""),
      filename_visualizer_(""),
      search_level_(0),
      tree_xml_(tree_xml),
      visualization_xml_(visualization_xml) {
  CHECK(solver != nullptr);
  CHECK(vars != nullptr);
  CHECK(tree_xml != nullptr);
  CHECK(visualization_xml != nullptr);

  Init(vars, size);
}

TreeMonitor::TreeMonitor(Solver* const solver, const IntVar* const* vars,
                         int size, const std::string& filename_config,
                         const std::string& filename_tree,
                         const std::string& filename_visualizer)
    : SearchMonitor(solver),
      config_xml_(nullptr),
      current_node_(nullptr),
      filename_config_(filename_config),
      filename_tree_(filename_tree),
      filename_visualizer_(filename_visualizer),
      search_level_(0),
      tree_xml_(nullptr),
      visualization_xml_(nullptr) {
  CHECK(solver != nullptr);
  CHECK(vars != nullptr);

  Init(vars, size);
}

TreeMonitor::TreeMonitor(Solver* const solver, const IntVar* const* vars,
                         int size, std::string* const config_xml,
                         std::string* const tree_xml,
                         std::string* const visualization_xml)
    : SearchMonitor(solver),
      config_xml_(config_xml),
      current_node_(nullptr),
      filename_config_(""),
      filename_tree_(""),
      filename_visualizer_(""),
      search_level_(0),
      tree_xml_(tree_xml),
      visualization_xml_(visualization_xml) {
  CHECK(solver != nullptr);
  CHECK(vars != nullptr);
  CHECK(config_xml != nullptr);
  CHECK(tree_xml != nullptr);
  CHECK(visualization_xml != nullptr);

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

    std::string name = vars[i]->name();

    if (name.empty()) {
      name = StrCat(i);
    }

    vars_[name] = vars[i];
  }
}

void TreeMonitor::EnterSearch() {
  if (!root_node_.get()) {
    id_counter_ = 0;
    root_node_.reset(new TreeNode(nullptr, id_counter_++));
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
  if (decision) {
    TreeDecisionVisitor visitor;
    decision->Accept(&visitor);

    if (visitor.valid()) {
      last_variable_ = visitor.name();
      last_value_[last_variable_] = visitor.value();
    }
  }

  if (!decision || decision->DebugString() != last_decision_) {
    if (current_node_->AddChild(id_counter_, last_variable_, last_value_,
                                !decision, vars_, &current_node_)) {
      ++id_counter_;
    }
  } else {
    current_node_->AddCycle();
  }

  last_decision_ = decision ? decision->DebugString() : "";

  if (!decision) {
    current_node_->set_node_type(TreeNode::SOLUTION);
  }
}

void TreeMonitor::RefuteDecision(Decision* const decision) {
  // Called when the solver goes up one level in the tree and undos a
  // change in the tree. As we have added multiple levels for both 'fail'
  // and 'success', we have to go up two levels in some cases.
  CHECK(decision != nullptr);

  if (current_node_->node_type() == TreeNode::SOLUTION) {
    // Solver calls RefuteDecision even on success if it looks for
    // more than one solution.
    // Just go back to the previous decision.
    current_node_ = current_node_->Parent();
  } else if (current_node_->node_type() == TreeNode::TRY) {
    // Add an extra node in case of a failure, so we can see the failed
    // decision.
    current_node_->set_node_type(TreeNode::TRY);

    if (current_node_->AddChild(id_counter_, last_variable_, last_value_, true,
                                vars_, &current_node_)) {
      ++id_counter_;
    }

    current_node_->set_node_type(TreeNode::FAIL);
    current_node_ = current_node_->Parent();
  }

  current_node_ = current_node_->Parent();
}

std::string TreeMonitor::GenerateTreeXML() const {
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

std::string TreeMonitor::GenerateVisualizationXML() const {
  XmlHelper xml_writer;
  xml_writer.StartDocument();

  xml_writer.StartElement("visualization");
  xml_writer.AddAttribute("version", "1.0");
  xml_writer.AddAttribute("xmlns:xsi",
                          "http://www.w3.org/2001/XMLSchema-instance");
  xml_writer.AddAttribute("xsi:noNamespaceSchemaLocation", "visualization.xsd");

  xml_writer.StartElement("visualizer");
  xml_writer.AddAttribute("id", 0);
  xml_writer.AddAttribute("type", "vector");
  xml_writer.AddAttribute("display", "expanded");
  xml_writer.AddAttribute("min", StrCat(min_));
  xml_writer.AddAttribute("max", StrCat(max_));
  xml_writer.AddAttribute("width", StrCat(vars_.size()));
  xml_writer.AddAttribute("height", StrCat(max_ - min_ + 1));
  xml_writer.EndElement();  // End of element: visualizer

  root_node_->GenerateVisualizationXML(&xml_writer);

  xml_writer.EndElement();  // End of element: visualization
  xml_writer.EndDocument();

  return xml_writer.GetContent();
}

std::string TreeMonitor::DebugString() const {
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
        LOG(INFO) << "Failed to gain write access to file: " << filename_tree_;
      }

      std::ofstream file_visualizer_(filename_visualizer_.c_str());

      if (file_visualizer_.is_open()) {
        file_visualizer_ << GenerateVisualizationXML().c_str();
        file_visualizer_.close();
      } else {
        LOG(INFO) << "Failed to gain write access to file: " << filename_tree_;
      }

      if(!filename_config_.empty()) {
        std::ofstream file_config_(filename_config_.c_str());

        if (file_config_.is_open()) {
          file_config_ << kConfigXml;
          file_config_.close();
        } else {
          LOG(INFO) << "Failed to gain write access to file: " <<
      filename_config_;
        }
      }
    } else {
      CHECK(tree_xml_ != nullptr);
      *tree_xml_ = GenerateTreeXML();

      CHECK(visualization_xml_ != nullptr);
      *visualization_xml_ = GenerateVisualizationXML();

      if (config_xml_) {
        *config_xml_ = kConfigXml;
      }
    }
  }
}

std::string TreeMonitor::StripSpecialCharacters(std::string attribute) {
  // Numbers, characters, dashes, underscored, brackets, colons, slashes,
  // periods, question marks, and parentheses are allowed
  const char* kAllowedCharacters = "0123456789abcdefghijklmnopqrstuvwxyz"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ_-[]:/.?()";

  std::set<char> character_set;

  char* allowed = const_cast<char*>(kAllowedCharacters);

  while (*allowed) {
    character_set.insert(*(allowed++));
  }

  for (int i = 0; i < attribute.length(); ++i) {
    if (character_set.find(attribute[i]) == character_set.end()) {
      attribute.replace(i, 1, "_");
    }
  }

  return attribute;
}
}  // namespace

// For tests

// Strips characters that cause problems with CPViz from attributes
std::string TreeMonitorStripSpecialCharacters(std::string attribute) {
  return TreeMonitor::StripSpecialCharacters(std::move(attribute));
}

// ----- API ----

SearchMonitor* Solver::MakeTreeMonitor(const std::vector<IntVar*>& vars,
                                       const std::string& file_tree,
                                       const std::string& file_visualization) {
  return RevAlloc(new TreeMonitor(this, vars.data(), vars.size(), file_tree,
                                  file_visualization));
}

SearchMonitor* Solver::MakeTreeMonitor(const std::vector<IntVar*>& vars,
                                       const std::string& file_config,
                                       const std::string& file_tree,
                                       const std::string& file_visualization) {
  return RevAlloc(new TreeMonitor(this, vars.data(), vars.size(), file_config,
                                  file_tree, file_visualization));
}

SearchMonitor* Solver::MakeTreeMonitor(const std::vector<IntVar*>& vars,
                                       std::string* const tree_xml,
                                       std::string* const visualization_xml) {
  return RevAlloc(new TreeMonitor(this, vars.data(), vars.size(), tree_xml,
                                  visualization_xml));
}

SearchMonitor* Solver::MakeTreeMonitor(const std::vector<IntVar*>& vars,
                                       std::string* const config_xml,
                                       std::string* const tree_xml,
                                       std::string* const visualization_xml) {
  return RevAlloc(new TreeMonitor(this, vars.data(), vars.size(), config_xml,
                                  tree_xml, visualization_xml));
}
}  // namespace operations_research
