#include <sstream>
#include "RemoveFlagCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Lex/MacroArgs.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"

namespace {

bool isComprisedOf(llvm::StringRef Sum) {
  return Sum.empty();
}

template<typename... SRS>
bool isComprisedOf(llvm::StringRef Sum, llvm::StringRef Prefix, SRS... Rest) {
  if (!Sum.startswith(Prefix))
    return false;

  return isComprisedOf(Sum.drop_front(Prefix.size()), Rest...);
}
}

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace misc {

namespace {

StringRef getText(const ASTContext &Context, SourceRange Range) {
  return Lexer::getSourceText(CharSourceRange::getTokenRange(Range),
                              Context.getSourceManager(),
                              Context.getLangOpts());
}

template<typename T>
StringRef getText(const ASTContext &Context, T &Node) {
  return getText(Context, Node.getSourceRange());
}

constexpr llvm::StringRef VariableSuffix = "VARIABLE";
constexpr llvm::StringRef FastFlagInfix = "FASTFLAG";
constexpr llvm::StringRef FastFlagNS = "FFlag::";
} // End of anonymous namespace.

StringRef RemoveFlagCheck::flagKindNSPrefix(FlagKind Kind) {
  switch (Kind) {
    case FlagKind::Dynamic:
      return "D";
    case FlagKind::Static:
      return "";
  }

  return "";
}

StringRef RemoveFlagCheck::flagKindDfnDeclPrefix(FlagKind Kind) {
  switch (Kind) {
    case FlagKind::Dynamic:
      return "DYNAMIC_";
    case FlagKind::Static:
      return "";
  }

  return "";
}

std::string RemoveFlagCheck::FlagDesc::QualVarName() const {
  std::string Result;
  Result.reserve(1 + FastFlagNS.size() + Name.size());
  Result += flagKindNSPrefix(Kind);
  Result += FastFlagNS;
  Result += Name;
  return Result;
}

std::vector <RemoveFlagCheck::FlagDesc> RemoveFlagCheck::deserializeFlags(StringRef Str) {
  SmallVector <StringRef> FlagStrs;
  Str.split(FlagStrs,',');
  std::vector <RemoveFlagCheck::FlagDesc> Result;
  Result.reserve(FlagStrs.size());
  for (const auto FlagStr: FlagStrs) {
    auto Flag = deserializeFlag(FlagStr);
    if (Flag)
      Result.push_back(*Flag);
  }
  return Result;
}

std::string RemoveFlagCheck::serializeFlags(std::vector <FlagDesc> Flags) {
  std::stringstream Result;
  bool First = true;
  for (const auto &Flag: Flags) {
    if (First) {
      Result << ",";
      First = false;
    }
    Result << serializeFlag(Flag);
  }
  return Result.str();
}

std::unique_ptr <RemoveFlagCheck::FlagDesc> RemoveFlagCheck::deserializeFlag(StringRef Str) {
  auto QualNameValue = Str.split('=');
  StringRef QualName = QualNameValue.first;
  StringRef Value = QualNameValue.second;

  if (Value.empty()) {
    llvm::errs() << "Failed to parse flag entry '" << Str << "', no value after '=' found\n";
    return nullptr;
  }

  auto NSName = QualName.split("::");
  StringRef NS = NSName.first;
  StringRef Name = NSName.second;
  if (Name.empty()) {
    llvm::errs() << "Failed to parse flag entry '" << Str << "', no name value after '::' found\n";
    return nullptr;
  }

  FlagKind Kind = FlagKind::Static;
  if (NS.startswith("D")) {
    Kind = FlagKind::Dynamic;
    NS = NS.drop_front(1);
  }

  if (NS != "FFlag") {
    llvm::errs() << "Failed to parse flag entry '" << Str << "', unknown namespace\n";
    return nullptr;
  }

  return std::make_unique<FlagDesc>(Kind, Name, Value);
}

std::string RemoveFlagCheck::serializeFlag(FlagDesc Flag) {
  std::string Result = Flag.QualVarName();
  Result += '=';
  Result += Flag.Value;
  return Result;
}

class RemoveFlagCheck::PPCallback : public PPCallbacks {
public:
    PPCallback(RemoveFlagCheck &Check)
        : Check(Check) {}

    void MacroExpands(const Token &MacroNameTok, const MacroDefinition &MD,
                      SourceRange Range, const MacroArgs *Args) override {
      if (MD.getMacroInfo()->getNumParams() < 1)
        return;

      StringRef MacroName = MacroNameTok.getIdentifierInfo()->getName();
      if (MacroName.endswith(VariableSuffix))
        MacroName = MacroName.drop_back(VariableSuffix.size());

      const Token *FlagVarTok = Args->getUnexpArgument(0);
      IdentifierInfo *IInfo = FlagVarTok->getIdentifierInfo();
      if (!IInfo)
        return;

      StringRef FlagVarName = IInfo->getName();

      for (const auto &Flag: Check.Flags) {
        if (!isComprisedOf(MacroName, flagKindDfnDeclPrefix(Flag.Kind), FastFlagInfix))
          continue;

        if (FlagVarName != Flag.Name)
          continue;

        Check.diag(MacroNameTok.getLocation(), "definition/declaration of a flag to be removed")
            << FixItHint::CreateRemoval(Range);
        return;
      }
    }

private:
    RemoveFlagCheck &Check;
};

class RemoveFlagCheck::CommentHinter : public CommentHandler {
public:
    CommentHinter(RemoveFlagCheck &Check)
        : Check(Check) {}

    bool HandleComment(Preprocessor &PP, SourceRange Range) override {
      StringRef Text =
          Lexer::getSourceText(CharSourceRange::getCharRange(Range),
                               PP.getSourceManager(), PP.getLangOpts());

      for (const auto &Flag: Check.Flags) {
        size_t From = 0;
        do {
          size_t Pos = Text.find(Flag.Name, From);
          if (Pos == StringRef::npos)
            break;

          From = Pos + Flag.Name.size();
          SourceLocation After = Range.getBegin().getLocWithOffset(From);
          Check.diag(Range.getBegin().getLocWithOffset(Pos), "comment on a flag to be removed")
              << FixItHint::CreateInsertion(After, " <--TODO: This flag is removed, check for required updates-- ");
        }
        while (true);
      }
      return false;
    }

private:
    RemoveFlagCheck &Check;
};

class RemoveFlagCheck::Visitor : public RecursiveASTVisitor<Visitor> {
    using Base = RecursiveASTVisitor<Visitor>;

public:
    Visitor(RemoveFlagCheck &Check, ASTContext &Context)
        : Check(Check), Context(Context) {}

    bool traverse() { return TraverseAST(Context); }

    bool VisitDeclRefExpr(DeclRefExpr *D) {
      StringRef Name = getText(Context, *D);
      for (const auto &Flag: Check.Flags) {
        if (!isComprisedOf(Name, flagKindNSPrefix(Flag.Kind), FastFlagNS, Flag.Name))
          continue;

        Check.diag(D->getLocation(), "usage of a flag to be substituted with value")
            << FixItHint::CreateReplacement(D->getSourceRange(), Flag.Value);
        break;
      }
      return true;
    }

private:
    RemoveFlagCheck &Check;
    ASTContext &Context;
};

RemoveFlagCheck::RemoveFlagCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context)
    , Hinter(std::make_unique<CommentHinter>(*this))
    , Flags(deserializeFlags(Options.get("Flags", ""))) {}

RemoveFlagCheck::~RemoveFlagCheck() = default;

void RemoveFlagCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "Flags", serializeFlags(Flags));
}

void RemoveFlagCheck::registerPPCallbacks(
    const SourceManager &SM, Preprocessor *PP, Preprocessor *ModuleExpanderPP) {
  PP->addPPCallbacks(::std::make_unique<PPCallback>(*this));
  PP->addCommentHandler(Hinter.get());
}

void RemoveFlagCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(translationUnitDecl(), this);
}

void RemoveFlagCheck::check(const MatchFinder::MatchResult &Result) {
  Visitor(*this, *Result.Context).traverse();
}

} // namespace misc
} // namespace tidy
} // namespace clang
