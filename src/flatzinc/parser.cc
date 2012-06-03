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
  STLDeleteElements(&domain_constraints_);
  STLDeleteElements(&constraints_);
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

void ParserState::AddConstraints() {
  for (unsigned int i = constraints_.size(); i--;) {
    if (!hadError) {
      model_->PostConstraint(constraints_[i]);
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

void ParserState::AddDomainConstraint(std::string id,
                                      AST::Node* var,
                                      Option<AST::SetLit* >& dom) {
  if (!dom.defined())
    return;
  AST::Array* args = new AST::Array(2);
  args->a[0] = var;
  args->a[1] = dom.value();
  domain_constraints_.push_back(new CtSpec(-1, id, args, NULL));
}

void ParserState::AddConstraint(const std::string& id,
                                AST::Array* const args,
                                AST::Node* const annotations) {
  constraints_.push_back(new CtSpec(constraints_.size(),
                                    id,
                                    args,
                                    annotations));
}

void ParserState::InitModel() {
  if (!hadError)
    model_->Init(int_variables_.size(),
                 bool_variables_.size(),
                 set_variables_.size());

  int array_index = 0;
  for (unsigned int i = 0; i < int_variables_.size(); i++) {
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
      model_->NewIntVar(name, int_variables_[i]);
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
      model_->NewBoolVar(name, bool_variables_[i]);
    }
  }
  for (unsigned int i=0; i<set_variables_.size(); i++) {
    if (!hadError) {
      //  model->newSetVar(static_cast<Set_Variables_pec*>(set_variables_[i]));
    }
  }
  for (unsigned int i = domain_constraints_.size(); i--;) {
    if (!hadError) {
      try {
        assert(domain_constraints_[i]->NumArgs() == 2);
        model_->PostConstraint(domain_constraints_[i]);
      } catch (operations_research::Error& e) {
        yyerror(this, e.DebugString().c_str());
      }
    }
  }
}

void ParserState::FillOutput(operations_research::FlatZincModel& m) {
  m.InitOutput(Output());
}

void FlatZincModel::Parse(const std::string& filename) {
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
  }

void FlatZincModel::Parse(std::istream& is) {
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
