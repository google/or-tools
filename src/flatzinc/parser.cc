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
  STLDeleteElements(&intvars);
  STLDeleteElements(&boolvars);
  STLDeleteElements(&setvars);
}

AST::Node* ParserState::ArrayElement(string id, unsigned int offset) {
  if (offset > 0) {
    vector<int> tmp;
    if (intvararrays.get(id, tmp) && offset<=tmp.size())
      return new AST::IntVar(tmp[offset-1]);
    if (boolvararrays.get(id, tmp) && offset<=tmp.size())
      return new AST::BoolVar(tmp[offset-1]);
    if (setvararrays.get(id, tmp) && offset<=tmp.size())
      return new AST::SetVar(tmp[offset-1]);

    if (intvalarrays.get(id, tmp) && offset<=tmp.size())
      return new AST::IntLit(tmp[offset-1]);
    if (boolvalarrays.get(id, tmp) && offset<=tmp.size())
      return new AST::BoolLit(tmp[offset-1]);
    vector<AST::SetLit> tmpS;
    if (setvalarrays.get(id, tmpS) && offset<=tmpS.size())
      return new AST::SetLit(tmpS[offset-1]);
  }

  LOG(ERROR) << "Error: array access to " << id << " invalid"
             << " in line no. " << yyget_lineno(yyscanner);
  hadError = true;
  return new AST::IntVar(0); // keep things consistent
}

AST::Node* ParserState::VarRefArg(string id, bool annotation) {
  int tmp;
  if (intvarTable.get(id, tmp))
    return new AST::IntVar(tmp);
  if (boolvarTable.get(id, tmp))
    return new AST::BoolVar(tmp);
  if (setvarTable.get(id, tmp))
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
  if (!dom())
    return;
  AST::Array* args = new AST::Array(2);
  args->a[0] = var;
  args->a[1] = dom.some();
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
    model->Init(intvars.size(),
                boolvars.size(),
                setvars.size());

  int array_index = 0;
  for (unsigned int i=0; i<intvars.size(); i++) {
    if (!hadError) {
      const std::string& raw_name = intvars[i]->Name();
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
      model->NewIntVar(name, intvars[i]);
    }
  }
  array_index = 0;
  for (unsigned int i=0; i<boolvars.size(); i++) {
    if (!hadError) {
      const std::string& raw_name = boolvars[i]->Name();
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
      model->NewBoolVar(name, boolvars[i]);
    }
  }
  for (unsigned int i=0; i<setvars.size(); i++) {
    if (!hadError) {
      //  model->newSetVar(static_cast<SetVarSpec*>(setvars[i]));
    }
  }
  for (unsigned int i = domain_constraints_.size(); i--;) {
    if (!hadError) {
      try {
        assert(domain_constraints_[i]->NumArgs() == 2);
        model->PostConstraint(domain_constraints_[i]);
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
