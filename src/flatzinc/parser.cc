#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "flatzinc/flatzinc.h"
#include "flatzinc/parser.h"

using namespace std;
extern int yyparse(void*);
extern int yylex(YYSTYPE*, void* scanner);
extern int yylex_init (void** scanner);
extern int yylex_destroy (void* scanner);
extern int yyget_lineno (void* scanner);
extern void yyset_extra (void* user_defined ,void* yyscanner );
extern void yyerror(void* parm, const char *str);

namespace operations_research {
ParserState::~ParserState() {
  STLDeleteElements(&int_variables_);
  STLDeleteElements(&bool_variables_);
  STLDeleteElements(&set_variables_);
  STLDeleteElements(&constraints_);
  for (int i = 0; i < domain_constraints_.size(); ++i) {
    delete domain_constraints_[i].second;
  }
}

int ParserState::FillBuffer(char* lexBuf, unsigned int lexBufSize) {
  if (pos >= length)
    return 0;
  int num = std::min(length - pos, lexBufSize);
  memcpy(lexBuf, buf + pos, num);
  pos += num;
  return num;
}

void ParserState::output(std::string x, AST::Node* n) {
  output_.push_back(std::pair<std::string,AST::Node*>(x,n));
}

AST::Array* ParserState::Output(void) {
  OutputOrder oo;
  std::sort(output_.begin(),output_.end(),oo);
  AST::Array* const a = new AST::Array();
  for (unsigned int i = 0; i < output_.size(); i++) {
    a->a.push_back(new AST::String(output_[i].first+" = "));
    if (output_[i].second->isArray()) {
      AST::Array* const oa = output_[i].second->getArray();
      for (unsigned int j = 0; j < oa->a.size(); j++) {
        a->a.push_back(oa->a[j]);
        oa->a[j] = NULL;
      }
      delete output_[i].second;
    } else {
      a->a.push_back(output_[i].second);
    }
    a->a.push_back(new AST::String(";\n"));
  }
  return a;
}

int ParserState::FindTarget(AST::Node* const annotations) const {
  if (annotations != NULL) {
    if (annotations->isArray()) {
      AST::Array* const ann_array = annotations->getArray();
      if (ann_array->a[0]->isCall("defines_var")) {
        AST::Call* const call = ann_array->a[0]->getCall();
        AST::Node* const args = call->args;
        if (args->isIntVar()) {
          return args->getIntVar();
        } else if (args->isBoolVar()) {
          return int_variables_.size() + args->getBoolVar();
        }
      }
    }
  }
  return CtSpec::kNoDefinition;
}

void ParserState::CollectRequired(AST::Array* const args,
                                  const hash_set<int>& candidates,
                                  hash_set<int>* const require) const {
  for (int i = 0; i < args->a.size(); ++i) {
    AST::Node* const node = args->a[i];
    if (node->isArray()) {
      CollectRequired(node->getArray(), candidates, require);
    } else if (node->isIntVar()) {
      const int req = node->getIntVar();
      if (ContainsKey(candidates, req)) {
        require->insert(req);
      }
    } else if (node->isBoolVar()) {
      const int req = node->getBoolVar() + int_variables_.size();
      if (ContainsKey(candidates, req)) {
        require->insert(req);
      }
    }
  }
}

void ParserState::ComputeViableTarget(
    CtSpec* const spec,
    hash_set<int>* const candidates) const {
  const string& id = spec->Id();
  if (id == "bool2int" ||
      id == "int_plus" ||
      id == "int_minus" ||
      id == "int_times" ||
      id == "array_var_int_element" ||
      id == "array_int_element" ||
      id == "array_bool_element") {
    // Defines an int var.
    const int define = FindTarget(spec->annotations());
    if (define != CtSpec::kNoDefinition) {
      CHECK(int_variables_[define]->introduced);
      candidates->insert(define);
      VLOG(1) << id << " -> insert " << define;
    }
  } else if (id == "array_bool_and" || id == "array_bool_or") {
    // Defines a bool var.
    const int bool_define = FindTarget(spec->annotations());
    if (bool_define != CtSpec::kNoDefinition) {
      CHECK(bool_variables_[bool_define - int_variables_.size()]->introduced);
      candidates->insert(bool_define);
      VLOG(1) << id << " -> insert " << bool_define;
    }
  } else if (id == "int2int") {
    candidates->insert(spec->Arg(1)->getIntVar());
    VLOG(1) << id << " -> insert " << spec->Arg(1)->getIntVar();
  } else if (id == "bool2bool") {
    const int bool_define = spec->Arg(1)->getBoolVar() + int_variables_.size();
    candidates->insert(bool_define);
    VLOG(1) << id << " -> insert " << bool_define;
  }
}

bool DoDefine(const CtSpec* const spec) {
  return spec->defines() != CtSpec::kNoDefinition;
}

bool ConstraintDependsOn(const CtSpec* const spec1,
                         const CtSpec* const spec2) {
  const int def1 = spec1->defines();
  const int def2 = spec2->defines();
  if (spec2->Require(def1)) {
    return true;
  }
  if (spec1->Require(def2)) {
    return false;
  }
  return spec1->Index() < spec2->Index();
}

void ParserState::ComputeDependencies(const hash_set<int>& candidates,
                                      CtSpec* const spec) const {
  const int define = spec->defines() == CtSpec::kNoDefinition ?
      FindTarget(spec->annotations()) :
      spec->defines();
  if (ContainsKey(candidates, define)) {
    spec->set_defines(define);
  }
  CollectRequired(spec->Args(), candidates, spec->require_map());
  if (define != CtSpec::kNoDefinition) {
    spec->require_map()->erase(define);
  }
}

void ParserState::CreateModel() {
  hash_set<int> candidates;
  // Add aliasing constraints.
  for (int i = 0; i < int_variables_.size(); ++i) {
    IntVarSpec* const spec = int_variables_[i];
    if (spec->alias) {
      AST::Array* args = new AST::Array(2);
      args->a[0] = new AST::IntVar(spec->i);
      args->a[1] = new AST::IntVar(i);
      CtSpec* const alias_ct = new CtSpec(constraints_.size(),
                                          "int2int",
                                          args,
                                          NULL);
      alias_ct->set_defines(i);
      constraints_.push_back(alias_ct);
    }
  }

  for (int i = 0; i < bool_variables_.size(); ++i) {
    BoolVarSpec* const spec = bool_variables_[i];
    if (spec->alias) {
      AST::Array* args = new AST::Array(2);
      args->a[0] = new AST::BoolVar(spec->i);
      args->a[1] = new AST::BoolVar(i);
      CtSpec* const alias_ct = new CtSpec(constraints_.size(),
                                          "bool2bool",
                                          args,
                                          NULL);
      alias_ct->set_defines(i + int_variables_.size());
      constraints_.push_back(alias_ct);
    }
  }

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    ComputeViableTarget(spec, &candidates);
  }

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    ComputeDependencies(candidates, spec);
    if (spec->defines() != CtSpec::kNoDefinition ||
        !spec->require_map()->empty()) {
      VLOG(1) << spec->DebugString();
    }
  }

  VLOG(1) << "Sort constraints";
  std::vector<CtSpec*> defines_only;
  std::vector<CtSpec*> no_defines;
  std::vector<CtSpec*> defines_and_require;
  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    if (DoDefine(spec) && spec->require_map()->empty()) {
      defines_only.push_back(spec);
    } else if (!DoDefine(spec)) {
      no_defines.push_back(spec);
    } else {
      defines_and_require.push_back(spec);
    }
  }

  VLOG(1) << "defines only        : " << defines_only.size();
  VLOG(1) << "no_defines          : " << no_defines.size();
  VLOG(1) << "defines_and_require : " << defines_and_require.size();

  const int size = constraints_.size();
  constraints_.clear();
  constraints_.resize(size);
  int index = 0;
  hash_set<int> defined;
  for (int i = 0; i < defines_only.size(); ++i) {
    constraints_[index++] = defines_only[i];
    defined.insert(defines_only[i]->defines());
    VLOG(1) << "defined.insert(" << defines_only[i]->defines() << ")";
  }

  // Topological sorting.
  hash_set<int> to_insert;
  for (int i = 0; i < defines_and_require.size(); ++i) {
    to_insert.insert(i);
    VLOG(1) << " to_insert " << defines_and_require[i]->DebugString();
  }

  while (!to_insert.empty()) {
    std::vector<int> inserted;
    for (ConstIter<hash_set<int> > it(to_insert); !it.at_end(); ++it) {
      CtSpec* const spec = defines_and_require[*it];
      VLOG(1) << "check " << spec->DebugString();
      bool ok = true;
      hash_set<int>* const required = spec->require_map();
      for (ConstIter<hash_set<int> > def(*required);
           !def.at_end();
           ++def) {
        if (!ContainsKey(defined, *def)) {
          ok = false;
          break;
        }
      }
      if (ok) {
        inserted.push_back(*it);
        defined.insert(spec->defines());
        VLOG(1) << "inserted.push_back " << *it;
        VLOG(1) << "defined.insert(" << spec->defines() << ")";
        constraints_[index++] = spec;
      }
    }
    CHECK(!inserted.empty());
    for (int i = 0; i < inserted.size(); ++i) {
      to_insert.erase(inserted[i]);
    }
  }

  // Push the rest.
  for (int i = 0; i < no_defines.size(); ++i) {
    constraints_[index++] = no_defines[i];
  }

  VLOG(1) << "Sorting finished";

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    VLOG(1) << i << " -> " << spec->DebugString();
  }

  int array_index = 0;
  for (unsigned int i = 0; i < int_variables_.size(); i++) {
    VLOG(1) << "Var spec " << i << int_variables_[i]->DebugString();
    if (!hadError) {
      const std::string& raw_name = int_variables_[i]->Name();
      std::string name;
      if (raw_name[0] == '[') {
        name = StringPrintf("%s[%d]", raw_name.c_str() + 1, ++array_index);
      } else {
        if (array_index == 0) {
          name = raw_name;
        } else {
          name = StringPrintf("%s[%d]", raw_name.c_str(), array_index + 1);
          array_index = 0;
        }
      }
      if (!ContainsKey(candidates, i)) {
        model_->NewIntVar(name, int_variables_[i]);
      } else {
        model_->SkipIntVar();
        VLOG(1) << "Skipping " << int_variables_[i]->DebugString();
      }
    }
  }

  array_index = 0;
  for (unsigned int i=0; i<bool_variables_.size(); i++) {
    if (!hadError) {
      const std::string& raw_name = bool_variables_[i]->Name();
      std::string name;
      if (raw_name[0] == '[') {
        name = StringPrintf("%s[%d]", raw_name.c_str() + 1, ++array_index);
      } else {
        if (array_index == 0) {
          name = raw_name;
        } else {
          name = StringPrintf("%s[%d]", raw_name.c_str(), array_index + 1);
          array_index = 0;
        }
      }
      if (!ContainsKey(candidates, i + int_variables_.size())) {
        model_->NewBoolVar(name, bool_variables_[i]);
      } else {
        model_->SkipBoolVar();
        VLOG(1) << "Skipping " << bool_variables_[i]->DebugString();
      }
    }
  }

  for (unsigned int i=0; i<set_variables_.size(); i++) {
    if (!hadError) {
      //  model->newSetVar(static_cast<Set_Variables_pec*>(set_variables_[i]));
    }
  }
  for (unsigned int i = 0; i < constraints_.size(); i++) {
    if (!hadError) {
      CtSpec* const spec = constraints_[i];
      model_->PostConstraint(constraints_[i]);
    }
  }
  for (unsigned int i = domain_constraints_.size(); i--;) {
    if (!hadError) {
      const int var_id = domain_constraints_[i].first;
      AST::SetLit* const dom = domain_constraints_[i].second;
      VLOG(1) << "Reduce integer variable " << var_id
              << " to " << dom->DebugString();
      if (dom->interval) {
        model_->IntegerVariable(var_id)->SetRange(dom->min, dom->max);
      } else {
        model_->IntegerVariable(var_id)->SetValues(dom->s);
      }
    }
  }
}

AST::Node* ParserState::ArrayElement(string id, unsigned int offset) {
  if (offset > 0) {
    vector<int> tmp;
    if (int_var_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AST::IntVar(tmp[offset-1]);
    if (bool_var_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AST::BoolVar(tmp[offset-1]);
    if (set_var_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AST::SetVar(tmp[offset-1]);

    if (int_value_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AST::IntLit(tmp[offset-1]);
    if (bool_value_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AST::BoolLit(tmp[offset-1]);
    vector<AST::SetLit> tmpS;
    if (set_value_array_map_.get(id, tmpS) && offset<=tmpS.size())
      return new AST::SetLit(tmpS[offset-1]);
  }

  LOG(ERROR) << "Error: array access to " << id << " invalid"
             << " in line no. " << yyget_lineno(yyscanner);
  hadError = true;
  return new AST::IntVar(0); // keep things consistent
}

AST::Node* ParserState::VarRefArg(string id, bool annotation) {
  int tmp;
  if (int_var_map_.get(id, tmp))
    return new AST::IntVar(tmp);
  if (bool_var_map_.get(id, tmp))
    return new AST::BoolVar(tmp);
  if (set_var_map_.get(id, tmp))
    return new AST::SetVar(tmp);
  if (annotation)
    return new AST::Atom(id);
  LOG(ERROR) << "Error: undefined variable " << id
             << " in line no. " << yyget_lineno(yyscanner);
  hadError = true;
  return new AST::IntVar(0); // keep things consistent
}

void ParserState::AddIntVarDomainConstraint(int var_id,
                                            AST::SetLit* const dom) {
  domain_constraints_.push_back(std::make_pair(var_id, dom));
}

void ParserState::AddConstraint(const std::string& id,
                                AST::Array* const args,
                                AST::Node* const annotations) {
  int target = CtSpec::kNoDefinition;
  constraints_.push_back(
      new CtSpec(constraints_.size(), id, args, annotations));
}

void ParserState::InitModel() {
  if (!hadError)
    model_->Init(int_variables_.size(),
                 bool_variables_.size(),
                 set_variables_.size());
}

void ParserState::FillOutput(operations_research::FlatZincModel& m) {
  m.InitOutput(Output());
}

bool FlatZincModel::Parse(const std::string& filename) {
#ifdef HAVE_MMAP
  int fd;
  char* data;
  struct stat sbuf;
  fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) {
    LOG(ERROR) << "Cannot open file " << filename;
    return NULL;
  }
  if (stat(filename.c_str(), &sbuf) == -1) {
    LOG(ERROR) << "Cannot stat file " << filename;
    return NULL;
  }
  data = (char*)mmap((caddr_t)0, sbuf.st_size, PROT_READ, MAP_SHARED, fd,0);
  if (data == (caddr_t)(-1)) {
    LOG(ERROR) << "Cannot mmap file " << filename;
    return NULL;
  }

  ParserState pp(data, sbuf.st_size, this);
#else
  std::ifstream file;
  file.open(filename.c_str());
  if (!file.is_open()) {
    LOG(FATAL) << "Cannot open file " << filename;
  }
  std::string s = string(istreambuf_iterator<char>(file),
                         istreambuf_iterator<char>());
  ParserState pp(s, this);
#endif
  yylex_init(&pp.yyscanner);
  yyset_extra(&pp, pp.yyscanner);
  // yydebug = 1;
  yyparse(&pp);
  pp.FillOutput(*this);

  if (pp.yyscanner)
    yylex_destroy(pp.yyscanner);
  parsed_ok_ = !pp.hadError;
}

bool FlatZincModel::Parse(std::istream& is) {
  std::string s = string(istreambuf_iterator<char>(is),
                         istreambuf_iterator<char>());

  ParserState pp(s, this);
  yylex_init(&pp.yyscanner);
  yyset_extra(&pp, pp.yyscanner);
  // yydebug = 1;
  yyparse(&pp);
  pp.FillOutput(*this);

  if (pp.yyscanner)
    yylex_destroy(pp.yyscanner);
  parsed_ok_ = !pp.hadError;
}

AST::Node* ArrayOutput(AST::Call* ann) {
  AST::Array* a = NULL;

  if (ann->args->isArray()) {
    a = ann->args->getArray();
  } else {
    a = new AST::Array(ann->args);
  }

  std::string out;

  out = StringPrintf("array%dd(", a->a.size());;
  for (unsigned int i = 0; i < a->a.size(); i++) {
    AST::SetLit* s = a->a[i]->getSet();
    if (s->empty()) {
      out += "{}, ";
    } else if (s->interval) {
      out += StringPrintf("%d..%d, ", s->min, s->max);
    } else {
      out += "{";
      for (unsigned int j = 0; j < s->s.size(); j++) {
        out += s->s[j];
        if (j < s->s.size() - 1) {
          out += ",";
        }
      }
      out += "}, ";
    }
  }

  if (!ann->args->isArray()) {
    a->a[0] = NULL;
    delete a;
  }
  return new AST::String(out);
}
}  // namespace operations_research
