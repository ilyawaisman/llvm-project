#include "ClangTidyTest.h"
#include "misc/RemoveFlagCheck.h"
#include "gtest/gtest.h"

namespace clang {
namespace tidy {
namespace test {

using misc::RemoveFlagCheck;

TEST(RemoveFlagCheckTest, RemoveDefinition) {
  ClangTidyOptions Options;
  Options.CheckOptions["test-check-0.Flags"] = "FFlag::MyFlag=false,DFFlag::MyDFlag=true,SFFlag::MySFlag=true";

  EXPECT_EQ("#define FASTFLAGVARIABLE(name, value)\n",
            runCheckOnCode<RemoveFlagCheck>(
            "#define FASTFLAGVARIABLE(name, value)\n"
            "FASTFLAGVARIABLE(MyFlag, false)",
                nullptr, "input.cc", None, Options));
  EXPECT_EQ("#define DYNAMIC_FASTFLAGVARIABLE(name, value)\n",
            runCheckOnCode<RemoveFlagCheck>(
            "#define DYNAMIC_FASTFLAGVARIABLE(name, value)\n"
            "DYNAMIC_FASTFLAGVARIABLE(MyDFlag, false)",
                nullptr, "input.cc", None, Options));
  EXPECT_EQ("#define SYNCHRONIZED_FASTFLAGVARIABLE(name, value)\n",
            runCheckOnCode<RemoveFlagCheck>(
            "#define SYNCHRONIZED_FASTFLAGVARIABLE(name, value)\n"
            "SYNCHRONIZED_FASTFLAGVARIABLE(MySFlag, false)",
                nullptr, "input.cc", None, Options));
}

TEST(RemoveFlagCheckTest, RemoveDeclaration) {
  ClangTidyOptions Options;
  Options.CheckOptions["test-check-0.Flags"] = "FFlag::MyFlag=false,DFFlag::MyDFlag=true,SFFlag::MySFlag=true";

  EXPECT_EQ("#define FASTFLAG(name)\n",
            runCheckOnCode<RemoveFlagCheck>(
            "#define FASTFLAG(name)\n"
            "FASTFLAG(MyFlag)",
                nullptr, "input.cc", None, Options));
  EXPECT_EQ("#define DYNAMIC_FASTFLAG(name)\n",
            runCheckOnCode<RemoveFlagCheck>(
            "#define DYNAMIC_FASTFLAG(name)\n"
            "DYNAMIC_FASTFLAG(MyDFlag)",
                nullptr, "input.cc", None, Options));
  EXPECT_EQ("#define SYNCHRONIZED_FASTFLAG(name)\n",
            runCheckOnCode<RemoveFlagCheck>(
            "#define SYNCHRONIZED_FASTFLAG(name)\n"
            "SYNCHRONIZED_FASTFLAG(MySFlag)",
                nullptr, "input.cc", None, Options));
}

TEST(RemoveFlagCheckTest, ReplaceValue) {
  ClangTidyOptions Options;
  Options.CheckOptions["test-check-0.Flags"] = "FFlag::MyFlag=false,DFFlag::MyDFlag=true,SFFlag::MySFlag=true";

  EXPECT_EQ("namespace FFlag { bool MyFlag = true; }\n"
            "void f() { bool b = false; }",
            runCheckOnCode<RemoveFlagCheck>(
            "namespace FFlag { bool MyFlag = true; }\n"
            "void f() { bool b = FFlag::MyFlag; }",
                nullptr, "input.cc", None, Options));
  EXPECT_EQ("namespace DFFlag { bool MyDFlag = false; }\n"
            "void f() { bool b = true; }",
            runCheckOnCode<RemoveFlagCheck>(
            "namespace DFFlag { bool MyDFlag = false; }\n"
            "void f() { bool b = DFFlag::MyDFlag; }",
                nullptr, "input.cc", None, Options));
  EXPECT_EQ("namespace SFFlag { bool MySFlag = true; }\n"
            "void f() { bool b = true; }",
            runCheckOnCode<RemoveFlagCheck>(
            "namespace SFFlag { bool MySFlag = true; }\n"
            "void f() { bool b = SFFlag::MySFlag; }",
                nullptr, "input.cc", None, Options));
}

TEST(RemoveFlagCheckTest, RefInComment) {
  ClangTidyOptions Options;
  Options.CheckOptions["test-check-0.Flags"] = "FFlag::MyFlag=false,DFFlag::MyDFlag=true,SFFlag::MySFlag=true";

  EXPECT_EQ("// Take care when MyFlag <--TODO: This flag is removed, check for required updates--  is removed",
            runCheckOnCode<RemoveFlagCheck>(
                "// Take care when MyFlag is removed",
                nullptr, "input.cc", None, Options));
  EXPECT_EQ("/* Remove together with MyDFlag <--TODO: This flag is removed, check for required updates--  */",
            runCheckOnCode<RemoveFlagCheck>(
                "/* Remove together with MyDFlag */",
                nullptr, "input.cc", None, Options));
}

} // namespace test
} // namespace tidy
} // namespace clang
