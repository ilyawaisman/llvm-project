#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_REMOVEFLAGCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_REMOVEFLAGCHECK_H

#include "../ClangTidyCheck.h"

namespace clang {
namespace tidy {
namespace misc {

/// Removes specific fast flags from codebase:
/// * removes definitions and declarations of flags,
/// * substitutes usages with values provided,
/// * adds a T_O_D_O item when flag is mentioned in comments.
class RemoveFlagCheck : public ClangTidyCheck {
public:
    RemoveFlagCheck(StringRef Name, ClangTidyContext *Context);
    ~RemoveFlagCheck();

    void storeOptions(ClangTidyOptions::OptionMap &Options) override;
    void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                             Preprocessor *ModuleExpanderPP) override;
    void registerMatchers(ast_matchers::MatchFinder *Finder) override;
    void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
    enum class FlagKind {
        Dynamic, Static
    };
    static StringRef flagKindDfnDeclPrefix(FlagKind Kind);
    static StringRef flagKindNSPrefix(FlagKind Kind);

    class FlagDesc {
    public:
        FlagDesc(FlagKind Kind, StringRef Name, StringRef Value)
            : Kind(Kind), Name(Name.str()), Value(Value.str()) {}

        std::string QualVarName() const;

        FlagKind Kind;
        std::string Name;
        std::string Value;
    };

    static std::vector<FlagDesc> deserializeFlags(StringRef Str);
    static std::string serializeFlags(std::vector<FlagDesc> Flags);

    static std::unique_ptr<FlagDesc> deserializeFlag(StringRef Str);
    static std::string serializeFlag(FlagDesc Flag);

    class PPCallback;
    class CommentHinter;
    class Visitor;

    std::unique_ptr<CommentHinter> Hinter;
    std::vector<FlagDesc> Flags;
};

} // namespace misc
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_REMOVEFLAGCHECK_H
