//===-- include/flang/Parser/parse-tree.h -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef FORTRAN_PARSER_PARSE_TREE_H_
#define FORTRAN_PARSER_PARSE_TREE_H_

// Defines the classes used to represent successful reductions of productions
// in the Fortran grammar.  The names and content of these definitions
// adhere closely to the syntax specifications in the language standard (q.v.)
// that are transcribed here and referenced via their requirement numbers.
// The representations of some productions that may also be of use in the
// run-time I/O support library have been isolated into a distinct header file
// (viz., format-specification.h).

#include "char-block.h"
#include "characters.h"
#include "format-specification.h"
#include "message.h"
#include "provenance.h"
#include "flang/Common/idioms.h"
#include "flang/Common/indirection.h"
#include "flang/Support/Fortran.h"
#include "llvm/Frontend/OpenACC/ACC.h.inc"
#include "llvm/Frontend/OpenMP/OMP.h"
#include "llvm/Frontend/OpenMP/OMPConstants.h"
#include <cinttypes>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

// Parse tree node class types do not have default constructors.  They
// explicitly declare "T() {} = delete;" to make this clear.  This restriction
// prevents the introduction of what would be a viral requirement to include
// std::monostate among most std::variant<> discriminated union members.

// Parse tree node class types do not have copy constructors or copy assignment
// operators.  They are explicitly declared "= delete;" to make this clear,
// although a C++ compiler wouldn't default them anyway due to the presence
// of explicitly defaulted move constructors and move assignments.

CLASS_TRAIT(EmptyTrait)
CLASS_TRAIT(WrapperTrait)
CLASS_TRAIT(UnionTrait)
CLASS_TRAIT(TupleTrait)
CLASS_TRAIT(ConstraintTrait)

// Some parse tree nodes have fields in them to cache the results of a
// successful semantic analysis later.  Their types are forward declared
// here.
namespace Fortran::semantics {
class Symbol;
class DeclTypeSpec;
class DerivedTypeSpec;
} // namespace Fortran::semantics

// Expressions in the parse tree have owning pointers that can be set to
// type-checked generic expression representations by semantic analysis.
namespace Fortran::evaluate {
struct GenericExprWrapper; // forward definition, wraps Expr<SomeType>
struct GenericAssignmentWrapper; // forward definition, represent assignment
class ProcedureRef; // forward definition, represents a CALL or function ref
} // namespace Fortran::evaluate

// Most non-template classes in this file use these default definitions
// for their move constructor and move assignment operator=, and disable
// their copy constructor and copy assignment operator=.
#define COPY_AND_ASSIGN_BOILERPLATE(classname) \
  classname(classname &&) = default; \
  classname &operator=(classname &&) = default; \
  classname(const classname &) = delete; \
  classname &operator=(const classname &) = delete

// Almost all classes in this file have no default constructor.
#define BOILERPLATE(classname) \
  COPY_AND_ASSIGN_BOILERPLATE(classname); \
  classname() = delete

// Empty classes are often used below as alternatives in std::variant<>
// discriminated unions.
#define EMPTY_CLASS(classname) \
  struct classname { \
    classname() {} \
    classname(const classname &) {} \
    classname(classname &&) {} \
    classname &operator=(const classname &) { return *this; }; \
    classname &operator=(classname &&) { return *this; }; \
    using EmptyTrait = std::true_type; \
  }

// Many classes below simply wrap a std::variant<> discriminated union,
// which is conventionally named "u".
#define UNION_CLASS_BOILERPLATE(classname) \
  template <typename A, typename = common::NoLvalue<A>> \
  classname(A &&x) : u(std::move(x)) {} \
  using UnionTrait = std::true_type; \
  BOILERPLATE(classname)

// Many other classes below simply wrap a std::tuple<> structure, which
// is conventionally named "t".
#define TUPLE_CLASS_BOILERPLATE(classname) \
  template <typename... Ts, typename = common::NoLvalue<Ts...>> \
  classname(Ts &&...args) : t(std::move(args)...) {} \
  using TupleTrait = std::true_type; \
  BOILERPLATE(classname)

// Many other classes below simply wrap a single data member, which is
// conventionally named "v".
#define WRAPPER_CLASS_BOILERPLATE(classname, type) \
  BOILERPLATE(classname); \
  classname(type &&x) : v(std::move(x)) {} \
  using WrapperTrait = std::true_type; \
  type v

#define WRAPPER_CLASS(classname, type) \
  struct classname { \
    WRAPPER_CLASS_BOILERPLATE(classname, type); \
  }

namespace Fortran::parser {

// These are the unavoidable recursively-defined productions of Fortran.
// Some references to the representations of their parses require
// indirection.  The Indirect<> pointer wrapper class is used to
// enforce ownership semantics and non-nullability.
struct SpecificationPart; // R504
struct ExecutableConstruct; // R514
struct ActionStmt; // R515
struct AcImpliedDo; // R774
struct DataImpliedDo; // R840
struct Designator; // R901
struct Variable; // R902
struct Expr; // R1001
struct WhereConstruct; // R1042
struct ForallConstruct; // R1050
struct InputImpliedDo; // R1218
struct OutputImpliedDo; // R1218
struct FunctionReference; // R1520
struct FunctionSubprogram; // R1529
struct SubroutineSubprogram; // R1534

// These additional forward references are declared so that the order of
// class definitions in this header file can remain reasonably consistent
// with order of the the requirement productions in the grammar.
struct DerivedTypeDef; // R726
struct EnumDef; // R759
struct TypeDeclarationStmt; // R801
struct AccessStmt; // R827
struct AllocatableStmt; // R829
struct AsynchronousStmt; // R831
struct BindStmt; // R832
struct CodimensionStmt; // R834
struct ContiguousStmt; // R836
struct DataStmt; // R837
struct DataStmtValue; // R843
struct DimensionStmt; // R848
struct IntentStmt; // R849
struct OptionalStmt; // R850
struct ParameterStmt; // R851
struct OldParameterStmt;
struct PointerStmt; // R853
struct ProtectedStmt; // R855
struct SaveStmt; // R856
struct TargetStmt; // R859
struct ValueStmt; // R861
struct VolatileStmt; // R862
struct ImplicitStmt; // R863
struct ImportStmt; // R867
struct NamelistStmt; // R868
struct EquivalenceStmt; // R870
struct CommonStmt; // R873
struct Substring; // R908
struct CharLiteralConstantSubstring;
struct SubstringInquiry;
struct DataRef; // R911
struct StructureComponent; // R913
struct CoindexedNamedObject; // R914
struct ArrayElement; // R917
struct AllocateStmt; // R927
struct NullifyStmt; // R939
struct DeallocateStmt; // R941
struct AssignmentStmt; // R1032
struct PointerAssignmentStmt; // R1033
struct WhereStmt; // R1041, R1045, R1046
struct ForallStmt; // R1055
struct AssociateConstruct; // R1102
struct BlockConstruct; // R1107
struct ChangeTeamConstruct; // R1111
struct CriticalConstruct; // R1116
struct DoConstruct; // R1119
struct LabelDoStmt; // R1121
struct ConcurrentHeader; // R1125
struct EndDoStmt; // R1132
struct CycleStmt; // R1133
struct IfConstruct; // R1134
struct IfStmt; // R1139
struct CaseConstruct; // R1140
struct SelectRankConstruct; // R1148
struct SelectTypeConstruct; // R1152
struct ExitStmt; // R1156
struct GotoStmt; // R1157
struct ComputedGotoStmt; // R1158
struct StopStmt; // R1160, R1161
struct NotifyWaitStmt; // F2023: R1166
struct SyncAllStmt; // R1164
struct SyncImagesStmt; // R1166
struct SyncMemoryStmt; // R1168
struct SyncTeamStmt; // R1169
struct EventPostStmt; // R1170, R1171
struct EventWaitSpec; // F2023: R1177
struct EventWaitStmt; // R1172, R1173, R1174
struct FormTeamStmt; // R1175, R1176, R1177
struct LockStmt; // R1178
struct UnlockStmt; // R1180
struct OpenStmt; // R1204
struct CloseStmt; // R1208
struct ReadStmt; // R1210
struct WriteStmt; // R1211
struct PrintStmt; // R1212
struct WaitStmt; // R1222
struct BackspaceStmt; // R1224
struct EndfileStmt; // R1225
struct RewindStmt; // R1226
struct FlushStmt; // R1228
struct InquireStmt; // R1230
struct FormatStmt; // R1301
struct MainProgram; // R1401
struct Module; // R1404
struct UseStmt; // R1409
struct Submodule; // R1416
struct BlockData; // R1420
struct InterfaceBlock; // R1501
struct GenericSpec; // R1508
struct GenericStmt; // R1510
struct ExternalStmt; // R1511
struct ProcedureDeclarationStmt; // R1512
struct IntrinsicStmt; // R1519
struct Call; // R1520 & R1521
struct CallStmt; // R1521
struct ProcedureDesignator; // R1522
struct ActualArg; // R1524
struct SeparateModuleSubprogram; // R1538
struct EntryStmt; // R1541
struct ReturnStmt; // R1542
struct StmtFunctionStmt; // R1544

// Directives, extensions, and deprecated statements
struct CompilerDirective;
struct BasedPointerStmt;
struct CUDAAttributesStmt;
struct StructureDef;
struct ArithmeticIfStmt;
struct AssignStmt;
struct AssignedGotoStmt;
struct PauseStmt;
struct OpenACCConstruct;
struct AccEndCombinedDirective;
struct OpenACCDeclarativeConstruct;
struct OpenACCRoutineConstruct;
struct OpenMPConstruct;
struct OpenMPLoopConstruct;
struct OpenMPDeclarativeConstruct;
struct OmpEndLoopDirective;
struct OmpMemoryOrderClause;
struct CUFKernelDoConstruct;

// Cooked character stream locations
using Location = const char *;

// A parse tree node with provenance only
struct Verbatim {
  // Allow a no-arg constructor for Verbatim so parsers can return `RESULT{}`.
  constexpr Verbatim() {}
  COPY_AND_ASSIGN_BOILERPLATE(Verbatim);
  using EmptyTrait = std::true_type;
  CharBlock source;
};

// Implicit definitions of the Standard

// R403 scalar-xyz -> xyz
// These template class wrappers correspond to the Standard's modifiers
// scalar-xyz, constant-xzy, int-xzy, default-char-xyz, & logical-xyz.
template <typename A> struct Scalar {
  using ConstraintTrait = std::true_type;
  Scalar(Scalar &&that) = default;
  Scalar(A &&that) : thing(std::move(that)) {}
  Scalar &operator=(Scalar &&) = default;
  A thing;
};

template <typename A> struct Constant {
  using ConstraintTrait = std::true_type;
  Constant(Constant &&that) = default;
  Constant(A &&that) : thing(std::move(that)) {}
  Constant &operator=(Constant &&) = default;
  A thing;
};

template <typename A> struct Integer {
  using ConstraintTrait = std::true_type;
  Integer(Integer &&that) = default;
  Integer(A &&that) : thing(std::move(that)) {}
  Integer &operator=(Integer &&) = default;
  A thing;
};

template <typename A> struct Logical {
  using ConstraintTrait = std::true_type;
  Logical(Logical &&that) = default;
  Logical(A &&that) : thing(std::move(that)) {}
  Logical &operator=(Logical &&) = default;
  A thing;
};

template <typename A> struct DefaultChar {
  using ConstraintTrait = std::true_type;
  DefaultChar(DefaultChar &&that) = default;
  DefaultChar(A &&that) : thing(std::move(that)) {}
  DefaultChar &operator=(DefaultChar &&) = default;
  A thing;
};

using LogicalExpr = Logical<common::Indirection<Expr>>; // R1024
using DefaultCharExpr = DefaultChar<common::Indirection<Expr>>; // R1025
using IntExpr = Integer<common::Indirection<Expr>>; // R1026
using ConstantExpr = Constant<common::Indirection<Expr>>; // R1029
using IntConstantExpr = Integer<ConstantExpr>; // R1031
using ScalarLogicalExpr = Scalar<LogicalExpr>;
using ScalarIntExpr = Scalar<IntExpr>;
using ScalarIntConstantExpr = Scalar<IntConstantExpr>;
using ScalarDefaultCharExpr = Scalar<DefaultCharExpr>;
// R1030 default-char-constant-expr is used in the Standard only as part of
// scalar-default-char-constant-expr.
using ScalarDefaultCharConstantExpr = Scalar<DefaultChar<ConstantExpr>>;

// R611 label -> digit [digit]...
using Label = common::Label; // validated later, must be in [1..99999]

// A wrapper for xzy-stmt productions that are statements, so that
// source provenances and labels have a uniform representation.
template <typename A> struct UnlabeledStatement {
  explicit UnlabeledStatement(A &&s) : statement(std::move(s)) {}
  CharBlock source;
  A statement;
};
template <typename A> struct Statement : public UnlabeledStatement<A> {
  Statement(std::optional<long> &&lab, A &&s)
      : UnlabeledStatement<A>{std::move(s)}, label(std::move(lab)) {}
  std::optional<Label> label;
};

// Error recovery marker
EMPTY_CLASS(ErrorRecovery);

// R513 other-specification-stmt ->
//        access-stmt | allocatable-stmt | asynchronous-stmt | bind-stmt |
//        codimension-stmt | contiguous-stmt | dimension-stmt | external-stmt |
//        intent-stmt | intrinsic-stmt | namelist-stmt | optional-stmt |
//        pointer-stmt | protected-stmt | save-stmt | target-stmt |
//        volatile-stmt | value-stmt | common-stmt | equivalence-stmt
// Extension: (Cray) based POINTER statement
// Extension: CUDA data attribute statement
struct OtherSpecificationStmt {
  UNION_CLASS_BOILERPLATE(OtherSpecificationStmt);
  std::variant<common::Indirection<AccessStmt>,
      common::Indirection<AllocatableStmt>,
      common::Indirection<AsynchronousStmt>, common::Indirection<BindStmt>,
      common::Indirection<CodimensionStmt>, common::Indirection<ContiguousStmt>,
      common::Indirection<DimensionStmt>, common::Indirection<ExternalStmt>,
      common::Indirection<IntentStmt>, common::Indirection<IntrinsicStmt>,
      common::Indirection<NamelistStmt>, common::Indirection<OptionalStmt>,
      common::Indirection<PointerStmt>, common::Indirection<ProtectedStmt>,
      common::Indirection<SaveStmt>, common::Indirection<TargetStmt>,
      common::Indirection<ValueStmt>, common::Indirection<VolatileStmt>,
      common::Indirection<CommonStmt>, common::Indirection<EquivalenceStmt>,
      common::Indirection<BasedPointerStmt>,
      common::Indirection<CUDAAttributesStmt>>
      u;
};

// R508 specification-construct ->
//        derived-type-def | enum-def | generic-stmt | interface-block |
//        parameter-stmt | procedure-declaration-stmt |
//        other-specification-stmt | type-declaration-stmt
struct SpecificationConstruct {
  UNION_CLASS_BOILERPLATE(SpecificationConstruct);
  std::variant<common::Indirection<DerivedTypeDef>,
      common::Indirection<EnumDef>, Statement<common::Indirection<GenericStmt>>,
      common::Indirection<InterfaceBlock>,
      Statement<common::Indirection<ParameterStmt>>,
      Statement<common::Indirection<OldParameterStmt>>,
      Statement<common::Indirection<ProcedureDeclarationStmt>>,
      Statement<OtherSpecificationStmt>,
      Statement<common::Indirection<TypeDeclarationStmt>>,
      common::Indirection<StructureDef>,
      common::Indirection<OpenACCDeclarativeConstruct>,
      common::Indirection<OpenMPDeclarativeConstruct>,
      common::Indirection<CompilerDirective>>
      u;
};

// R506 implicit-part-stmt ->
//         implicit-stmt | parameter-stmt | format-stmt | entry-stmt
struct ImplicitPartStmt {
  UNION_CLASS_BOILERPLATE(ImplicitPartStmt);
  std::variant<Statement<common::Indirection<ImplicitStmt>>,
      Statement<common::Indirection<ParameterStmt>>,
      Statement<common::Indirection<OldParameterStmt>>,
      Statement<common::Indirection<FormatStmt>>,
      Statement<common::Indirection<EntryStmt>>,
      common::Indirection<CompilerDirective>,
      common::Indirection<OpenACCDeclarativeConstruct>>
      u;
};

// R505 implicit-part -> [implicit-part-stmt]... implicit-stmt
WRAPPER_CLASS(ImplicitPart, std::list<ImplicitPartStmt>);

// R507 declaration-construct ->
//        specification-construct | data-stmt | format-stmt |
//        entry-stmt | stmt-function-stmt
struct DeclarationConstruct {
  UNION_CLASS_BOILERPLATE(DeclarationConstruct);
  std::variant<SpecificationConstruct, Statement<common::Indirection<DataStmt>>,
      Statement<common::Indirection<FormatStmt>>,
      Statement<common::Indirection<EntryStmt>>,
      Statement<common::Indirection<StmtFunctionStmt>>, ErrorRecovery>
      u;
};

// R504 specification-part -> [use-stmt]... [import-stmt]... [implicit-part]
//                            [declaration-construct]...
// PARAMETER, FORMAT, and ENTRY statements that appear before any other
// kind of declaration-construct will be parsed into the implicit-part,
// even if there are no IMPLICIT statements.
struct SpecificationPart {
  TUPLE_CLASS_BOILERPLATE(SpecificationPart);
  std::tuple<std::list<OpenACCDeclarativeConstruct>,
      std::list<OpenMPDeclarativeConstruct>,
      std::list<common::Indirection<CompilerDirective>>,
      std::list<Statement<common::Indirection<UseStmt>>>,
      std::list<Statement<common::Indirection<ImportStmt>>>, ImplicitPart,
      std::list<DeclarationConstruct>>
      t;
};

// R512 internal-subprogram -> function-subprogram | subroutine-subprogram
struct InternalSubprogram {
  UNION_CLASS_BOILERPLATE(InternalSubprogram);
  std::variant<common::Indirection<FunctionSubprogram>,
      common::Indirection<SubroutineSubprogram>,
      common::Indirection<CompilerDirective>>
      u;
};

// R1543 contains-stmt -> CONTAINS
EMPTY_CLASS(ContainsStmt);

// R511 internal-subprogram-part -> contains-stmt [internal-subprogram]...
struct InternalSubprogramPart {
  TUPLE_CLASS_BOILERPLATE(InternalSubprogramPart);
  std::tuple<Statement<ContainsStmt>, std::list<InternalSubprogram>> t;
};

// R1159 continue-stmt -> CONTINUE
EMPTY_CLASS(ContinueStmt);

// R1163 fail-image-stmt -> FAIL IMAGE
EMPTY_CLASS(FailImageStmt);

// R515 action-stmt ->
//        allocate-stmt | assignment-stmt | backspace-stmt | call-stmt |
//        close-stmt | continue-stmt | cycle-stmt | deallocate-stmt |
//        endfile-stmt | error-stop-stmt | event-post-stmt | event-wait-stmt |
//        exit-stmt | fail-image-stmt | flush-stmt | form-team-stmt |
//        goto-stmt | if-stmt | inquire-stmt | lock-stmt | notify-wait-stmt |
//        nullify-stmt | open-stmt | pointer-assignment-stmt | print-stmt |
//        read-stmt | return-stmt | rewind-stmt | stop-stmt | sync-all-stmt |
//        sync-images-stmt | sync-memory-stmt | sync-team-stmt | unlock-stmt |
//        wait-stmt | where-stmt | write-stmt | computed-goto-stmt | forall-stmt
struct ActionStmt {
  UNION_CLASS_BOILERPLATE(ActionStmt);
  std::variant<common::Indirection<AllocateStmt>,
      common::Indirection<AssignmentStmt>, common::Indirection<BackspaceStmt>,
      common::Indirection<CallStmt>, common::Indirection<CloseStmt>,
      ContinueStmt, common::Indirection<CycleStmt>,
      common::Indirection<DeallocateStmt>, common::Indirection<EndfileStmt>,
      common::Indirection<EventPostStmt>, common::Indirection<EventWaitStmt>,
      common::Indirection<ExitStmt>, FailImageStmt,
      common::Indirection<FlushStmt>, common::Indirection<FormTeamStmt>,
      common::Indirection<GotoStmt>, common::Indirection<IfStmt>,
      common::Indirection<InquireStmt>, common::Indirection<LockStmt>,
      common::Indirection<NotifyWaitStmt>, common::Indirection<NullifyStmt>,
      common::Indirection<OpenStmt>, common::Indirection<PointerAssignmentStmt>,
      common::Indirection<PrintStmt>, common::Indirection<ReadStmt>,
      common::Indirection<ReturnStmt>, common::Indirection<RewindStmt>,
      common::Indirection<StopStmt>, common::Indirection<SyncAllStmt>,
      common::Indirection<SyncImagesStmt>, common::Indirection<SyncMemoryStmt>,
      common::Indirection<SyncTeamStmt>, common::Indirection<UnlockStmt>,
      common::Indirection<WaitStmt>, common::Indirection<WhereStmt>,
      common::Indirection<WriteStmt>, common::Indirection<ComputedGotoStmt>,
      common::Indirection<ForallStmt>, common::Indirection<ArithmeticIfStmt>,
      common::Indirection<AssignStmt>, common::Indirection<AssignedGotoStmt>,
      common::Indirection<PauseStmt>>
      u;
};

// R514 executable-construct ->
//        action-stmt | associate-construct | block-construct |
//        case-construct | change-team-construct | critical-construct |
//        do-construct | if-construct | select-rank-construct |
//        select-type-construct | where-construct | forall-construct |
// (CUDA) CUF-kernel-do-construct
struct ExecutableConstruct {
  UNION_CLASS_BOILERPLATE(ExecutableConstruct);
  std::variant<Statement<ActionStmt>, common::Indirection<AssociateConstruct>,
      common::Indirection<BlockConstruct>, common::Indirection<CaseConstruct>,
      common::Indirection<ChangeTeamConstruct>,
      common::Indirection<CriticalConstruct>,
      Statement<common::Indirection<LabelDoStmt>>,
      Statement<common::Indirection<EndDoStmt>>,
      common::Indirection<DoConstruct>, common::Indirection<IfConstruct>,
      common::Indirection<SelectRankConstruct>,
      common::Indirection<SelectTypeConstruct>,
      common::Indirection<WhereConstruct>, common::Indirection<ForallConstruct>,
      common::Indirection<CompilerDirective>,
      common::Indirection<OpenACCConstruct>,
      common::Indirection<AccEndCombinedDirective>,
      common::Indirection<OpenMPConstruct>,
      common::Indirection<OmpEndLoopDirective>,
      common::Indirection<CUFKernelDoConstruct>>
      u;
};

// R510 execution-part-construct ->
//        executable-construct | format-stmt | entry-stmt | data-stmt
// Extension (PGI/Intel): also accept NAMELIST in execution part
struct ExecutionPartConstruct {
  UNION_CLASS_BOILERPLATE(ExecutionPartConstruct);
  std::variant<ExecutableConstruct, Statement<common::Indirection<FormatStmt>>,
      Statement<common::Indirection<EntryStmt>>,
      Statement<common::Indirection<DataStmt>>,
      Statement<common::Indirection<NamelistStmt>>, ErrorRecovery>
      u;
};

// R509 execution-part -> executable-construct [execution-part-construct]...
// R1101 block -> [execution-part-construct]...
using Block = std::list<ExecutionPartConstruct>;
WRAPPER_CLASS(ExecutionPart, Block);

// R502 program-unit ->
//        main-program | external-subprogram | module | submodule | block-data
// R503 external-subprogram -> function-subprogram | subroutine-subprogram
struct ProgramUnit {
  UNION_CLASS_BOILERPLATE(ProgramUnit);
  std::variant<common::Indirection<MainProgram>,
      common::Indirection<FunctionSubprogram>,
      common::Indirection<SubroutineSubprogram>, common::Indirection<Module>,
      common::Indirection<Submodule>, common::Indirection<BlockData>,
      common::Indirection<CompilerDirective>,
      common::Indirection<OpenACCRoutineConstruct>>
      u;
};

// R501 program -> program-unit [program-unit]...
// This is the top-level production.
WRAPPER_CLASS(Program, std::list<ProgramUnit>);

// R603 name -> letter [alphanumeric-character]...
struct Name {
  std::string ToString() const { return source.ToString(); }
  CharBlock source;
  mutable semantics::Symbol *symbol{nullptr}; // filled in during semantics
};

// R516 keyword -> name
WRAPPER_CLASS(Keyword, Name);

// R606 named-constant -> name
WRAPPER_CLASS(NamedConstant, Name);

// R1003 defined-unary-op -> . letter [letter]... .
// R1023 defined-binary-op -> . letter [letter]... .
// R1414 local-defined-operator -> defined-unary-op | defined-binary-op
// R1415 use-defined-operator -> defined-unary-op | defined-binary-op
// The Name here is stored with the dots; e.g., .FOO.
WRAPPER_CLASS(DefinedOpName, Name);

// R608 intrinsic-operator ->
//        ** | * | / | + | - | // | .LT. | .LE. | .EQ. | .NE. | .GE. | .GT. |
//        .NOT. | .AND. | .OR. | .EQV. | .NEQV.
// R609 defined-operator ->
//        defined-unary-op | defined-binary-op | extended-intrinsic-op
// R610 extended-intrinsic-op -> intrinsic-operator
struct DefinedOperator {
  UNION_CLASS_BOILERPLATE(DefinedOperator);
  ENUM_CLASS(IntrinsicOperator, Power, Multiply, Divide, Add, Subtract, Concat,
      LT, LE, EQ, NE, GE, GT, NOT, AND, OR, EQV, NEQV)
  std::variant<DefinedOpName, IntrinsicOperator> u;
};

// R804 object-name -> name
using ObjectName = Name;

// R867 import-stmt ->
//        IMPORT [[::] import-name-list] |
//        IMPORT , ONLY : import-name-list | IMPORT , NONE | IMPORT , ALL
struct ImportStmt {
  BOILERPLATE(ImportStmt);
  ImportStmt(common::ImportKind &&k) : kind{k} {}
  ImportStmt(std::list<Name> &&n) : names(std::move(n)) {}
  ImportStmt(common::ImportKind &&, std::list<Name> &&);
  common::ImportKind kind{common::ImportKind::Default};
  std::list<Name> names;
};

// R868 namelist-stmt ->
//        NAMELIST / namelist-group-name / namelist-group-object-list
//        [[,] / namelist-group-name / namelist-group-object-list]...
// R869 namelist-group-object -> variable-name
struct NamelistStmt {
  struct Group {
    TUPLE_CLASS_BOILERPLATE(Group);
    std::tuple<Name, std::list<Name>> t;
  };
  WRAPPER_CLASS_BOILERPLATE(NamelistStmt, std::list<Group>);
};

// R701 type-param-value -> scalar-int-expr | * | :
EMPTY_CLASS(Star);

struct TypeParamValue {
  UNION_CLASS_BOILERPLATE(TypeParamValue);
  EMPTY_CLASS(Deferred); // :
  std::variant<ScalarIntExpr, Star, Deferred> u;
};

// R706 kind-selector -> ( [KIND =] scalar-int-constant-expr )
// Legacy extension: kind-selector -> * digit-string
// N.B. These are not semantically identical in the case of COMPLEX.
struct KindSelector {
  UNION_CLASS_BOILERPLATE(KindSelector);
  WRAPPER_CLASS(StarSize, std::uint64_t);
  std::variant<ScalarIntConstantExpr, StarSize> u;
};

// R705 integer-type-spec -> INTEGER [kind-selector]
WRAPPER_CLASS(IntegerTypeSpec, std::optional<KindSelector>);

WRAPPER_CLASS(UnsignedTypeSpec, std::optional<KindSelector>);

// R723 char-length -> ( type-param-value ) | digit-string
struct CharLength {
  UNION_CLASS_BOILERPLATE(CharLength);
  std::variant<TypeParamValue, std::uint64_t> u;
};

// R722 length-selector -> ( [LEN =] type-param-value ) | * char-length [,]
struct LengthSelector {
  UNION_CLASS_BOILERPLATE(LengthSelector);
  std::variant<TypeParamValue, CharLength> u;
};

// R721 char-selector ->
//        length-selector |
//        ( LEN = type-param-value , KIND = scalar-int-constant-expr ) |
//        ( type-param-value , [KIND =] scalar-int-constant-expr ) |
//        ( KIND = scalar-int-constant-expr [, LEN = type-param-value] )
struct CharSelector {
  UNION_CLASS_BOILERPLATE(CharSelector);
  struct LengthAndKind {
    BOILERPLATE(LengthAndKind);
    LengthAndKind(std::optional<TypeParamValue> &&l, ScalarIntConstantExpr &&k)
        : length(std::move(l)), kind(std::move(k)) {}
    std::optional<TypeParamValue> length;
    ScalarIntConstantExpr kind;
  };
  CharSelector(TypeParamValue &&l, ScalarIntConstantExpr &&k)
      : u{LengthAndKind{std::make_optional(std::move(l)), std::move(k)}} {}
  CharSelector(ScalarIntConstantExpr &&k, std::optional<TypeParamValue> &&l)
      : u{LengthAndKind{std::move(l), std::move(k)}} {}
  std::variant<LengthSelector, LengthAndKind> u;
};

// R704 intrinsic-type-spec ->
//        integer-type-spec | REAL [kind-selector] | DOUBLE PRECISION |
//        COMPLEX [kind-selector] | CHARACTER [char-selector] |
//        LOGICAL [kind-selector]
// Extensions: DOUBLE COMPLEX & UNSIGNED [kind-selector]
struct IntrinsicTypeSpec {
  UNION_CLASS_BOILERPLATE(IntrinsicTypeSpec);
  struct Real {
    BOILERPLATE(Real);
    Real(std::optional<KindSelector> &&k) : kind{std::move(k)} {}
    std::optional<KindSelector> kind;
  };
  EMPTY_CLASS(DoublePrecision);
  struct Complex {
    BOILERPLATE(Complex);
    Complex(std::optional<KindSelector> &&k) : kind{std::move(k)} {}
    std::optional<KindSelector> kind;
  };
  struct Character {
    BOILERPLATE(Character);
    Character(std::optional<CharSelector> &&s) : selector{std::move(s)} {}
    std::optional<CharSelector> selector;
  };
  struct Logical {
    BOILERPLATE(Logical);
    Logical(std::optional<KindSelector> &&k) : kind{std::move(k)} {}
    std::optional<KindSelector> kind;
  };
  EMPTY_CLASS(DoubleComplex);
  std::variant<IntegerTypeSpec, UnsignedTypeSpec, Real, DoublePrecision,
      Complex, Character, Logical, DoubleComplex>
      u;
};

// Extension: Vector type
struct VectorElementType {
  UNION_CLASS_BOILERPLATE(VectorElementType);
  std::variant<IntegerTypeSpec, IntrinsicTypeSpec::Real, UnsignedTypeSpec> u;
};
WRAPPER_CLASS(IntrinsicVectorTypeSpec, VectorElementType);
struct VectorTypeSpec {
  UNION_CLASS_BOILERPLATE(VectorTypeSpec);
  EMPTY_CLASS(PairVectorTypeSpec);
  EMPTY_CLASS(QuadVectorTypeSpec);
  std::variant<IntrinsicVectorTypeSpec, PairVectorTypeSpec, QuadVectorTypeSpec>
      u;
};

// R755 type-param-spec -> [keyword =] type-param-value
struct TypeParamSpec {
  TUPLE_CLASS_BOILERPLATE(TypeParamSpec);
  std::tuple<std::optional<Keyword>, TypeParamValue> t;
};

// R754 derived-type-spec -> type-name [(type-param-spec-list)]
struct DerivedTypeSpec {
  TUPLE_CLASS_BOILERPLATE(DerivedTypeSpec);
  mutable const semantics::DerivedTypeSpec *derivedTypeSpec{nullptr};
  std::tuple<Name, std::list<TypeParamSpec>> t;
};

// R702 type-spec -> intrinsic-type-spec | derived-type-spec
struct TypeSpec {
  UNION_CLASS_BOILERPLATE(TypeSpec);
  mutable const semantics::DeclTypeSpec *declTypeSpec{nullptr};
  std::variant<IntrinsicTypeSpec, DerivedTypeSpec> u;
};

// R703 declaration-type-spec ->
//        intrinsic-type-spec | TYPE ( intrinsic-type-spec ) |
//        TYPE ( derived-type-spec ) | CLASS ( derived-type-spec ) |
//        CLASS ( * ) | TYPE ( * )
// Legacy extension: RECORD /struct/
struct DeclarationTypeSpec {
  UNION_CLASS_BOILERPLATE(DeclarationTypeSpec);
  struct Type {
    BOILERPLATE(Type);
    Type(DerivedTypeSpec &&dt) : derived(std::move(dt)) {}
    DerivedTypeSpec derived;
  };
  struct Class {
    BOILERPLATE(Class);
    Class(DerivedTypeSpec &&dt) : derived(std::move(dt)) {}
    DerivedTypeSpec derived;
  };
  EMPTY_CLASS(ClassStar);
  EMPTY_CLASS(TypeStar);
  WRAPPER_CLASS(Record, Name);
  std::variant<IntrinsicTypeSpec, Type, Class, ClassStar, TypeStar, Record,
      VectorTypeSpec>
      u;
};

// R709 kind-param -> digit-string | scalar-int-constant-name
struct KindParam {
  UNION_CLASS_BOILERPLATE(KindParam);
  std::variant<std::uint64_t, Scalar<Integer<Constant<Name>>>> u;
};

// R707 signed-int-literal-constant -> [sign] int-literal-constant
struct SignedIntLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(SignedIntLiteralConstant);
  CharBlock source;
  std::tuple<CharBlock, std::optional<KindParam>> t;
};

// R708 int-literal-constant -> digit-string [_ kind-param]
struct IntLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(IntLiteralConstant);
  std::tuple<CharBlock, std::optional<KindParam>> t;
};

// extension: unsigned-literal-constant -> digit-string U [_ kind-param]
struct UnsignedLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(UnsignedLiteralConstant);
  std::tuple<CharBlock, std::optional<KindParam>> t;
};

// R712 sign -> + | -
enum class Sign { Positive, Negative };

// R714 real-literal-constant ->
//        significand [exponent-letter exponent] [_ kind-param] |
//        digit-string exponent-letter exponent [_ kind-param]
// R715 significand -> digit-string . [digit-string] | . digit-string
// R717 exponent -> signed-digit-string
struct RealLiteralConstant {
  BOILERPLATE(RealLiteralConstant);
  struct Real {
    COPY_AND_ASSIGN_BOILERPLATE(Real);
    Real() {}
    CharBlock source;
  };
  RealLiteralConstant(Real &&r, std::optional<KindParam> &&k)
      : real{std::move(r)}, kind{std::move(k)} {}
  Real real;
  std::optional<KindParam> kind;
};

// R713 signed-real-literal-constant -> [sign] real-literal-constant
struct SignedRealLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(SignedRealLiteralConstant);
  std::tuple<std::optional<Sign>, RealLiteralConstant> t;
};

// R719 real-part ->
//        signed-int-literal-constant | signed-real-literal-constant |
//        named-constant
// R720 imag-part ->
//        signed-int-literal-constant | signed-real-literal-constant |
//        named-constant
struct ComplexPart {
  UNION_CLASS_BOILERPLATE(ComplexPart);
  std::variant<SignedIntLiteralConstant, SignedRealLiteralConstant,
      NamedConstant>
      u;
};

// R718 complex-literal-constant -> ( real-part , imag-part )
struct ComplexLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(ComplexLiteralConstant);
  std::tuple<ComplexPart, ComplexPart> t; // real, imaginary
};

// Extension: signed COMPLEX constant
struct SignedComplexLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(SignedComplexLiteralConstant);
  std::tuple<Sign, ComplexLiteralConstant> t;
};

// R724 char-literal-constant ->
//        [kind-param _] ' [rep-char]... ' |
//        [kind-param _] " [rep-char]... "
struct CharLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(CharLiteralConstant);
  std::tuple<std::optional<KindParam>, std::string> t;
  std::string GetString() const { return std::get<std::string>(t); }
};

// legacy extension
struct HollerithLiteralConstant {
  WRAPPER_CLASS_BOILERPLATE(HollerithLiteralConstant, std::string);
  std::string GetString() const { return v; }
};

// R725 logical-literal-constant ->
//        .TRUE. [_ kind-param] | .FALSE. [_ kind-param]
struct LogicalLiteralConstant {
  TUPLE_CLASS_BOILERPLATE(LogicalLiteralConstant);
  std::tuple<bool, std::optional<KindParam>> t;
};

// R764 boz-literal-constant -> binary-constant | octal-constant | hex-constant
// R765 binary-constant -> B ' digit [digit]... ' | B " digit [digit]... "
// R766 octal-constant -> O ' digit [digit]... ' | O " digit [digit]... "
// R767 hex-constant ->
//        Z ' hex-digit [hex-digit]... ' | Z " hex-digit [hex-digit]... "
// The constant must be large enough to hold any real or integer scalar
// of any supported kind (F'2018 7.7).
WRAPPER_CLASS(BOZLiteralConstant, std::string);

// R605 literal-constant ->
//        int-literal-constant | real-literal-constant |
//        complex-literal-constant | logical-literal-constant |
//        char-literal-constant | boz-literal-constant
struct LiteralConstant {
  UNION_CLASS_BOILERPLATE(LiteralConstant);
  std::variant<HollerithLiteralConstant, IntLiteralConstant,
      RealLiteralConstant, ComplexLiteralConstant, BOZLiteralConstant,
      CharLiteralConstant, LogicalLiteralConstant, UnsignedLiteralConstant>
      u;
};

// R807 access-spec -> PUBLIC | PRIVATE
struct AccessSpec {
  ENUM_CLASS(Kind, Public, Private)
  WRAPPER_CLASS_BOILERPLATE(AccessSpec, Kind);
};

// R728 type-attr-spec ->
//        ABSTRACT | access-spec | BIND(C) | EXTENDS ( parent-type-name )
EMPTY_CLASS(Abstract);
struct TypeAttrSpec {
  UNION_CLASS_BOILERPLATE(TypeAttrSpec);
  EMPTY_CLASS(BindC);
  WRAPPER_CLASS(Extends, Name);
  std::variant<Abstract, AccessSpec, BindC, Extends> u;
};

// R727 derived-type-stmt ->
//        TYPE [[, type-attr-spec-list] ::] type-name [( type-param-name-list )]
struct DerivedTypeStmt {
  TUPLE_CLASS_BOILERPLATE(DerivedTypeStmt);
  std::tuple<std::list<TypeAttrSpec>, Name, std::list<Name>> t;
};

// R731 sequence-stmt -> SEQUENCE
EMPTY_CLASS(SequenceStmt);

// R745 private-components-stmt -> PRIVATE
// R747 binding-private-stmt -> PRIVATE
EMPTY_CLASS(PrivateStmt);

// R729 private-or-sequence -> private-components-stmt | sequence-stmt
struct PrivateOrSequence {
  UNION_CLASS_BOILERPLATE(PrivateOrSequence);
  std::variant<PrivateStmt, SequenceStmt> u;
};

// R733 type-param-decl -> type-param-name [= scalar-int-constant-expr]
struct TypeParamDecl {
  TUPLE_CLASS_BOILERPLATE(TypeParamDecl);
  std::tuple<Name, std::optional<ScalarIntConstantExpr>> t;
};

// R732 type-param-def-stmt ->
//        integer-type-spec , type-param-attr-spec :: type-param-decl-list
// R734 type-param-attr-spec -> KIND | LEN
struct TypeParamDefStmt {
  TUPLE_CLASS_BOILERPLATE(TypeParamDefStmt);
  std::tuple<IntegerTypeSpec, common::TypeParamAttr, std::list<TypeParamDecl>>
      t;
};

// R1028 specification-expr -> scalar-int-expr
WRAPPER_CLASS(SpecificationExpr, ScalarIntExpr);

// R816 explicit-shape-spec -> [lower-bound :] upper-bound
// R817 lower-bound -> specification-expr
// R818 upper-bound -> specification-expr
struct ExplicitShapeSpec {
  TUPLE_CLASS_BOILERPLATE(ExplicitShapeSpec);
  std::tuple<std::optional<SpecificationExpr>, SpecificationExpr> t;
};

// R810 deferred-coshape-spec -> :
// deferred-coshape-spec-list is just a count of the colons (i.e., the rank).
WRAPPER_CLASS(DeferredCoshapeSpecList, int);

// R811 explicit-coshape-spec ->
//        [[lower-cobound :] upper-cobound ,]... [lower-cobound :] *
// R812 lower-cobound -> specification-expr
// R813 upper-cobound -> specification-expr
struct ExplicitCoshapeSpec {
  TUPLE_CLASS_BOILERPLATE(ExplicitCoshapeSpec);
  std::tuple<std::list<ExplicitShapeSpec>, std::optional<SpecificationExpr>> t;
};

// R809 coarray-spec -> deferred-coshape-spec-list | explicit-coshape-spec
struct CoarraySpec {
  UNION_CLASS_BOILERPLATE(CoarraySpec);
  std::variant<DeferredCoshapeSpecList, ExplicitCoshapeSpec> u;
};

// R820 deferred-shape-spec -> :
// deferred-shape-spec-list is just a count of the colons (i.e., the rank).
WRAPPER_CLASS(DeferredShapeSpecList, int);

// R740 component-array-spec ->
//        explicit-shape-spec-list | deferred-shape-spec-list
struct ComponentArraySpec {
  UNION_CLASS_BOILERPLATE(ComponentArraySpec);
  std::variant<std::list<ExplicitShapeSpec>, DeferredShapeSpecList> u;
};

// R738 component-attr-spec ->
//        access-spec | ALLOCATABLE |
//        CODIMENSION lbracket coarray-spec rbracket |
//        CONTIGUOUS | DIMENSION ( component-array-spec ) | POINTER |
// (CUDA) CONSTANT | DEVICE | MANAGED | PINNED | SHARED | TEXTURE | UNIFIED
EMPTY_CLASS(Allocatable);
EMPTY_CLASS(Pointer);
EMPTY_CLASS(Contiguous);
struct ComponentAttrSpec {
  UNION_CLASS_BOILERPLATE(ComponentAttrSpec);
  std::variant<AccessSpec, Allocatable, CoarraySpec, Contiguous,
      ComponentArraySpec, Pointer, common::CUDADataAttr, ErrorRecovery>
      u;
};

// R806 null-init -> function-reference   ... which must be NULL()
WRAPPER_CLASS(NullInit, common::Indirection<Expr>);

// R744 initial-data-target -> designator
using InitialDataTarget = common::Indirection<Designator>;

// R743 component-initialization ->
//        = constant-expr | => null-init | => initial-data-target
// R805 initialization ->
//        = constant-expr | => null-init | => initial-data-target
// Universal extension: initialization -> / data-stmt-value-list /
struct Initialization {
  UNION_CLASS_BOILERPLATE(Initialization);
  std::variant<ConstantExpr, NullInit, InitialDataTarget,
      std::list<common::Indirection<DataStmtValue>>>
      u;
};

// R739 component-decl ->
//        component-name [( component-array-spec )]
//          [lbracket coarray-spec rbracket] [* char-length]
//          [component-initialization] |
//        component-name *char-length [( component-array-spec )]
//          [lbracket coarray-spec rbracket] [component-initialization]
struct ComponentDecl {
  TUPLE_CLASS_BOILERPLATE(ComponentDecl);
  ComponentDecl(Name &&name, CharLength &&length,
      std::optional<ComponentArraySpec> &&aSpec,
      std::optional<CoarraySpec> &&coaSpec,
      std::optional<Initialization> &&init)
      : t{std::move(name), std::move(aSpec), std::move(coaSpec),
            std::move(length), std::move(init)} {}
  std::tuple<Name, std::optional<ComponentArraySpec>,
      std::optional<CoarraySpec>, std::optional<CharLength>,
      std::optional<Initialization>>
      t;
};

// A %FILL component for a DEC STRUCTURE.  The name will be replaced
// with a distinct compiler-generated name.
struct FillDecl {
  TUPLE_CLASS_BOILERPLATE(FillDecl);
  std::tuple<Name, std::optional<ComponentArraySpec>, std::optional<CharLength>>
      t;
};

struct ComponentOrFill {
  UNION_CLASS_BOILERPLATE(ComponentOrFill);
  std::variant<ComponentDecl, FillDecl> u;
};

// R737 data-component-def-stmt ->
//        declaration-type-spec [[, component-attr-spec-list] ::]
//        component-decl-list
struct DataComponentDefStmt {
  TUPLE_CLASS_BOILERPLATE(DataComponentDefStmt);
  std::tuple<DeclarationTypeSpec, std::list<ComponentAttrSpec>,
      std::list<ComponentOrFill>>
      t;
};

// R742 proc-component-attr-spec ->
//        access-spec | NOPASS | PASS [(arg-name)] | POINTER
EMPTY_CLASS(NoPass);
WRAPPER_CLASS(Pass, std::optional<Name>);
struct ProcComponentAttrSpec {
  UNION_CLASS_BOILERPLATE(ProcComponentAttrSpec);
  std::variant<AccessSpec, NoPass, Pass, Pointer> u;
};

// R1517 proc-pointer-init -> null-init | initial-proc-target
// R1518 initial-proc-target -> procedure-name
struct ProcPointerInit {
  UNION_CLASS_BOILERPLATE(ProcPointerInit);
  std::variant<NullInit, Name> u;
};

// R1513 proc-interface -> interface-name | declaration-type-spec
// R1516 interface-name -> name
struct ProcInterface {
  UNION_CLASS_BOILERPLATE(ProcInterface);
  std::variant<Name, DeclarationTypeSpec> u;
};

// R1515 proc-decl -> procedure-entity-name [=> proc-pointer-init]
struct ProcDecl {
  TUPLE_CLASS_BOILERPLATE(ProcDecl);
  std::tuple<Name, std::optional<ProcPointerInit>> t;
};

// R741 proc-component-def-stmt ->
//        PROCEDURE ( [proc-interface] ) , proc-component-attr-spec-list
//          :: proc-decl-list
struct ProcComponentDefStmt {
  TUPLE_CLASS_BOILERPLATE(ProcComponentDefStmt);
  std::tuple<std::optional<ProcInterface>, std::list<ProcComponentAttrSpec>,
      std::list<ProcDecl>>
      t;
};

// R736 component-def-stmt -> data-component-def-stmt | proc-component-def-stmt
struct ComponentDefStmt {
  UNION_CLASS_BOILERPLATE(ComponentDefStmt);
  std::variant<DataComponentDefStmt, ProcComponentDefStmt,
      common::Indirection<CompilerDirective>, ErrorRecovery
      // , TypeParamDefStmt -- PGI accidental extension, not enabled
      >
      u;
};

// R752 bind-attr ->
//        access-spec | DEFERRED | NON_OVERRIDABLE | NOPASS | PASS [(arg-name)]
struct BindAttr {
  UNION_CLASS_BOILERPLATE(BindAttr);
  EMPTY_CLASS(Deferred);
  EMPTY_CLASS(Non_Overridable);
  std::variant<AccessSpec, Deferred, Non_Overridable, NoPass, Pass> u;
};

// R750 type-bound-proc-decl -> binding-name [=> procedure-name]
struct TypeBoundProcDecl {
  TUPLE_CLASS_BOILERPLATE(TypeBoundProcDecl);
  std::tuple<Name, std::optional<Name>> t;
};

// R749 type-bound-procedure-stmt ->
//        PROCEDURE [[, bind-attr-list] ::] type-bound-proc-decl-list |
//        PROCEDURE ( interface-name ) , bind-attr-list :: binding-name-list
// The second form, with interface-name, requires DEFERRED in bind-attr-list,
// and thus can appear only in an abstract type.
struct TypeBoundProcedureStmt {
  UNION_CLASS_BOILERPLATE(TypeBoundProcedureStmt);
  struct WithoutInterface {
    BOILERPLATE(WithoutInterface);
    WithoutInterface(
        std::list<BindAttr> &&as, std::list<TypeBoundProcDecl> &&ds)
        : attributes(std::move(as)), declarations(std::move(ds)) {}
    std::list<BindAttr> attributes;
    std::list<TypeBoundProcDecl> declarations;
  };
  struct WithInterface {
    BOILERPLATE(WithInterface);
    WithInterface(Name &&n, std::list<BindAttr> &&as, std::list<Name> &&bs)
        : interfaceName(std::move(n)), attributes(std::move(as)),
          bindingNames(std::move(bs)) {}
    Name interfaceName;
    std::list<BindAttr> attributes;
    std::list<Name> bindingNames;
  };
  std::variant<WithoutInterface, WithInterface> u;
};

// R751 type-bound-generic-stmt ->
//        GENERIC [, access-spec] :: generic-spec => binding-name-list
struct TypeBoundGenericStmt {
  TUPLE_CLASS_BOILERPLATE(TypeBoundGenericStmt);
  std::tuple<std::optional<AccessSpec>, common::Indirection<GenericSpec>,
      std::list<Name>>
      t;
};

// R753 final-procedure-stmt -> FINAL [::] final-subroutine-name-list
WRAPPER_CLASS(FinalProcedureStmt, std::list<Name>);

// R748 type-bound-proc-binding ->
//        type-bound-procedure-stmt | type-bound-generic-stmt |
//        final-procedure-stmt
struct TypeBoundProcBinding {
  UNION_CLASS_BOILERPLATE(TypeBoundProcBinding);
  std::variant<TypeBoundProcedureStmt, TypeBoundGenericStmt, FinalProcedureStmt,
      ErrorRecovery>
      u;
};

// R746 type-bound-procedure-part ->
//        contains-stmt [binding-private-stmt] [type-bound-proc-binding]...
struct TypeBoundProcedurePart {
  TUPLE_CLASS_BOILERPLATE(TypeBoundProcedurePart);
  std::tuple<Statement<ContainsStmt>, std::optional<Statement<PrivateStmt>>,
      std::list<Statement<TypeBoundProcBinding>>>
      t;
};

// R730 end-type-stmt -> END TYPE [type-name]
WRAPPER_CLASS(EndTypeStmt, std::optional<Name>);

// R726 derived-type-def ->
//        derived-type-stmt [type-param-def-stmt]... [private-or-sequence]...
//        [component-part] [type-bound-procedure-part] end-type-stmt
// R735 component-part -> [component-def-stmt]...
struct DerivedTypeDef {
  TUPLE_CLASS_BOILERPLATE(DerivedTypeDef);
  std::tuple<Statement<DerivedTypeStmt>, std::list<Statement<TypeParamDefStmt>>,
      std::list<Statement<PrivateOrSequence>>,
      std::list<Statement<ComponentDefStmt>>,
      std::optional<TypeBoundProcedurePart>, Statement<EndTypeStmt>>
      t;
};

// R758 component-data-source -> expr | data-target | proc-target
// R1037 data-target -> expr
// R1040 proc-target -> expr | procedure-name | proc-component-ref
WRAPPER_CLASS(ComponentDataSource, common::Indirection<Expr>);

// R757 component-spec -> [keyword =] component-data-source
struct ComponentSpec {
  TUPLE_CLASS_BOILERPLATE(ComponentSpec);
  std::tuple<std::optional<Keyword>, ComponentDataSource> t;
};

// R756 structure-constructor -> derived-type-spec ( [component-spec-list] )
struct StructureConstructor {
  TUPLE_CLASS_BOILERPLATE(StructureConstructor);
  std::tuple<DerivedTypeSpec, std::list<ComponentSpec>> t;
};

// R760 enum-def-stmt -> ENUM, BIND(C)
EMPTY_CLASS(EnumDefStmt);

// R762 enumerator -> named-constant [= scalar-int-constant-expr]
struct Enumerator {
  TUPLE_CLASS_BOILERPLATE(Enumerator);
  std::tuple<NamedConstant, std::optional<ScalarIntConstantExpr>> t;
};

// R761 enumerator-def-stmt -> ENUMERATOR [::] enumerator-list
WRAPPER_CLASS(EnumeratorDefStmt, std::list<Enumerator>);

// R763 end-enum-stmt -> END ENUM
EMPTY_CLASS(EndEnumStmt);

// R759 enum-def ->
//        enum-def-stmt enumerator-def-stmt [enumerator-def-stmt]...
//        end-enum-stmt
struct EnumDef {
  TUPLE_CLASS_BOILERPLATE(EnumDef);
  std::tuple<Statement<EnumDefStmt>, std::list<Statement<EnumeratorDefStmt>>,
      Statement<EndEnumStmt>>
      t;
};

// R773 ac-value -> expr | ac-implied-do
struct AcValue {
  struct Triplet { // PGI/Intel extension
    TUPLE_CLASS_BOILERPLATE(Triplet);
    std::tuple<ScalarIntExpr, ScalarIntExpr, std::optional<ScalarIntExpr>> t;
  };
  UNION_CLASS_BOILERPLATE(AcValue);
  std::variant<Triplet, common::Indirection<Expr>,
      common::Indirection<AcImpliedDo>>
      u;
};

// R770 ac-spec -> type-spec :: | [type-spec ::] ac-value-list
struct AcSpec {
  BOILERPLATE(AcSpec);
  AcSpec(std::optional<TypeSpec> &&ts, std::list<AcValue> &&xs)
      : type(std::move(ts)), values(std::move(xs)) {}
  explicit AcSpec(TypeSpec &&ts) : type{std::move(ts)} {}
  std::optional<TypeSpec> type;
  std::list<AcValue> values;
};

// R769 array-constructor -> (/ ac-spec /) | lbracket ac-spec rbracket
WRAPPER_CLASS(ArrayConstructor, AcSpec);

// R1124 do-variable -> scalar-int-variable-name
using DoVariable = Scalar<Integer<Name>>;

template <typename VAR, typename BOUND> struct LoopBounds {
  LoopBounds(LoopBounds &&that) = default;
  LoopBounds(
      VAR &&name, BOUND &&lower, BOUND &&upper, std::optional<BOUND> &&step)
      : name{std::move(name)}, lower{std::move(lower)}, upper{std::move(upper)},
        step{std::move(step)} {}
  LoopBounds &operator=(LoopBounds &&) = default;
  VAR name;
  BOUND lower, upper;
  std::optional<BOUND> step;
};

using ScalarName = Scalar<Name>;
using ScalarExpr = Scalar<common::Indirection<Expr>>;

// R775 ac-implied-do-control ->
//        [integer-type-spec ::] ac-do-variable = scalar-int-expr ,
//        scalar-int-expr [, scalar-int-expr]
// R776 ac-do-variable -> do-variable
struct AcImpliedDoControl {
  TUPLE_CLASS_BOILERPLATE(AcImpliedDoControl);
  using Bounds = LoopBounds<DoVariable, ScalarIntExpr>;
  std::tuple<std::optional<IntegerTypeSpec>, Bounds> t;
};

// R774 ac-implied-do -> ( ac-value-list , ac-implied-do-control )
struct AcImpliedDo {
  TUPLE_CLASS_BOILERPLATE(AcImpliedDo);
  std::tuple<std::list<AcValue>, AcImpliedDoControl> t;
};

// R808 language-binding-spec ->
//        BIND ( C [, NAME = scalar-default-char-constant-expr ]
//                 [, CDEFINED ] )
// R1528 proc-language-binding-spec -> language-binding-spec
struct LanguageBindingSpec {
  TUPLE_CLASS_BOILERPLATE(LanguageBindingSpec);
  std::tuple<std::optional<ScalarDefaultCharConstantExpr>, bool> t;
};

// R852 named-constant-def -> named-constant = constant-expr
struct NamedConstantDef {
  TUPLE_CLASS_BOILERPLATE(NamedConstantDef);
  std::tuple<NamedConstant, ConstantExpr> t;
};

// R851 parameter-stmt -> PARAMETER ( named-constant-def-list )
WRAPPER_CLASS(ParameterStmt, std::list<NamedConstantDef>);

// R819 assumed-shape-spec -> [lower-bound] :
WRAPPER_CLASS(AssumedShapeSpec, std::optional<SpecificationExpr>);

// R821 assumed-implied-spec -> [lower-bound :] *
WRAPPER_CLASS(AssumedImpliedSpec, std::optional<SpecificationExpr>);

// R822 assumed-size-spec -> explicit-shape-spec-list , assumed-implied-spec
struct AssumedSizeSpec {
  TUPLE_CLASS_BOILERPLATE(AssumedSizeSpec);
  std::tuple<std::list<ExplicitShapeSpec>, AssumedImpliedSpec> t;
};

// R823 implied-shape-or-assumed-size-spec -> assumed-implied-spec
// R824 implied-shape-spec -> assumed-implied-spec , assumed-implied-spec-list
// I.e., when the assumed-implied-spec-list has a single item, it constitutes an
// implied-shape-or-assumed-size-spec; otherwise, an implied-shape-spec.
WRAPPER_CLASS(ImpliedShapeSpec, std::list<AssumedImpliedSpec>);

// R825 assumed-rank-spec -> ..
EMPTY_CLASS(AssumedRankSpec);

// R815 array-spec ->
//        explicit-shape-spec-list | assumed-shape-spec-list |
//        deferred-shape-spec-list | assumed-size-spec | implied-shape-spec |
//        implied-shape-or-assumed-size-spec | assumed-rank-spec
struct ArraySpec {
  UNION_CLASS_BOILERPLATE(ArraySpec);
  std::variant<std::list<ExplicitShapeSpec>, std::list<AssumedShapeSpec>,
      DeferredShapeSpecList, AssumedSizeSpec, ImpliedShapeSpec, AssumedRankSpec>
      u;
};

// R826 intent-spec -> IN | OUT | INOUT
struct IntentSpec {
  ENUM_CLASS(Intent, In, Out, InOut)
  WRAPPER_CLASS_BOILERPLATE(IntentSpec, Intent);
};

// R802 attr-spec ->
//        access-spec | ALLOCATABLE | ASYNCHRONOUS |
//        CODIMENSION lbracket coarray-spec rbracket | CONTIGUOUS |
//        DIMENSION ( array-spec ) | EXTERNAL | INTENT ( intent-spec ) |
//        INTRINSIC | language-binding-spec | OPTIONAL | PARAMETER | POINTER |
//        PROTECTED | SAVE | TARGET | VALUE | VOLATILE |
// (CUDA) CONSTANT | DEVICE | MANAGED | PINNED | SHARED | TEXTURE
EMPTY_CLASS(Asynchronous);
EMPTY_CLASS(External);
EMPTY_CLASS(Intrinsic);
EMPTY_CLASS(Optional);
EMPTY_CLASS(Parameter);
EMPTY_CLASS(Protected);
EMPTY_CLASS(Save);
EMPTY_CLASS(Target);
EMPTY_CLASS(Value);
EMPTY_CLASS(Volatile);
struct AttrSpec {
  UNION_CLASS_BOILERPLATE(AttrSpec);
  std::variant<AccessSpec, Allocatable, Asynchronous, CoarraySpec, Contiguous,
      ArraySpec, External, IntentSpec, Intrinsic, LanguageBindingSpec, Optional,
      Parameter, Pointer, Protected, Save, Target, Value, Volatile,
      common::CUDADataAttr>
      u;
};

// R803 entity-decl ->
//        object-name [( array-spec )] [lbracket coarray-spec rbracket]
//          [* char-length] [initialization] |
//        function-name [* char-length] |
// (ext.) object-name *char-length [( array-spec )]
//          [lbracket coarray-spec rbracket] [initialization]
struct EntityDecl {
  TUPLE_CLASS_BOILERPLATE(EntityDecl);
  EntityDecl(ObjectName &&name, CharLength &&length,
      std::optional<ArraySpec> &&aSpec, std::optional<CoarraySpec> &&coaSpec,
      std::optional<Initialization> &&init)
      : t{std::move(name), std::move(aSpec), std::move(coaSpec),
            std::move(length), std::move(init)} {}
  std::tuple<ObjectName, std::optional<ArraySpec>, std::optional<CoarraySpec>,
      std::optional<CharLength>, std::optional<Initialization>>
      t;
};

// R801 type-declaration-stmt ->
//        declaration-type-spec [[, attr-spec]... ::] entity-decl-list
struct TypeDeclarationStmt {
  TUPLE_CLASS_BOILERPLATE(TypeDeclarationStmt);
  std::tuple<DeclarationTypeSpec, std::list<AttrSpec>, std::list<EntityDecl>> t;
};

// R828 access-id -> access-name | generic-spec
// "access-name" is ambiguous with "generic-spec", so that's what's parsed
WRAPPER_CLASS(AccessId, common::Indirection<GenericSpec>);

// R827 access-stmt -> access-spec [[::] access-id-list]
struct AccessStmt {
  TUPLE_CLASS_BOILERPLATE(AccessStmt);
  std::tuple<AccessSpec, std::list<AccessId>> t;
};

// R830 allocatable-decl ->
//        object-name [( array-spec )] [lbracket coarray-spec rbracket]
// R860 target-decl ->
//        object-name [( array-spec )] [lbracket coarray-spec rbracket]
struct ObjectDecl {
  TUPLE_CLASS_BOILERPLATE(ObjectDecl);
  std::tuple<ObjectName, std::optional<ArraySpec>, std::optional<CoarraySpec>>
      t;
};

// R829 allocatable-stmt -> ALLOCATABLE [::] allocatable-decl-list
WRAPPER_CLASS(AllocatableStmt, std::list<ObjectDecl>);

// R831 asynchronous-stmt -> ASYNCHRONOUS [::] object-name-list
WRAPPER_CLASS(AsynchronousStmt, std::list<ObjectName>);

// R833 bind-entity -> entity-name | / common-block-name /
struct BindEntity {
  TUPLE_CLASS_BOILERPLATE(BindEntity);
  ENUM_CLASS(Kind, Object, Common)
  std::tuple<Kind, Name> t;
};

// R832 bind-stmt -> language-binding-spec [::] bind-entity-list
struct BindStmt {
  TUPLE_CLASS_BOILERPLATE(BindStmt);
  std::tuple<LanguageBindingSpec, std::list<BindEntity>> t;
};

// R835 codimension-decl -> coarray-name lbracket coarray-spec rbracket
struct CodimensionDecl {
  TUPLE_CLASS_BOILERPLATE(CodimensionDecl);
  std::tuple<Name, CoarraySpec> t;
};

// R834 codimension-stmt -> CODIMENSION [::] codimension-decl-list
WRAPPER_CLASS(CodimensionStmt, std::list<CodimensionDecl>);

// R836 contiguous-stmt -> CONTIGUOUS [::] object-name-list
WRAPPER_CLASS(ContiguousStmt, std::list<ObjectName>);

// R847 constant-subobject -> designator
// R846 int-constant-subobject -> constant-subobject
using ConstantSubobject = Constant<common::Indirection<Designator>>;

// Represents an analyzed expression
using TypedExpr = common::ForwardOwningPointer<evaluate::GenericExprWrapper>;

// R845 data-stmt-constant ->
//        scalar-constant | scalar-constant-subobject |
//        signed-int-literal-constant | signed-real-literal-constant |
//        null-init | initial-data-target |
//        structure-constructor
// N.B. Parsing ambiguities abound here without recourse to symbols
// (see comments on R845's parser).
struct DataStmtConstant {
  UNION_CLASS_BOILERPLATE(DataStmtConstant);
  CharBlock source;
  mutable TypedExpr typedExpr;
  std::variant<common::Indirection<CharLiteralConstantSubstring>,
      LiteralConstant, SignedIntLiteralConstant, SignedRealLiteralConstant,
      SignedComplexLiteralConstant, NullInit, common::Indirection<Designator>,
      StructureConstructor, UnsignedLiteralConstant>
      u;
};

// R844 data-stmt-repeat -> scalar-int-constant | scalar-int-constant-subobject
// R607 int-constant -> constant
// R604 constant -> literal-constant | named-constant
// (only literal-constant -> int-literal-constant applies)
struct DataStmtRepeat {
  UNION_CLASS_BOILERPLATE(DataStmtRepeat);
  std::variant<IntLiteralConstant, Scalar<Integer<ConstantSubobject>>> u;
};

// R843 data-stmt-value -> [data-stmt-repeat *] data-stmt-constant
struct DataStmtValue {
  TUPLE_CLASS_BOILERPLATE(DataStmtValue);
  mutable std::int64_t repetitions{1}; // replaced during semantics
  std::tuple<std::optional<DataStmtRepeat>, DataStmtConstant> t;
};

// R841 data-i-do-object ->
//        array-element | scalar-structure-component | data-implied-do
struct DataIDoObject {
  UNION_CLASS_BOILERPLATE(DataIDoObject);
  std::variant<Scalar<common::Indirection<Designator>>,
      common::Indirection<DataImpliedDo>>
      u;
};

// R840 data-implied-do ->
//        ( data-i-do-object-list , [integer-type-spec ::] data-i-do-variable
//        = scalar-int-constant-expr , scalar-int-constant-expr
//        [, scalar-int-constant-expr] )
// R842 data-i-do-variable -> do-variable
struct DataImpliedDo {
  TUPLE_CLASS_BOILERPLATE(DataImpliedDo);
  using Bounds = LoopBounds<DoVariable, ScalarIntConstantExpr>;
  std::tuple<std::list<DataIDoObject>, std::optional<IntegerTypeSpec>, Bounds>
      t;
};

// R839 data-stmt-object -> variable | data-implied-do
struct DataStmtObject {
  UNION_CLASS_BOILERPLATE(DataStmtObject);
  std::variant<common::Indirection<Variable>, DataImpliedDo> u;
};

// R838 data-stmt-set -> data-stmt-object-list / data-stmt-value-list /
struct DataStmtSet {
  TUPLE_CLASS_BOILERPLATE(DataStmtSet);
  std::tuple<std::list<DataStmtObject>, std::list<DataStmtValue>> t;
};

// R837 data-stmt -> DATA data-stmt-set [[,] data-stmt-set]...
WRAPPER_CLASS(DataStmt, std::list<DataStmtSet>);

// R848 dimension-stmt ->
//        DIMENSION [::] array-name ( array-spec )
//        [, array-name ( array-spec )]...
struct DimensionStmt {
  struct Declaration {
    TUPLE_CLASS_BOILERPLATE(Declaration);
    std::tuple<Name, ArraySpec> t;
  };
  WRAPPER_CLASS_BOILERPLATE(DimensionStmt, std::list<Declaration>);
};

// R849 intent-stmt -> INTENT ( intent-spec ) [::] dummy-arg-name-list
struct IntentStmt {
  TUPLE_CLASS_BOILERPLATE(IntentStmt);
  std::tuple<IntentSpec, std::list<Name>> t;
};

// R850 optional-stmt -> OPTIONAL [::] dummy-arg-name-list
WRAPPER_CLASS(OptionalStmt, std::list<Name>);

// R854 pointer-decl ->
//        object-name [( deferred-shape-spec-list )] | proc-entity-name
struct PointerDecl {
  TUPLE_CLASS_BOILERPLATE(PointerDecl);
  std::tuple<Name, std::optional<DeferredShapeSpecList>> t;
};

// R853 pointer-stmt -> POINTER [::] pointer-decl-list
WRAPPER_CLASS(PointerStmt, std::list<PointerDecl>);

// R855 protected-stmt -> PROTECTED [::] entity-name-list
WRAPPER_CLASS(ProtectedStmt, std::list<Name>);

// R857 saved-entity -> object-name | proc-pointer-name | / common-block-name /
// R858 proc-pointer-name -> name
struct SavedEntity {
  TUPLE_CLASS_BOILERPLATE(SavedEntity);
  ENUM_CLASS(Kind, Entity, Common)
  std::tuple<Kind, Name> t;
};

// R856 save-stmt -> SAVE [[::] saved-entity-list]
WRAPPER_CLASS(SaveStmt, std::list<SavedEntity>);

// R859 target-stmt -> TARGET [::] target-decl-list
WRAPPER_CLASS(TargetStmt, std::list<ObjectDecl>);

// R861 value-stmt -> VALUE [::] dummy-arg-name-list
WRAPPER_CLASS(ValueStmt, std::list<Name>);

// R862 volatile-stmt -> VOLATILE [::] object-name-list
WRAPPER_CLASS(VolatileStmt, std::list<ObjectName>);

// R865 letter-spec -> letter [- letter]
struct LetterSpec {
  TUPLE_CLASS_BOILERPLATE(LetterSpec);
  std::tuple<Location, std::optional<Location>> t;
};

// R864 implicit-spec -> declaration-type-spec ( letter-spec-list )
struct ImplicitSpec {
  TUPLE_CLASS_BOILERPLATE(ImplicitSpec);
  std::tuple<DeclarationTypeSpec, std::list<LetterSpec>> t;
};

// R863 implicit-stmt ->
//        IMPLICIT implicit-spec-list |
//        IMPLICIT NONE [( [implicit-name-spec-list] )]
// R866 implicit-name-spec -> EXTERNAL | TYPE
struct ImplicitStmt {
  UNION_CLASS_BOILERPLATE(ImplicitStmt);
  ENUM_CLASS(ImplicitNoneNameSpec, External, Type) // R866
  std::variant<std::list<ImplicitSpec>, std::list<ImplicitNoneNameSpec>> u;
};

// R874 common-block-object -> variable-name [( array-spec )]
struct CommonBlockObject {
  TUPLE_CLASS_BOILERPLATE(CommonBlockObject);
  std::tuple<Name, std::optional<ArraySpec>> t;
};

// R873 common-stmt ->
//        COMMON [/ [common-block-name] /] common-block-object-list
//        [[,] / [common-block-name] / common-block-object-list]...
struct CommonStmt {
  struct Block {
    TUPLE_CLASS_BOILERPLATE(Block);
    std::tuple<std::optional<Name>, std::list<CommonBlockObject>> t;
  };
  BOILERPLATE(CommonStmt);
  CommonStmt(std::optional<Name> &&, std::list<CommonBlockObject> &&,
      std::list<Block> &&);
  std::list<Block> blocks;
};

// R872 equivalence-object -> variable-name | array-element | substring
WRAPPER_CLASS(EquivalenceObject, common::Indirection<Designator>);

// R870 equivalence-stmt -> EQUIVALENCE equivalence-set-list
// R871 equivalence-set -> ( equivalence-object , equivalence-object-list )
WRAPPER_CLASS(EquivalenceStmt, std::list<std::list<EquivalenceObject>>);

// R910 substring-range -> [scalar-int-expr] : [scalar-int-expr]
struct SubstringRange {
  TUPLE_CLASS_BOILERPLATE(SubstringRange);
  std::tuple<std::optional<ScalarIntExpr>, std::optional<ScalarIntExpr>> t;
};

// R919 subscript -> scalar-int-expr
using Subscript = ScalarIntExpr;

// R921 subscript-triplet -> [subscript] : [subscript] [: stride]
struct SubscriptTriplet {
  TUPLE_CLASS_BOILERPLATE(SubscriptTriplet);
  std::tuple<std::optional<Subscript>, std::optional<Subscript>,
      std::optional<Subscript>>
      t;
};

// R920 section-subscript -> subscript | subscript-triplet | vector-subscript
// R923 vector-subscript -> int-expr
struct SectionSubscript {
  UNION_CLASS_BOILERPLATE(SectionSubscript);
  std::variant<IntExpr, SubscriptTriplet> u;
};

// R925 cosubscript -> scalar-int-expr
using Cosubscript = ScalarIntExpr;

// R1115 team-value -> scalar-expr
WRAPPER_CLASS(TeamValue, Scalar<common::Indirection<Expr>>);

// R926 image-selector-spec ->
//        STAT = stat-variable | TEAM = team-value |
//        TEAM_NUMBER = scalar-int-expr
struct ImageSelectorSpec {
  WRAPPER_CLASS(Stat, Scalar<Integer<common::Indirection<Variable>>>);
  WRAPPER_CLASS(Team_Number, ScalarIntExpr);
  UNION_CLASS_BOILERPLATE(ImageSelectorSpec);
  std::variant<Stat, TeamValue, Team_Number> u;
};

// R924 image-selector ->
//        lbracket cosubscript-list [, image-selector-spec-list] rbracket
struct ImageSelector {
  TUPLE_CLASS_BOILERPLATE(ImageSelector);
  std::tuple<std::list<Cosubscript>, std::list<ImageSelectorSpec>> t;
};

// R1001 - R1022 expressions
struct Expr {
  UNION_CLASS_BOILERPLATE(Expr);

  WRAPPER_CLASS(IntrinsicUnary, common::Indirection<Expr>);
  struct Parentheses : public IntrinsicUnary {
    using IntrinsicUnary::IntrinsicUnary;
  };
  struct UnaryPlus : public IntrinsicUnary {
    using IntrinsicUnary::IntrinsicUnary;
  };
  struct Negate : public IntrinsicUnary {
    using IntrinsicUnary::IntrinsicUnary;
  };
  struct NOT : public IntrinsicUnary {
    using IntrinsicUnary::IntrinsicUnary;
  };

  WRAPPER_CLASS(PercentLoc, common::Indirection<Variable>); // %LOC(v) extension

  struct DefinedUnary {
    TUPLE_CLASS_BOILERPLATE(DefinedUnary);
    std::tuple<DefinedOpName, common::Indirection<Expr>> t;
  };

  struct IntrinsicBinary {
    TUPLE_CLASS_BOILERPLATE(IntrinsicBinary);
    std::tuple<common::Indirection<Expr>, common::Indirection<Expr>> t;
  };
  struct Power : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct Multiply : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct Divide : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct Add : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct Subtract : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct Concat : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct LT : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct LE : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct EQ : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct NE : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct GE : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct GT : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct AND : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct OR : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct EQV : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };
  struct NEQV : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };

  // PGI/XLF extension: (x,y), not both constant
  struct ComplexConstructor : public IntrinsicBinary {
    using IntrinsicBinary::IntrinsicBinary;
  };

  struct DefinedBinary {
    TUPLE_CLASS_BOILERPLATE(DefinedBinary);
    std::tuple<DefinedOpName, common::Indirection<Expr>,
        common::Indirection<Expr>>
        t;
  };

  explicit Expr(Designator &&);
  explicit Expr(FunctionReference &&);

  mutable TypedExpr typedExpr;

  CharBlock source;

  std::variant<common::Indirection<CharLiteralConstantSubstring>,
      LiteralConstant, common::Indirection<Designator>, ArrayConstructor,
      StructureConstructor, common::Indirection<FunctionReference>, Parentheses,
      UnaryPlus, Negate, NOT, PercentLoc, DefinedUnary, Power, Multiply, Divide,
      Add, Subtract, Concat, LT, LE, EQ, NE, GE, GT, AND, OR, EQV, NEQV,
      DefinedBinary, ComplexConstructor, common::Indirection<SubstringInquiry>>
      u;
};

// R912 part-ref -> part-name [( section-subscript-list )] [image-selector]
struct PartRef {
  BOILERPLATE(PartRef);
  PartRef(Name &&n, std::list<SectionSubscript> &&ss,
      std::optional<ImageSelector> &&is)
      : name{std::move(n)}, subscripts(std::move(ss)),
        imageSelector{std::move(is)} {}
  Name name;
  std::list<SectionSubscript> subscripts;
  std::optional<ImageSelector> imageSelector;
};

// R911 data-ref -> part-ref [% part-ref]...
struct DataRef {
  UNION_CLASS_BOILERPLATE(DataRef);
  explicit DataRef(std::list<PartRef> &&);
  std::variant<Name, common::Indirection<StructureComponent>,
      common::Indirection<ArrayElement>,
      common::Indirection<CoindexedNamedObject>>
      u;
};

// R908 substring -> parent-string ( substring-range )
// R909 parent-string ->
//        scalar-variable-name | array-element | coindexed-named-object |
//        scalar-structure-component | scalar-char-literal-constant |
//        scalar-named-constant
// Substrings of character literals have been factored out into their
// own productions so that they can't appear as designators in any context
// other than a primary expression.
struct Substring {
  TUPLE_CLASS_BOILERPLATE(Substring);
  std::tuple<DataRef, SubstringRange> t;
};

struct CharLiteralConstantSubstring {
  TUPLE_CLASS_BOILERPLATE(CharLiteralConstantSubstring);
  std::tuple<CharLiteralConstant, SubstringRange> t;
};

// substring%KIND/LEN type parameter inquiry for cases that could not be
// parsed as part-refs and fixed up afterwards.  N.B. we only have to
// handle inquiries into designator-based substrings, not those based on
// char-literal-constants.
struct SubstringInquiry {
  CharBlock source;
  WRAPPER_CLASS_BOILERPLATE(SubstringInquiry, Substring);
};

// R901 designator -> object-name | array-element | array-section |
//                    coindexed-named-object | complex-part-designator |
//                    structure-component | substring
struct Designator {
  UNION_CLASS_BOILERPLATE(Designator);
  bool EndsInBareName() const;
  CharBlock source;
  std::variant<DataRef, Substring> u;
};

// R902 variable -> designator | function-reference
struct Variable {
  UNION_CLASS_BOILERPLATE(Variable);
  mutable TypedExpr typedExpr;
  CharBlock GetSource() const;
  std::variant<common::Indirection<Designator>,
      common::Indirection<FunctionReference>>
      u;
};

// R904 logical-variable -> variable
// Appears only as part of scalar-logical-variable.
using ScalarLogicalVariable = Scalar<Logical<Variable>>;

// R906 default-char-variable -> variable
// Appears only as part of scalar-default-char-variable.
using ScalarDefaultCharVariable = Scalar<DefaultChar<Variable>>;

// R907 int-variable -> variable
// Appears only as part of scalar-int-variable.
using ScalarIntVariable = Scalar<Integer<Variable>>;

// R913 structure-component -> data-ref
struct StructureComponent {
  BOILERPLATE(StructureComponent);
  StructureComponent(DataRef &&dr, Name &&n)
      : base{std::move(dr)}, component(std::move(n)) {}
  DataRef base;
  Name component;
};

// R1039 proc-component-ref -> scalar-variable % procedure-component-name
// C1027 constrains the scalar-variable to be a data-ref without coindices.
struct ProcComponentRef {
  WRAPPER_CLASS_BOILERPLATE(ProcComponentRef, Scalar<StructureComponent>);
};

// R914 coindexed-named-object -> data-ref
struct CoindexedNamedObject {
  BOILERPLATE(CoindexedNamedObject);
  CoindexedNamedObject(DataRef &&dr, ImageSelector &&is)
      : base{std::move(dr)}, imageSelector{std::move(is)} {}
  DataRef base;
  ImageSelector imageSelector;
};

// R917 array-element -> data-ref
struct ArrayElement {
  BOILERPLATE(ArrayElement);
  ArrayElement(DataRef &&dr, std::list<SectionSubscript> &&ss)
      : base{std::move(dr)}, subscripts(std::move(ss)) {}
  Substring ConvertToSubstring();
  StructureConstructor ConvertToStructureConstructor(
      const semantics::DerivedTypeSpec &);
  DataRef base;
  std::list<SectionSubscript> subscripts;
};

// R933 allocate-object -> variable-name | structure-component
struct AllocateObject {
  UNION_CLASS_BOILERPLATE(AllocateObject);
  mutable TypedExpr typedExpr;
  std::variant<Name, StructureComponent> u;
};

// R935 lower-bound-expr -> scalar-int-expr
// R936 upper-bound-expr -> scalar-int-expr
using BoundExpr = ScalarIntExpr;

// R934 allocate-shape-spec -> [lower-bound-expr :] upper-bound-expr
// R938 allocate-coshape-spec -> [lower-bound-expr :] upper-bound-expr
struct AllocateShapeSpec {
  TUPLE_CLASS_BOILERPLATE(AllocateShapeSpec);
  std::tuple<std::optional<BoundExpr>, BoundExpr> t;
};

using AllocateCoshapeSpec = AllocateShapeSpec;

// R937 allocate-coarray-spec ->
//      [allocate-coshape-spec-list ,] [lower-bound-expr :] *
struct AllocateCoarraySpec {
  TUPLE_CLASS_BOILERPLATE(AllocateCoarraySpec);
  std::tuple<std::list<AllocateCoshapeSpec>, std::optional<BoundExpr>> t;
};

// R932 allocation ->
//        allocate-object [( allocate-shape-spec-list )]
//        [lbracket allocate-coarray-spec rbracket]
struct Allocation {
  TUPLE_CLASS_BOILERPLATE(Allocation);
  std::tuple<AllocateObject, std::list<AllocateShapeSpec>,
      std::optional<AllocateCoarraySpec>>
      t;
};

// R929 stat-variable -> scalar-int-variable
WRAPPER_CLASS(StatVariable, ScalarIntVariable);

// R930 errmsg-variable -> scalar-default-char-variable
// R1207 iomsg-variable -> scalar-default-char-variable
WRAPPER_CLASS(MsgVariable, ScalarDefaultCharVariable);

// R942 dealloc-opt -> STAT = stat-variable | ERRMSG = errmsg-variable
// R1165 sync-stat -> STAT = stat-variable | ERRMSG = errmsg-variable
struct StatOrErrmsg {
  UNION_CLASS_BOILERPLATE(StatOrErrmsg);
  std::variant<StatVariable, MsgVariable> u;
};

// R928 alloc-opt ->
//        ERRMSG = errmsg-variable | MOLD = source-expr |
//        SOURCE = source-expr | STAT = stat-variable |
// (CUDA) STREAM = scalar-int-expr
//        PINNED = scalar-logical-variable
// R931 source-expr -> expr
struct AllocOpt {
  UNION_CLASS_BOILERPLATE(AllocOpt);
  WRAPPER_CLASS(Mold, common::Indirection<Expr>);
  WRAPPER_CLASS(Source, common::Indirection<Expr>);
  WRAPPER_CLASS(Stream, common::Indirection<ScalarIntExpr>);
  WRAPPER_CLASS(Pinned, common::Indirection<ScalarLogicalVariable>);
  std::variant<Mold, Source, StatOrErrmsg, Stream, Pinned> u;
};

// R927 allocate-stmt ->
//        ALLOCATE ( [type-spec ::] allocation-list [, alloc-opt-list] )
struct AllocateStmt {
  TUPLE_CLASS_BOILERPLATE(AllocateStmt);
  std::tuple<std::optional<TypeSpec>, std::list<Allocation>,
      std::list<AllocOpt>>
      t;
};

// R940 pointer-object ->
//        variable-name | structure-component | proc-pointer-name
struct PointerObject {
  UNION_CLASS_BOILERPLATE(PointerObject);
  mutable TypedExpr typedExpr;
  std::variant<Name, StructureComponent> u;
};

// R939 nullify-stmt -> NULLIFY ( pointer-object-list )
WRAPPER_CLASS(NullifyStmt, std::list<PointerObject>);

// R941 deallocate-stmt ->
//        DEALLOCATE ( allocate-object-list [, dealloc-opt-list] )
struct DeallocateStmt {
  TUPLE_CLASS_BOILERPLATE(DeallocateStmt);
  std::tuple<std::list<AllocateObject>, std::list<StatOrErrmsg>> t;
};

// R1032 assignment-stmt -> variable = expr
struct AssignmentStmt {
  TUPLE_CLASS_BOILERPLATE(AssignmentStmt);
  using TypedAssignment =
      common::ForwardOwningPointer<evaluate::GenericAssignmentWrapper>;
  mutable TypedAssignment typedAssignment;
  std::tuple<Variable, Expr> t;
};

// R1035 bounds-spec -> lower-bound-expr :
WRAPPER_CLASS(BoundsSpec, BoundExpr);

// R1036 bounds-remapping -> lower-bound-expr : upper-bound-expr
struct BoundsRemapping {
  TUPLE_CLASS_BOILERPLATE(BoundsRemapping);
  std::tuple<BoundExpr, BoundExpr> t;
};

// R1033 pointer-assignment-stmt ->
//         data-pointer-object [( bounds-spec-list )] => data-target |
//         data-pointer-object ( bounds-remapping-list ) => data-target |
//         proc-pointer-object => proc-target
// R1034 data-pointer-object ->
//         variable-name | scalar-variable % data-pointer-component-name
// R1038 proc-pointer-object -> proc-pointer-name | proc-component-ref
struct PointerAssignmentStmt {
  struct Bounds {
    UNION_CLASS_BOILERPLATE(Bounds);
    std::variant<std::list<BoundsRemapping>, std::list<BoundsSpec>> u;
  };
  TUPLE_CLASS_BOILERPLATE(PointerAssignmentStmt);
  mutable AssignmentStmt::TypedAssignment typedAssignment;
  std::tuple<DataRef, Bounds, Expr> t;
};

// R1041 where-stmt -> WHERE ( mask-expr ) where-assignment-stmt
// R1045 where-assignment-stmt -> assignment-stmt
// R1046 mask-expr -> logical-expr
struct WhereStmt {
  TUPLE_CLASS_BOILERPLATE(WhereStmt);
  std::tuple<LogicalExpr, AssignmentStmt> t;
};

// R1043 where-construct-stmt -> [where-construct-name :] WHERE ( mask-expr )
struct WhereConstructStmt {
  TUPLE_CLASS_BOILERPLATE(WhereConstructStmt);
  std::tuple<std::optional<Name>, LogicalExpr> t;
};

// R1044 where-body-construct ->
//         where-assignment-stmt | where-stmt | where-construct
struct WhereBodyConstruct {
  UNION_CLASS_BOILERPLATE(WhereBodyConstruct);
  std::variant<Statement<AssignmentStmt>, Statement<WhereStmt>,
      common::Indirection<WhereConstruct>>
      u;
};

// R1047 masked-elsewhere-stmt ->
//         ELSEWHERE ( mask-expr ) [where-construct-name]
struct MaskedElsewhereStmt {
  TUPLE_CLASS_BOILERPLATE(MaskedElsewhereStmt);
  std::tuple<LogicalExpr, std::optional<Name>> t;
};

// R1048 elsewhere-stmt -> ELSEWHERE [where-construct-name]
WRAPPER_CLASS(ElsewhereStmt, std::optional<Name>);

// R1049 end-where-stmt -> END WHERE [where-construct-name]
WRAPPER_CLASS(EndWhereStmt, std::optional<Name>);

// R1042 where-construct ->
//         where-construct-stmt [where-body-construct]...
//         [masked-elsewhere-stmt [where-body-construct]...]...
//         [elsewhere-stmt [where-body-construct]...] end-where-stmt
struct WhereConstruct {
  struct MaskedElsewhere {
    TUPLE_CLASS_BOILERPLATE(MaskedElsewhere);
    std::tuple<Statement<MaskedElsewhereStmt>, std::list<WhereBodyConstruct>> t;
  };
  struct Elsewhere {
    TUPLE_CLASS_BOILERPLATE(Elsewhere);
    std::tuple<Statement<ElsewhereStmt>, std::list<WhereBodyConstruct>> t;
  };
  TUPLE_CLASS_BOILERPLATE(WhereConstruct);
  std::tuple<Statement<WhereConstructStmt>, std::list<WhereBodyConstruct>,
      std::list<MaskedElsewhere>, std::optional<Elsewhere>,
      Statement<EndWhereStmt>>
      t;
};

// R1051 forall-construct-stmt ->
//         [forall-construct-name :] FORALL concurrent-header
struct ForallConstructStmt {
  TUPLE_CLASS_BOILERPLATE(ForallConstructStmt);
  std::tuple<std::optional<Name>, common::Indirection<ConcurrentHeader>> t;
};

// R1053 forall-assignment-stmt -> assignment-stmt | pointer-assignment-stmt
struct ForallAssignmentStmt {
  UNION_CLASS_BOILERPLATE(ForallAssignmentStmt);
  std::variant<AssignmentStmt, PointerAssignmentStmt> u;
};

// R1055 forall-stmt -> FORALL concurrent-header forall-assignment-stmt
struct ForallStmt {
  TUPLE_CLASS_BOILERPLATE(ForallStmt);
  std::tuple<common::Indirection<ConcurrentHeader>,
      UnlabeledStatement<ForallAssignmentStmt>>
      t;
};

// R1052 forall-body-construct ->
//         forall-assignment-stmt | where-stmt | where-construct |
//         forall-construct | forall-stmt
struct ForallBodyConstruct {
  UNION_CLASS_BOILERPLATE(ForallBodyConstruct);
  std::variant<Statement<ForallAssignmentStmt>, Statement<WhereStmt>,
      WhereConstruct, common::Indirection<ForallConstruct>,
      Statement<ForallStmt>>
      u;
};

// R1054 end-forall-stmt -> END FORALL [forall-construct-name]
WRAPPER_CLASS(EndForallStmt, std::optional<Name>);

// R1050 forall-construct ->
//         forall-construct-stmt [forall-body-construct]... end-forall-stmt
struct ForallConstruct {
  TUPLE_CLASS_BOILERPLATE(ForallConstruct);
  std::tuple<Statement<ForallConstructStmt>, std::list<ForallBodyConstruct>,
      Statement<EndForallStmt>>
      t;
};

// R1105 selector -> expr | variable
struct Selector {
  UNION_CLASS_BOILERPLATE(Selector);
  std::variant<Expr, Variable> u;
};

// R1104 association -> associate-name => selector
struct Association {
  TUPLE_CLASS_BOILERPLATE(Association);
  std::tuple<Name, Selector> t;
};

// R1103 associate-stmt ->
//        [associate-construct-name :] ASSOCIATE ( association-list )
struct AssociateStmt {
  TUPLE_CLASS_BOILERPLATE(AssociateStmt);
  std::tuple<std::optional<Name>, std::list<Association>> t;
};

// R1106 end-associate-stmt -> END ASSOCIATE [associate-construct-name]
WRAPPER_CLASS(EndAssociateStmt, std::optional<Name>);

// R1102 associate-construct -> associate-stmt block end-associate-stmt
struct AssociateConstruct {
  TUPLE_CLASS_BOILERPLATE(AssociateConstruct);
  std::tuple<Statement<AssociateStmt>, Block, Statement<EndAssociateStmt>> t;
};

// R1108 block-stmt -> [block-construct-name :] BLOCK
WRAPPER_CLASS(BlockStmt, std::optional<Name>);

// R1110 end-block-stmt -> END BLOCK [block-construct-name]
WRAPPER_CLASS(EndBlockStmt, std::optional<Name>);

// R1109 block-specification-part ->
//         [use-stmt]... [import-stmt]...
//         [[declaration-construct]... specification-construct]
// N.B. Because BlockSpecificationPart just wraps the more general
// SpecificationPart, it can misrecognize an ImplicitPart as part of
// the BlockSpecificationPart during parsing, and we have to detect and
// flag such usage in semantics.
WRAPPER_CLASS(BlockSpecificationPart, SpecificationPart);

// R1107 block-construct ->
//         block-stmt [block-specification-part] block end-block-stmt
struct BlockConstruct {
  TUPLE_CLASS_BOILERPLATE(BlockConstruct);
  std::tuple<Statement<BlockStmt>, BlockSpecificationPart, Block,
      Statement<EndBlockStmt>>
      t;
};

// R1113 coarray-association -> codimension-decl => selector
struct CoarrayAssociation {
  TUPLE_CLASS_BOILERPLATE(CoarrayAssociation);
  std::tuple<CodimensionDecl, Selector> t;
};

// R1112 change-team-stmt ->
//         [team-construct-name :] CHANGE TEAM
//         ( team-value [, coarray-association-list] [, sync-stat-list] )
struct ChangeTeamStmt {
  TUPLE_CLASS_BOILERPLATE(ChangeTeamStmt);
  std::tuple<std::optional<Name>, TeamValue, std::list<CoarrayAssociation>,
      std::list<StatOrErrmsg>>
      t;
};

// R1114 end-change-team-stmt ->
//         END TEAM [( [sync-stat-list] )] [team-construct-name]
struct EndChangeTeamStmt {
  TUPLE_CLASS_BOILERPLATE(EndChangeTeamStmt);
  std::tuple<std::list<StatOrErrmsg>, std::optional<Name>> t;
};

// R1111 change-team-construct -> change-team-stmt block end-change-team-stmt
struct ChangeTeamConstruct {
  TUPLE_CLASS_BOILERPLATE(ChangeTeamConstruct);
  std::tuple<Statement<ChangeTeamStmt>, Block, Statement<EndChangeTeamStmt>> t;
};

// R1117 critical-stmt ->
//         [critical-construct-name :] CRITICAL [( [sync-stat-list] )]
struct CriticalStmt {
  TUPLE_CLASS_BOILERPLATE(CriticalStmt);
  std::tuple<std::optional<Name>, std::list<StatOrErrmsg>> t;
};

// R1118 end-critical-stmt -> END CRITICAL [critical-construct-name]
WRAPPER_CLASS(EndCriticalStmt, std::optional<Name>);

// R1116 critical-construct -> critical-stmt block end-critical-stmt
struct CriticalConstruct {
  TUPLE_CLASS_BOILERPLATE(CriticalConstruct);
  std::tuple<Statement<CriticalStmt>, Block, Statement<EndCriticalStmt>> t;
};

// R1126 concurrent-control ->
//         index-name = concurrent-limit : concurrent-limit [: concurrent-step]
// R1127 concurrent-limit -> scalar-int-expr
// R1128 concurrent-step -> scalar-int-expr
struct ConcurrentControl {
  TUPLE_CLASS_BOILERPLATE(ConcurrentControl);
  std::tuple<Name, ScalarIntExpr, ScalarIntExpr, std::optional<ScalarIntExpr>>
      t;
};

// R1125 concurrent-header ->
//         ( [integer-type-spec ::] concurrent-control-list
//         [, scalar-mask-expr] )
struct ConcurrentHeader {
  TUPLE_CLASS_BOILERPLATE(ConcurrentHeader);
  std::tuple<std::optional<IntegerTypeSpec>, std::list<ConcurrentControl>,
      std::optional<ScalarLogicalExpr>>
      t;
};

// F'2023 R1131 reduce-operation -> reduction-operator
// CUF reduction-op -> reduction-operator
// OpenACC 3.3 2.5.15 reduction-operator ->
//                      + | * | .AND. | .OR. | .EQV. | .NEQV. |
//                      MAX | MIN | IAND | IOR | IEOR
struct ReductionOperator {
  ENUM_CLASS(
      Operator, Plus, Multiply, Max, Min, Iand, Ior, Ieor, And, Or, Eqv, Neqv)
  WRAPPER_CLASS_BOILERPLATE(ReductionOperator, Operator);
  CharBlock source;
};

// R1130 locality-spec ->
//         LOCAL ( variable-name-list ) | LOCAL_INIT ( variable-name-list ) |
//         REDUCE ( reduce-operation : variable-name-list ) |
//         SHARED ( variable-name-list ) | DEFAULT ( NONE )
struct LocalitySpec {
  UNION_CLASS_BOILERPLATE(LocalitySpec);
  WRAPPER_CLASS(Local, std::list<Name>);
  WRAPPER_CLASS(LocalInit, std::list<Name>);
  struct Reduce {
    TUPLE_CLASS_BOILERPLATE(Reduce);
    using Operator = ReductionOperator;
    std::tuple<Operator, std::list<Name>> t;
  };
  WRAPPER_CLASS(Shared, std::list<Name>);
  EMPTY_CLASS(DefaultNone);
  std::variant<Local, LocalInit, Reduce, Shared, DefaultNone> u;
};

// R1123 loop-control ->
//         [,] do-variable = scalar-int-expr , scalar-int-expr
//           [, scalar-int-expr] |
//         [,] WHILE ( scalar-logical-expr ) |
//         [,] CONCURRENT concurrent-header concurrent-locality
// R1129 concurrent-locality -> [locality-spec]...
struct LoopControl {
  UNION_CLASS_BOILERPLATE(LoopControl);
  struct Concurrent {
    TUPLE_CLASS_BOILERPLATE(Concurrent);
    std::tuple<ConcurrentHeader, std::list<LocalitySpec>> t;
  };
  using Bounds = LoopBounds<ScalarName, ScalarExpr>;
  std::variant<Bounds, ScalarLogicalExpr, Concurrent> u;
};

// R1121 label-do-stmt -> [do-construct-name :] DO label [loop-control]
// A label-do-stmt with a do-construct-name is parsed as a non-label-do-stmt.
struct LabelDoStmt {
  TUPLE_CLASS_BOILERPLATE(LabelDoStmt);
  std::tuple<Label, std::optional<LoopControl>> t;
};

// R1122 nonlabel-do-stmt -> [do-construct-name :] DO [loop-control]
struct NonLabelDoStmt {
  TUPLE_CLASS_BOILERPLATE(NonLabelDoStmt);
  std::tuple<std::optional<Name>, std::optional<Label>,
      std::optional<LoopControl>>
      t;
};

// R1132 end-do-stmt -> END DO [do-construct-name]
WRAPPER_CLASS(EndDoStmt, std::optional<Name>);

// R1131 end-do -> end-do-stmt | continue-stmt

// R1119 do-construct -> do-stmt block end-do
// R1120 do-stmt -> nonlabel-do-stmt | label-do-stmt
// Deprecated, but supported: "label DO" loops ending on statements other
// than END DO and CONTINUE, and multiple "label DO" loops ending on the
// same label.
struct DoConstruct {
  TUPLE_CLASS_BOILERPLATE(DoConstruct);
  const std::optional<LoopControl> &GetLoopControl() const;
  bool IsDoNormal() const;
  bool IsDoWhile() const;
  bool IsDoConcurrent() const;
  std::tuple<Statement<NonLabelDoStmt>, Block, Statement<EndDoStmt>> t;
};

// R1133 cycle-stmt -> CYCLE [do-construct-name]
WRAPPER_CLASS(CycleStmt, std::optional<Name>);

// R1135 if-then-stmt -> [if-construct-name :] IF ( scalar-logical-expr ) THEN
struct IfThenStmt {
  TUPLE_CLASS_BOILERPLATE(IfThenStmt);
  std::tuple<std::optional<Name>, ScalarLogicalExpr> t;
};

// R1136 else-if-stmt ->
//         ELSE IF ( scalar-logical-expr ) THEN [if-construct-name]
struct ElseIfStmt {
  TUPLE_CLASS_BOILERPLATE(ElseIfStmt);
  std::tuple<ScalarLogicalExpr, std::optional<Name>> t;
};

// R1137 else-stmt -> ELSE [if-construct-name]
WRAPPER_CLASS(ElseStmt, std::optional<Name>);

// R1138 end-if-stmt -> END IF [if-construct-name]
WRAPPER_CLASS(EndIfStmt, std::optional<Name>);

// R1134 if-construct ->
//         if-then-stmt block [else-if-stmt block]...
//         [else-stmt block] end-if-stmt
struct IfConstruct {
  struct ElseIfBlock {
    TUPLE_CLASS_BOILERPLATE(ElseIfBlock);
    std::tuple<Statement<ElseIfStmt>, Block> t;
  };
  struct ElseBlock {
    TUPLE_CLASS_BOILERPLATE(ElseBlock);
    std::tuple<Statement<ElseStmt>, Block> t;
  };
  TUPLE_CLASS_BOILERPLATE(IfConstruct);
  std::tuple<Statement<IfThenStmt>, Block, std::list<ElseIfBlock>,
      std::optional<ElseBlock>, Statement<EndIfStmt>>
      t;
};

// R1139 if-stmt -> IF ( scalar-logical-expr ) action-stmt
struct IfStmt {
  TUPLE_CLASS_BOILERPLATE(IfStmt);
  std::tuple<ScalarLogicalExpr, UnlabeledStatement<ActionStmt>> t;
};

// R1141 select-case-stmt -> [case-construct-name :] SELECT CASE ( case-expr )
// R1144 case-expr -> scalar-expr
struct SelectCaseStmt {
  TUPLE_CLASS_BOILERPLATE(SelectCaseStmt);
  std::tuple<std::optional<Name>, Scalar<Expr>> t;
};

// R1147 case-value -> scalar-constant-expr
using CaseValue = Scalar<ConstantExpr>;

// R1146 case-value-range ->
//         case-value | case-value : | : case-value | case-value : case-value
struct CaseValueRange {
  UNION_CLASS_BOILERPLATE(CaseValueRange);
  struct Range {
    BOILERPLATE(Range);
    Range(std::optional<CaseValue> &&l, std::optional<CaseValue> &&u)
        : lower{std::move(l)}, upper{std::move(u)} {}
    std::optional<CaseValue> lower, upper; // not both missing
  };
  std::variant<CaseValue, Range> u;
};

// R1145 case-selector -> ( case-value-range-list ) | DEFAULT
EMPTY_CLASS(Default);

struct CaseSelector {
  UNION_CLASS_BOILERPLATE(CaseSelector);
  std::variant<std::list<CaseValueRange>, Default> u;
};

// R1142 case-stmt -> CASE case-selector [case-construct-name]
struct CaseStmt {
  TUPLE_CLASS_BOILERPLATE(CaseStmt);
  std::tuple<CaseSelector, std::optional<Name>> t;
};

// R1143 end-select-stmt -> END SELECT [case-construct-name]
// R1151 end-select-rank-stmt -> END SELECT [select-construct-name]
// R1155 end-select-type-stmt -> END SELECT [select-construct-name]
WRAPPER_CLASS(EndSelectStmt, std::optional<Name>);

// R1140 case-construct ->
//         select-case-stmt [case-stmt block]... end-select-stmt
struct CaseConstruct {
  struct Case {
    TUPLE_CLASS_BOILERPLATE(Case);
    std::tuple<Statement<CaseStmt>, Block> t;
  };
  TUPLE_CLASS_BOILERPLATE(CaseConstruct);
  std::tuple<Statement<SelectCaseStmt>, std::list<Case>,
      Statement<EndSelectStmt>>
      t;
};

// R1149 select-rank-stmt ->
//         [select-construct-name :] SELECT RANK
//         ( [associate-name =>] selector )
struct SelectRankStmt {
  TUPLE_CLASS_BOILERPLATE(SelectRankStmt);
  std::tuple<std::optional<Name>, std::optional<Name>, Selector> t;
};

// R1150 select-rank-case-stmt ->
//         RANK ( scalar-int-constant-expr ) [select-construct-name] |
//         RANK ( * ) [select-construct-name] |
//         RANK DEFAULT [select-construct-name]
struct SelectRankCaseStmt {
  struct Rank {
    UNION_CLASS_BOILERPLATE(Rank);
    std::variant<ScalarIntConstantExpr, Star, Default> u;
  };
  TUPLE_CLASS_BOILERPLATE(SelectRankCaseStmt);
  std::tuple<Rank, std::optional<Name>> t;
};

// R1148 select-rank-construct ->
//         select-rank-stmt [select-rank-case-stmt block]...
//         end-select-rank-stmt
struct SelectRankConstruct {
  TUPLE_CLASS_BOILERPLATE(SelectRankConstruct);
  struct RankCase {
    TUPLE_CLASS_BOILERPLATE(RankCase);
    std::tuple<Statement<SelectRankCaseStmt>, Block> t;
  };
  std::tuple<Statement<SelectRankStmt>, std::list<RankCase>,
      Statement<EndSelectStmt>>
      t;
};

// R1153 select-type-stmt ->
//         [select-construct-name :] SELECT TYPE
//         ( [associate-name =>] selector )
struct SelectTypeStmt {
  TUPLE_CLASS_BOILERPLATE(SelectTypeStmt);
  std::tuple<std::optional<Name>, std::optional<Name>, Selector> t;
};

// R1154 type-guard-stmt ->
//         TYPE IS ( type-spec ) [select-construct-name] |
//         CLASS IS ( derived-type-spec ) [select-construct-name] |
//         CLASS DEFAULT [select-construct-name]
struct TypeGuardStmt {
  struct Guard {
    UNION_CLASS_BOILERPLATE(Guard);
    std::variant<TypeSpec, DerivedTypeSpec, Default> u;
  };
  TUPLE_CLASS_BOILERPLATE(TypeGuardStmt);
  std::tuple<Guard, std::optional<Name>> t;
};

// R1152 select-type-construct ->
//         select-type-stmt [type-guard-stmt block]... end-select-type-stmt
struct SelectTypeConstruct {
  TUPLE_CLASS_BOILERPLATE(SelectTypeConstruct);
  struct TypeCase {
    TUPLE_CLASS_BOILERPLATE(TypeCase);
    std::tuple<Statement<TypeGuardStmt>, Block> t;
  };
  std::tuple<Statement<SelectTypeStmt>, std::list<TypeCase>,
      Statement<EndSelectStmt>>
      t;
};

// R1156 exit-stmt -> EXIT [construct-name]
WRAPPER_CLASS(ExitStmt, std::optional<Name>);

// R1157 goto-stmt -> GO TO label
WRAPPER_CLASS(GotoStmt, Label);

// R1158 computed-goto-stmt -> GO TO ( label-list ) [,] scalar-int-expr
struct ComputedGotoStmt {
  TUPLE_CLASS_BOILERPLATE(ComputedGotoStmt);
  std::tuple<std::list<Label>, ScalarIntExpr> t;
};

// R1162 stop-code -> scalar-default-char-expr | scalar-int-expr
// We can't distinguish character expressions from integer
// expressions during parsing, so we just parse an expr and
// check its type later.
WRAPPER_CLASS(StopCode, Scalar<Expr>);

// R1160 stop-stmt -> STOP [stop-code] [, QUIET = scalar-logical-expr]
// R1161 error-stop-stmt ->
//         ERROR STOP [stop-code] [, QUIET = scalar-logical-expr]
struct StopStmt {
  ENUM_CLASS(Kind, Stop, ErrorStop)
  TUPLE_CLASS_BOILERPLATE(StopStmt);
  std::tuple<Kind, std::optional<StopCode>, std::optional<ScalarLogicalExpr>> t;
};

// F2023: R1166 notify-wait-stmt -> NOTIFY WAIT ( notify-variable [,
// event-wait-spec-list] )
struct NotifyWaitStmt {
  TUPLE_CLASS_BOILERPLATE(NotifyWaitStmt);
  std::tuple<Scalar<Variable>, std::list<EventWaitSpec>> t;
};

// R1164 sync-all-stmt -> SYNC ALL [( [sync-stat-list] )]
WRAPPER_CLASS(SyncAllStmt, std::list<StatOrErrmsg>);

// R1166 sync-images-stmt -> SYNC IMAGES ( image-set [, sync-stat-list] )
// R1167 image-set -> int-expr | *
struct SyncImagesStmt {
  struct ImageSet {
    UNION_CLASS_BOILERPLATE(ImageSet);
    std::variant<IntExpr, Star> u;
  };
  TUPLE_CLASS_BOILERPLATE(SyncImagesStmt);
  std::tuple<ImageSet, std::list<StatOrErrmsg>> t;
};

// R1168 sync-memory-stmt -> SYNC MEMORY [( [sync-stat-list] )]
WRAPPER_CLASS(SyncMemoryStmt, std::list<StatOrErrmsg>);

// R1169 sync-team-stmt -> SYNC TEAM ( team-value [, sync-stat-list] )
struct SyncTeamStmt {
  TUPLE_CLASS_BOILERPLATE(SyncTeamStmt);
  std::tuple<TeamValue, std::list<StatOrErrmsg>> t;
};

// R1171 event-variable -> scalar-variable
using EventVariable = Scalar<Variable>;

// R1170 event-post-stmt -> EVENT POST ( event-variable [, sync-stat-list] )
struct EventPostStmt {
  TUPLE_CLASS_BOILERPLATE(EventPostStmt);
  std::tuple<EventVariable, std::list<StatOrErrmsg>> t;
};

// R1173 event-wait-spec -> until-spec | sync-stat
struct EventWaitSpec {
  UNION_CLASS_BOILERPLATE(EventWaitSpec);
  std::variant<ScalarIntExpr, StatOrErrmsg> u;
};

// R1172 event-wait-stmt ->
//         EVENT WAIT ( event-variable [, event-wait-spec-list] )
// R1174 until-spec -> UNTIL_COUNT = scalar-int-expr
struct EventWaitStmt {
  TUPLE_CLASS_BOILERPLATE(EventWaitStmt);
  std::tuple<EventVariable, std::list<EventWaitSpec>> t;
};

// R1177 team-variable -> scalar-variable
using TeamVariable = Scalar<Variable>;

// R1175 form-team-stmt ->
//         FORM TEAM ( team-number , team-variable [, form-team-spec-list] )
// R1176 team-number -> scalar-int-expr
// R1178 form-team-spec -> NEW_INDEX = scalar-int-expr | sync-stat
struct FormTeamStmt {
  struct FormTeamSpec {
    UNION_CLASS_BOILERPLATE(FormTeamSpec);
    std::variant<ScalarIntExpr, StatOrErrmsg> u;
  };
  TUPLE_CLASS_BOILERPLATE(FormTeamStmt);
  std::tuple<ScalarIntExpr, TeamVariable, std::list<FormTeamSpec>> t;
};

// R1182 lock-variable -> scalar-variable
using LockVariable = Scalar<Variable>;

// R1179 lock-stmt -> LOCK ( lock-variable [, lock-stat-list] )
// R1180 lock-stat -> ACQUIRED_LOCK = scalar-logical-variable | sync-stat
struct LockStmt {
  struct LockStat {
    UNION_CLASS_BOILERPLATE(LockStat);
    std::variant<Scalar<Logical<Variable>>, StatOrErrmsg> u;
  };
  TUPLE_CLASS_BOILERPLATE(LockStmt);
  std::tuple<LockVariable, std::list<LockStat>> t;
};

// R1181 unlock-stmt -> UNLOCK ( lock-variable [, sync-stat-list] )
struct UnlockStmt {
  TUPLE_CLASS_BOILERPLATE(UnlockStmt);
  std::tuple<LockVariable, std::list<StatOrErrmsg>> t;
};

// R1202 file-unit-number -> scalar-int-expr
WRAPPER_CLASS(FileUnitNumber, ScalarIntExpr);

// R1201 io-unit -> file-unit-number | * | internal-file-variable
// R1203 internal-file-variable -> char-variable
// R905 char-variable -> variable
// When Variable appears as an IoUnit, it must be character of a default,
// ASCII, or Unicode kind; this constraint is not automatically checked.
// The parse is ambiguous and is repaired if necessary once the types of
// symbols are known.
struct IoUnit {
  UNION_CLASS_BOILERPLATE(IoUnit);
  std::variant<Variable, common::Indirection<Expr>, Star> u;
};

// R1206 file-name-expr -> scalar-default-char-expr
using FileNameExpr = ScalarDefaultCharExpr;

// R1205 connect-spec ->
//         [UNIT =] file-unit-number | ACCESS = scalar-default-char-expr |
//         ACTION = scalar-default-char-expr |
//         ASYNCHRONOUS = scalar-default-char-expr |
//         BLANK = scalar-default-char-expr |
//         DECIMAL = scalar-default-char-expr |
//         DELIM = scalar-default-char-expr |
//         ENCODING = scalar-default-char-expr | ERR = label |
//         FILE = file-name-expr | FORM = scalar-default-char-expr |
//         IOMSG = iomsg-variable | IOSTAT = scalar-int-variable |
//         NEWUNIT = scalar-int-variable | PAD = scalar-default-char-expr |
//         POSITION = scalar-default-char-expr | RECL = scalar-int-expr |
//         ROUND = scalar-default-char-expr | SIGN = scalar-default-char-expr |
//         STATUS = scalar-default-char-expr
//         @ | CARRIAGECONTROL = scalar-default-char-variable
//           | CONVERT = scalar-default-char-variable
//           | DISPOSE = scalar-default-char-variable
WRAPPER_CLASS(StatusExpr, ScalarDefaultCharExpr);
WRAPPER_CLASS(ErrLabel, Label);

struct ConnectSpec {
  UNION_CLASS_BOILERPLATE(ConnectSpec);
  struct CharExpr {
    ENUM_CLASS(Kind, Access, Action, Asynchronous, Blank, Decimal, Delim,
        Encoding, Form, Pad, Position, Round, Sign,
        /* extensions: */ Carriagecontrol, Convert, Dispose)
    TUPLE_CLASS_BOILERPLATE(CharExpr);
    std::tuple<Kind, ScalarDefaultCharExpr> t;
  };
  WRAPPER_CLASS(Recl, ScalarIntExpr);
  WRAPPER_CLASS(Newunit, ScalarIntVariable);
  std::variant<FileUnitNumber, FileNameExpr, CharExpr, MsgVariable,
      StatVariable, Recl, Newunit, ErrLabel, StatusExpr>
      u;
};

// R1204 open-stmt -> OPEN ( connect-spec-list )
WRAPPER_CLASS(OpenStmt, std::list<ConnectSpec>);

// R1208 close-stmt -> CLOSE ( close-spec-list )
// R1209 close-spec ->
//         [UNIT =] file-unit-number | IOSTAT = scalar-int-variable |
//         IOMSG = iomsg-variable | ERR = label |
//         STATUS = scalar-default-char-expr
struct CloseStmt {
  struct CloseSpec {
    UNION_CLASS_BOILERPLATE(CloseSpec);
    std::variant<FileUnitNumber, StatVariable, MsgVariable, ErrLabel,
        StatusExpr>
        u;
  };
  WRAPPER_CLASS_BOILERPLATE(CloseStmt, std::list<CloseSpec>);
};

// R1215 format -> default-char-expr | label | *
// deprecated(ASSIGN): | scalar-int-name
struct Format {
  UNION_CLASS_BOILERPLATE(Format);
  std::variant<Expr, Label, Star> u;
};

// R1214 id-variable -> scalar-int-variable
WRAPPER_CLASS(IdVariable, ScalarIntVariable);

// R1213 io-control-spec ->
//         [UNIT =] io-unit | [FMT =] format | [NML =] namelist-group-name |
//         ADVANCE = scalar-default-char-expr |
//         ASYNCHRONOUS = scalar-default-char-constant-expr |
//         BLANK = scalar-default-char-expr |
//         DECIMAL = scalar-default-char-expr |
//         DELIM = scalar-default-char-expr | END = label | EOR = label |
//         ERR = label | ID = id-variable | IOMSG = iomsg-variable |
//         IOSTAT = scalar-int-variable | PAD = scalar-default-char-expr |
//         POS = scalar-int-expr | REC = scalar-int-expr |
//         ROUND = scalar-default-char-expr | SIGN = scalar-default-char-expr |
//         SIZE = scalar-int-variable
WRAPPER_CLASS(EndLabel, Label);
WRAPPER_CLASS(EorLabel, Label);
struct IoControlSpec {
  UNION_CLASS_BOILERPLATE(IoControlSpec);
  struct CharExpr {
    ENUM_CLASS(Kind, Advance, Blank, Decimal, Delim, Pad, Round, Sign)
    TUPLE_CLASS_BOILERPLATE(CharExpr);
    std::tuple<Kind, ScalarDefaultCharExpr> t;
  };
  WRAPPER_CLASS(Asynchronous, ScalarDefaultCharConstantExpr);
  WRAPPER_CLASS(Pos, ScalarIntExpr);
  WRAPPER_CLASS(Rec, ScalarIntExpr);
  WRAPPER_CLASS(Size, ScalarIntVariable);
  std::variant<IoUnit, Format, Name, CharExpr, Asynchronous, EndLabel, EorLabel,
      ErrLabel, IdVariable, MsgVariable, StatVariable, Pos, Rec, Size>
      u;
};

// R1216 input-item -> variable | io-implied-do
struct InputItem {
  UNION_CLASS_BOILERPLATE(InputItem);
  std::variant<Variable, common::Indirection<InputImpliedDo>> u;
};

// R1210 read-stmt ->
//         READ ( io-control-spec-list ) [input-item-list] |
//         READ format [, input-item-list]
struct ReadStmt {
  BOILERPLATE(ReadStmt);
  ReadStmt(std::optional<IoUnit> &&i, std::optional<Format> &&f,
      std::list<IoControlSpec> &&cs, std::list<InputItem> &&its)
      : iounit{std::move(i)}, format{std::move(f)}, controls(std::move(cs)),
        items(std::move(its)) {}
  std::optional<IoUnit> iounit; // if first in controls without UNIT= &/or
                                // followed by untagged format/namelist
  std::optional<Format> format; // if second in controls without FMT=/NML=, or
                                // no (io-control-spec-list); might be
                                // an untagged namelist group name
  std::list<IoControlSpec> controls;
  std::list<InputItem> items;
};

// R1217 output-item -> expr | io-implied-do
struct OutputItem {
  UNION_CLASS_BOILERPLATE(OutputItem);
  std::variant<Expr, common::Indirection<OutputImpliedDo>> u;
};

// R1211 write-stmt -> WRITE ( io-control-spec-list ) [output-item-list]
struct WriteStmt {
  BOILERPLATE(WriteStmt);
  WriteStmt(std::optional<IoUnit> &&i, std::optional<Format> &&f,
      std::list<IoControlSpec> &&cs, std::list<OutputItem> &&its)
      : iounit{std::move(i)}, format{std::move(f)}, controls(std::move(cs)),
        items(std::move(its)) {}
  std::optional<IoUnit> iounit; // if first in controls without UNIT= &/or
                                // followed by untagged format/namelist
  std::optional<Format> format; // if second in controls without FMT=/NML=;
                                // might be an untagged namelist group, too
  std::list<IoControlSpec> controls;
  std::list<OutputItem> items;
};

// R1212 print-stmt PRINT format [, output-item-list]
struct PrintStmt {
  TUPLE_CLASS_BOILERPLATE(PrintStmt);
  std::tuple<Format, std::list<OutputItem>> t;
};

// R1220 io-implied-do-control ->
//         do-variable = scalar-int-expr , scalar-int-expr [, scalar-int-expr]
using IoImpliedDoControl = LoopBounds<DoVariable, ScalarIntExpr>;

// R1218 io-implied-do -> ( io-implied-do-object-list , io-implied-do-control )
// R1219 io-implied-do-object -> input-item | output-item
struct InputImpliedDo {
  TUPLE_CLASS_BOILERPLATE(InputImpliedDo);
  std::tuple<std::list<InputItem>, IoImpliedDoControl> t;
};

struct OutputImpliedDo {
  TUPLE_CLASS_BOILERPLATE(OutputImpliedDo);
  std::tuple<std::list<OutputItem>, IoImpliedDoControl> t;
};

// R1223 wait-spec ->
//         [UNIT =] file-unit-number | END = label | EOR = label | ERR = label |
//         ID = scalar-int-expr | IOMSG = iomsg-variable |
//         IOSTAT = scalar-int-variable
WRAPPER_CLASS(IdExpr, ScalarIntExpr);
struct WaitSpec {
  UNION_CLASS_BOILERPLATE(WaitSpec);
  std::variant<FileUnitNumber, EndLabel, EorLabel, ErrLabel, IdExpr,
      MsgVariable, StatVariable>
      u;
};

// R1222 wait-stmt -> WAIT ( wait-spec-list )
WRAPPER_CLASS(WaitStmt, std::list<WaitSpec>);

// R1227 position-spec ->
//         [UNIT =] file-unit-number | IOMSG = iomsg-variable |
//         IOSTAT = scalar-int-variable | ERR = label
// R1229 flush-spec ->
//         [UNIT =] file-unit-number | IOSTAT = scalar-int-variable |
//         IOMSG = iomsg-variable | ERR = label
struct PositionOrFlushSpec {
  UNION_CLASS_BOILERPLATE(PositionOrFlushSpec);
  std::variant<FileUnitNumber, MsgVariable, StatVariable, ErrLabel> u;
};

// R1224 backspace-stmt ->
//         BACKSPACE file-unit-number | BACKSPACE ( position-spec-list )
WRAPPER_CLASS(BackspaceStmt, std::list<PositionOrFlushSpec>);

// R1225 endfile-stmt ->
//         ENDFILE file-unit-number | ENDFILE ( position-spec-list )
WRAPPER_CLASS(EndfileStmt, std::list<PositionOrFlushSpec>);

// R1226 rewind-stmt -> REWIND file-unit-number | REWIND ( position-spec-list )
WRAPPER_CLASS(RewindStmt, std::list<PositionOrFlushSpec>);

// R1228 flush-stmt -> FLUSH file-unit-number | FLUSH ( flush-spec-list )
WRAPPER_CLASS(FlushStmt, std::list<PositionOrFlushSpec>);

// R1231 inquire-spec ->
//         [UNIT =] file-unit-number | FILE = file-name-expr |
//         ACCESS = scalar-default-char-variable |
//         ACTION = scalar-default-char-variable |
//         ASYNCHRONOUS = scalar-default-char-variable |
//         BLANK = scalar-default-char-variable |
//         DECIMAL = scalar-default-char-variable |
//         DELIM = scalar-default-char-variable |
//         DIRECT = scalar-default-char-variable |
//         ENCODING = scalar-default-char-variable |
//         ERR = label | EXIST = scalar-logical-variable |
//         FORM = scalar-default-char-variable |
//         FORMATTED = scalar-default-char-variable |
//         ID = scalar-int-expr | IOMSG = iomsg-variable |
//         IOSTAT = scalar-int-variable |
//         NAME = scalar-default-char-variable |
//         NAMED = scalar-logical-variable |
//         NEXTREC = scalar-int-variable | NUMBER = scalar-int-variable |
//         OPENED = scalar-logical-variable |
//         PAD = scalar-default-char-variable |
//         PENDING = scalar-logical-variable | POS = scalar-int-variable |
//         POSITION = scalar-default-char-variable |
//         READ = scalar-default-char-variable |
//         READWRITE = scalar-default-char-variable |
//         RECL = scalar-int-variable | ROUND = scalar-default-char-variable |
//         SEQUENTIAL = scalar-default-char-variable |
//         SIGN = scalar-default-char-variable |
//         SIZE = scalar-int-variable |
//         STREAM = scalar-default-char-variable |
//         STATUS = scalar-default-char-variable |
//         UNFORMATTED = scalar-default-char-variable |
//         WRITE = scalar-default-char-variable
//         @ | CARRIAGECONTROL = scalar-default-char-variable
//           | CONVERT = scalar-default-char-variable
//           | DISPOSE = scalar-default-char-variable
struct InquireSpec {
  UNION_CLASS_BOILERPLATE(InquireSpec);
  struct CharVar {
    ENUM_CLASS(Kind, Access, Action, Asynchronous, Blank, Decimal, Delim,
        Direct, Encoding, Form, Formatted, Iomsg, Name, Pad, Position, Read,
        Readwrite, Round, Sequential, Sign, Stream, Status, Unformatted, Write,
        /* extensions: */ Carriagecontrol, Convert, Dispose)
    TUPLE_CLASS_BOILERPLATE(CharVar);
    std::tuple<Kind, ScalarDefaultCharVariable> t;
  };
  struct IntVar {
    ENUM_CLASS(Kind, Iostat, Nextrec, Number, Pos, Recl, Size)
    TUPLE_CLASS_BOILERPLATE(IntVar);
    std::tuple<Kind, ScalarIntVariable> t;
  };
  struct LogVar {
    ENUM_CLASS(Kind, Exist, Named, Opened, Pending)
    TUPLE_CLASS_BOILERPLATE(LogVar);
    std::tuple<Kind, Scalar<Logical<Variable>>> t;
  };
  std::variant<FileUnitNumber, FileNameExpr, CharVar, IntVar, LogVar, IdExpr,
      ErrLabel>
      u;
};

// R1230 inquire-stmt ->
//         INQUIRE ( inquire-spec-list ) |
//         INQUIRE ( IOLENGTH = scalar-int-variable ) output-item-list
struct InquireStmt {
  UNION_CLASS_BOILERPLATE(InquireStmt);
  struct Iolength {
    TUPLE_CLASS_BOILERPLATE(Iolength);
    std::tuple<ScalarIntVariable, std::list<OutputItem>> t;
  };
  std::variant<std::list<InquireSpec>, Iolength> u;
};

// R1301 format-stmt -> FORMAT format-specification
WRAPPER_CLASS(FormatStmt, format::FormatSpecification);

// R1402 program-stmt -> PROGRAM program-name
WRAPPER_CLASS(ProgramStmt, Name);

// R1403 end-program-stmt -> END [PROGRAM [program-name]]
WRAPPER_CLASS(EndProgramStmt, std::optional<Name>);

// R1401 main-program ->
//         [program-stmt] [specification-part] [execution-part]
//         [internal-subprogram-part] end-program-stmt
struct MainProgram {
  TUPLE_CLASS_BOILERPLATE(MainProgram);
  std::tuple<std::optional<Statement<ProgramStmt>>, SpecificationPart,
      ExecutionPart, std::optional<InternalSubprogramPart>,
      Statement<EndProgramStmt>>
      t;
};

// R1405 module-stmt -> MODULE module-name
WRAPPER_CLASS(ModuleStmt, Name);

// R1408 module-subprogram ->
//         function-subprogram | subroutine-subprogram |
//         separate-module-subprogram
struct ModuleSubprogram {
  UNION_CLASS_BOILERPLATE(ModuleSubprogram);
  std::variant<common::Indirection<FunctionSubprogram>,
      common::Indirection<SubroutineSubprogram>,
      common::Indirection<SeparateModuleSubprogram>,
      common::Indirection<CompilerDirective>>
      u;
};

// R1407 module-subprogram-part -> contains-stmt [module-subprogram]...
struct ModuleSubprogramPart {
  TUPLE_CLASS_BOILERPLATE(ModuleSubprogramPart);
  std::tuple<Statement<ContainsStmt>, std::list<ModuleSubprogram>> t;
};

// R1406 end-module-stmt -> END [MODULE [module-name]]
WRAPPER_CLASS(EndModuleStmt, std::optional<Name>);

// R1404 module ->
//         module-stmt [specification-part] [module-subprogram-part]
//         end-module-stmt
struct Module {
  TUPLE_CLASS_BOILERPLATE(Module);
  std::tuple<Statement<ModuleStmt>, SpecificationPart,
      std::optional<ModuleSubprogramPart>, Statement<EndModuleStmt>>
      t;
};

// R1411 rename ->
//         local-name => use-name |
//         OPERATOR ( local-defined-operator ) =>
//           OPERATOR ( use-defined-operator )
struct Rename {
  UNION_CLASS_BOILERPLATE(Rename);
  struct Names {
    TUPLE_CLASS_BOILERPLATE(Names);
    std::tuple<Name, Name> t;
  };
  struct Operators {
    TUPLE_CLASS_BOILERPLATE(Operators);
    std::tuple<DefinedOpName, DefinedOpName> t;
  };
  std::variant<Names, Operators> u;
};

// R1418 parent-identifier -> ancestor-module-name [: parent-submodule-name]
struct ParentIdentifier {
  TUPLE_CLASS_BOILERPLATE(ParentIdentifier);
  std::tuple<Name, std::optional<Name>> t;
};

// R1417 submodule-stmt -> SUBMODULE ( parent-identifier ) submodule-name
struct SubmoduleStmt {
  TUPLE_CLASS_BOILERPLATE(SubmoduleStmt);
  std::tuple<ParentIdentifier, Name> t;
};

// R1419 end-submodule-stmt -> END [SUBMODULE [submodule-name]]
WRAPPER_CLASS(EndSubmoduleStmt, std::optional<Name>);

// R1416 submodule ->
//         submodule-stmt [specification-part] [module-subprogram-part]
//         end-submodule-stmt
struct Submodule {
  TUPLE_CLASS_BOILERPLATE(Submodule);
  std::tuple<Statement<SubmoduleStmt>, SpecificationPart,
      std::optional<ModuleSubprogramPart>, Statement<EndSubmoduleStmt>>
      t;
};

// R1421 block-data-stmt -> BLOCK DATA [block-data-name]
WRAPPER_CLASS(BlockDataStmt, std::optional<Name>);

// R1422 end-block-data-stmt -> END [BLOCK DATA [block-data-name]]
WRAPPER_CLASS(EndBlockDataStmt, std::optional<Name>);

// R1420 block-data -> block-data-stmt [specification-part] end-block-data-stmt
struct BlockData {
  TUPLE_CLASS_BOILERPLATE(BlockData);
  std::tuple<Statement<BlockDataStmt>, SpecificationPart,
      Statement<EndBlockDataStmt>>
      t;
};

// R1508 generic-spec ->
//         generic-name | OPERATOR ( defined-operator ) |
//         ASSIGNMENT ( = ) | defined-io-generic-spec
// R1509 defined-io-generic-spec ->
//         READ ( FORMATTED ) | READ ( UNFORMATTED ) |
//         WRITE ( FORMATTED ) | WRITE ( UNFORMATTED )
struct GenericSpec {
  UNION_CLASS_BOILERPLATE(GenericSpec);
  EMPTY_CLASS(Assignment);
  EMPTY_CLASS(ReadFormatted);
  EMPTY_CLASS(ReadUnformatted);
  EMPTY_CLASS(WriteFormatted);
  EMPTY_CLASS(WriteUnformatted);
  CharBlock source;
  std::variant<Name, DefinedOperator, Assignment, ReadFormatted,
      ReadUnformatted, WriteFormatted, WriteUnformatted>
      u;
};

// R1510 generic-stmt ->
//         GENERIC [, access-spec] :: generic-spec => specific-procedure-list
struct GenericStmt {
  TUPLE_CLASS_BOILERPLATE(GenericStmt);
  std::tuple<std::optional<AccessSpec>, GenericSpec, std::list<Name>> t;
};

// R1503 interface-stmt -> INTERFACE [generic-spec] | ABSTRACT INTERFACE
struct InterfaceStmt {
  UNION_CLASS_BOILERPLATE(InterfaceStmt);
  // Workaround for clang with libstc++10 bug
  InterfaceStmt(Abstract x) : u{x} {}

  std::variant<std::optional<GenericSpec>, Abstract> u;
};

// R1412 only -> generic-spec | only-use-name | rename
// R1413 only-use-name -> use-name
struct Only {
  UNION_CLASS_BOILERPLATE(Only);
  std::variant<common::Indirection<GenericSpec>, Name, Rename> u;
};

// R1409 use-stmt ->
//         USE [[, module-nature] ::] module-name [, rename-list] |
//         USE [[, module-nature] ::] module-name , ONLY : [only-list]
// R1410 module-nature -> INTRINSIC | NON_INTRINSIC
struct UseStmt {
  BOILERPLATE(UseStmt);
  ENUM_CLASS(ModuleNature, Intrinsic, Non_Intrinsic) // R1410
  template <typename A>
  UseStmt(std::optional<ModuleNature> &&nat, Name &&n, std::list<A> &&x)
      : nature(std::move(nat)), moduleName(std::move(n)), u(std::move(x)) {}
  std::optional<ModuleNature> nature;
  Name moduleName;
  std::variant<std::list<Rename>, std::list<Only>> u;
};

// R1514 proc-attr-spec ->
//         access-spec | proc-language-binding-spec | INTENT ( intent-spec ) |
//         OPTIONAL | POINTER | PROTECTED | SAVE
struct ProcAttrSpec {
  UNION_CLASS_BOILERPLATE(ProcAttrSpec);
  std::variant<AccessSpec, LanguageBindingSpec, IntentSpec, Optional, Pointer,
      Protected, Save>
      u;
};

// R1512 procedure-declaration-stmt ->
//         PROCEDURE ( [proc-interface] ) [[, proc-attr-spec]... ::]
//         proc-decl-list
struct ProcedureDeclarationStmt {
  TUPLE_CLASS_BOILERPLATE(ProcedureDeclarationStmt);
  std::tuple<std::optional<ProcInterface>, std::list<ProcAttrSpec>,
      std::list<ProcDecl>>
      t;
};

// R1527 prefix-spec ->
//         declaration-type-spec | ELEMENTAL | IMPURE | MODULE |
//         NON_RECURSIVE | PURE | RECURSIVE |
// (CUDA)  ATTRIBUTES ( (DEVICE | GLOBAL | GRID_GLOBAL | HOST)... )
//         LAUNCH_BOUNDS(expr-list) | CLUSTER_DIMS(expr-list)
struct PrefixSpec {
  UNION_CLASS_BOILERPLATE(PrefixSpec);
  EMPTY_CLASS(Elemental);
  EMPTY_CLASS(Impure);
  EMPTY_CLASS(Module);
  EMPTY_CLASS(Non_Recursive);
  EMPTY_CLASS(Pure);
  EMPTY_CLASS(Recursive);
  WRAPPER_CLASS(Attributes, std::list<common::CUDASubprogramAttrs>);
  WRAPPER_CLASS(Launch_Bounds, std::list<ScalarIntConstantExpr>);
  WRAPPER_CLASS(Cluster_Dims, std::list<ScalarIntConstantExpr>);
  std::variant<DeclarationTypeSpec, Elemental, Impure, Module, Non_Recursive,
      Pure, Recursive, Attributes, Launch_Bounds, Cluster_Dims>
      u;
};

// R1532 suffix ->
//         proc-language-binding-spec [RESULT ( result-name )] |
//         RESULT ( result-name ) [proc-language-binding-spec]
struct Suffix {
  BOILERPLATE(Suffix);
  Suffix(LanguageBindingSpec &&lbs, std::optional<Name> &&rn)
      : binding(std::move(lbs)), resultName(std::move(rn)) {}
  Suffix(Name &&rn, std::optional<LanguageBindingSpec> &&lbs)
      : binding(std::move(lbs)), resultName(std::move(rn)) {}
  std::optional<LanguageBindingSpec> binding;
  std::optional<Name> resultName;
};

// R1530 function-stmt ->
//         [prefix] FUNCTION function-name ( [dummy-arg-name-list] ) [suffix]
// R1526 prefix -> prefix-spec [prefix-spec]...
// R1531 dummy-arg-name -> name
struct FunctionStmt {
  TUPLE_CLASS_BOILERPLATE(FunctionStmt);
  std::tuple<std::list<PrefixSpec>, Name, std::list<Name>,
      std::optional<Suffix>>
      t;
};

// R1533 end-function-stmt -> END [FUNCTION [function-name]]
WRAPPER_CLASS(EndFunctionStmt, std::optional<Name>);

// R1536 dummy-arg -> dummy-arg-name | *
struct DummyArg {
  UNION_CLASS_BOILERPLATE(DummyArg);
  std::variant<Name, Star> u;
};

// R1535 subroutine-stmt ->
//         [prefix] SUBROUTINE subroutine-name [( [dummy-arg-list] )
//         [proc-language-binding-spec]]
struct SubroutineStmt {
  TUPLE_CLASS_BOILERPLATE(SubroutineStmt);
  std::tuple<std::list<PrefixSpec>, Name, std::list<DummyArg>,
      std::optional<LanguageBindingSpec>>
      t;
};

// R1537 end-subroutine-stmt -> END [SUBROUTINE [subroutine-name]]
WRAPPER_CLASS(EndSubroutineStmt, std::optional<Name>);

// R1505 interface-body ->
//         function-stmt [specification-part] end-function-stmt |
//         subroutine-stmt [specification-part] end-subroutine-stmt
struct InterfaceBody {
  UNION_CLASS_BOILERPLATE(InterfaceBody);
  struct Function {
    TUPLE_CLASS_BOILERPLATE(Function);
    std::tuple<Statement<FunctionStmt>, common::Indirection<SpecificationPart>,
        Statement<EndFunctionStmt>>
        t;
  };
  struct Subroutine {
    TUPLE_CLASS_BOILERPLATE(Subroutine);
    std::tuple<Statement<SubroutineStmt>,
        common::Indirection<SpecificationPart>, Statement<EndSubroutineStmt>>
        t;
  };
  std::variant<Function, Subroutine> u;
};

// R1506 procedure-stmt -> [MODULE] PROCEDURE [::] specific-procedure-list
struct ProcedureStmt {
  ENUM_CLASS(Kind, ModuleProcedure, Procedure)
  TUPLE_CLASS_BOILERPLATE(ProcedureStmt);
  std::tuple<Kind, std::list<Name>> t;
};

// R1502 interface-specification -> interface-body | procedure-stmt
struct InterfaceSpecification {
  UNION_CLASS_BOILERPLATE(InterfaceSpecification);
  std::variant<InterfaceBody, Statement<ProcedureStmt>> u;
};

// R1504 end-interface-stmt -> END INTERFACE [generic-spec]
WRAPPER_CLASS(EndInterfaceStmt, std::optional<GenericSpec>);

// R1501 interface-block ->
//         interface-stmt [interface-specification]... end-interface-stmt
struct InterfaceBlock {
  TUPLE_CLASS_BOILERPLATE(InterfaceBlock);
  std::tuple<Statement<InterfaceStmt>, std::list<InterfaceSpecification>,
      Statement<EndInterfaceStmt>>
      t;
};

// R1511 external-stmt -> EXTERNAL [::] external-name-list
WRAPPER_CLASS(ExternalStmt, std::list<Name>);

// R1519 intrinsic-stmt -> INTRINSIC [::] intrinsic-procedure-name-list
WRAPPER_CLASS(IntrinsicStmt, std::list<Name>);

// R1522 procedure-designator ->
//         procedure-name | proc-component-ref | data-ref % binding-name
struct ProcedureDesignator {
  UNION_CLASS_BOILERPLATE(ProcedureDesignator);
  std::variant<Name, ProcComponentRef> u;
};

// R1525 alt-return-spec -> * label
WRAPPER_CLASS(AltReturnSpec, Label);

// R1524 actual-arg ->
//         expr | variable | procedure-name | proc-component-ref |
//         alt-return-spec
struct ActualArg {
  WRAPPER_CLASS(PercentRef, Expr); // %REF(x) extension
  WRAPPER_CLASS(PercentVal, Expr); // %VAL(x) extension
  UNION_CLASS_BOILERPLATE(ActualArg);
  ActualArg(Expr &&x) : u{common::Indirection<Expr>(std::move(x))} {}
  std::variant<common::Indirection<Expr>, AltReturnSpec, PercentRef, PercentVal>
      u;
};

// R1523 actual-arg-spec -> [keyword =] actual-arg
struct ActualArgSpec {
  TUPLE_CLASS_BOILERPLATE(ActualArgSpec);
  std::tuple<std::optional<Keyword>, ActualArg> t;
};

// R1520 function-reference -> procedure-designator
//         ( [actual-arg-spec-list] )
struct Call {
  TUPLE_CLASS_BOILERPLATE(Call);
  std::tuple<ProcedureDesignator, std::list<ActualArgSpec>> t;
};

struct FunctionReference {
  WRAPPER_CLASS_BOILERPLATE(FunctionReference, Call);
  CharBlock source;
  Designator ConvertToArrayElementRef();
  StructureConstructor ConvertToStructureConstructor(
      const semantics::DerivedTypeSpec &);
};

// R1521 call-stmt -> CALL procedure-designator [ chevrons ]
//         [( [actual-arg-spec-list] )]
// (CUDA) chevrons -> <<< * | scalar-expr, scalar-expr [,
//          scalar-int-expr [, scalar-int-expr ] ] >>>
struct CallStmt {
  BOILERPLATE(CallStmt);
  WRAPPER_CLASS(StarOrExpr, std::optional<ScalarExpr>);
  struct Chevrons {
    TUPLE_CLASS_BOILERPLATE(Chevrons);
    std::tuple<StarOrExpr, ScalarExpr, std::optional<ScalarIntExpr>,
        std::optional<ScalarIntExpr>>
        t;
  };
  explicit CallStmt(ProcedureDesignator &&pd, std::optional<Chevrons> &&ch,
      std::list<ActualArgSpec> &&args)
      : call{std::move(pd), std::move(args)}, chevrons{std::move(ch)} {}
  Call call;
  std::optional<Chevrons> chevrons;
  CharBlock source;
  mutable common::ForwardOwningPointer<evaluate::ProcedureRef>
      typedCall; // filled by semantics
};

// R1529 function-subprogram ->
//         function-stmt [specification-part] [execution-part]
//         [internal-subprogram-part] end-function-stmt
struct FunctionSubprogram {
  TUPLE_CLASS_BOILERPLATE(FunctionSubprogram);
  std::tuple<Statement<FunctionStmt>, SpecificationPart, ExecutionPart,
      std::optional<InternalSubprogramPart>, Statement<EndFunctionStmt>>
      t;
};

// R1534 subroutine-subprogram ->
//         subroutine-stmt [specification-part] [execution-part]
//         [internal-subprogram-part] end-subroutine-stmt
struct SubroutineSubprogram {
  TUPLE_CLASS_BOILERPLATE(SubroutineSubprogram);
  std::tuple<Statement<SubroutineStmt>, SpecificationPart, ExecutionPart,
      std::optional<InternalSubprogramPart>, Statement<EndSubroutineStmt>>
      t;
};

// R1539 mp-subprogram-stmt -> MODULE PROCEDURE procedure-name
WRAPPER_CLASS(MpSubprogramStmt, Name);

// R1540 end-mp-subprogram-stmt -> END [PROCEDURE [procedure-name]]
WRAPPER_CLASS(EndMpSubprogramStmt, std::optional<Name>);

// R1538 separate-module-subprogram ->
//         mp-subprogram-stmt [specification-part] [execution-part]
//         [internal-subprogram-part] end-mp-subprogram-stmt
struct SeparateModuleSubprogram {
  TUPLE_CLASS_BOILERPLATE(SeparateModuleSubprogram);
  std::tuple<Statement<MpSubprogramStmt>, SpecificationPart, ExecutionPart,
      std::optional<InternalSubprogramPart>, Statement<EndMpSubprogramStmt>>
      t;
};

// R1541 entry-stmt -> ENTRY entry-name [( [dummy-arg-list] ) [suffix]]
struct EntryStmt {
  TUPLE_CLASS_BOILERPLATE(EntryStmt);
  std::tuple<Name, std::list<DummyArg>, std::optional<Suffix>> t;
};

// R1542 return-stmt -> RETURN [scalar-int-expr]
WRAPPER_CLASS(ReturnStmt, std::optional<ScalarIntExpr>);

// R1544 stmt-function-stmt ->
//         function-name ( [dummy-arg-name-list] ) = scalar-expr
struct StmtFunctionStmt {
  TUPLE_CLASS_BOILERPLATE(StmtFunctionStmt);
  std::tuple<Name, std::list<Name>, Scalar<Expr>> t;
  Statement<ActionStmt> ConvertToAssignment();
};

// Compiler directives
// !DIR$ IGNORE_TKR [ [(tkrdmac...)] name ]...
// !DIR$ LOOP COUNT (n1[, n2]...)
// !DIR$ name[=value] [, name[=value]]...    = can be :
// !DIR$ UNROLL [N]
// !DIR$ UNROLL_AND_JAM [N]
// !DIR$ NOVECTOR
// !DIR$ NOUNROLL
// !DIR$ NOUNROLL_AND_JAM
// !DIR$ <anything else>
struct CompilerDirective {
  UNION_CLASS_BOILERPLATE(CompilerDirective);
  struct IgnoreTKR {
    TUPLE_CLASS_BOILERPLATE(IgnoreTKR);
    std::tuple<std::optional<std::list<const char *>>, Name> t;
  };
  struct LoopCount {
    WRAPPER_CLASS_BOILERPLATE(LoopCount, std::list<std::uint64_t>);
  };
  struct AssumeAligned {
    TUPLE_CLASS_BOILERPLATE(AssumeAligned);
    std::tuple<common::Indirection<Designator>, uint64_t> t;
  };
  EMPTY_CLASS(VectorAlways);
  struct NameValue {
    TUPLE_CLASS_BOILERPLATE(NameValue);
    std::tuple<Name, std::optional<std::uint64_t>> t;
  };
  struct Unroll {
    WRAPPER_CLASS_BOILERPLATE(Unroll, std::optional<std::uint64_t>);
  };
  struct UnrollAndJam {
    WRAPPER_CLASS_BOILERPLATE(UnrollAndJam, std::optional<std::uint64_t>);
  };
  EMPTY_CLASS(NoVector);
  EMPTY_CLASS(NoUnroll);
  EMPTY_CLASS(NoUnrollAndJam);
  EMPTY_CLASS(Unrecognized);
  CharBlock source;
  std::variant<std::list<IgnoreTKR>, LoopCount, std::list<AssumeAligned>,
      VectorAlways, std::list<NameValue>, Unroll, UnrollAndJam, Unrecognized,
      NoVector, NoUnroll, NoUnrollAndJam>
      u;
};

// (CUDA) ATTRIBUTE(attribute) [::] name-list
struct CUDAAttributesStmt {
  TUPLE_CLASS_BOILERPLATE(CUDAAttributesStmt);
  std::tuple<common::CUDADataAttr, std::list<Name>> t;
};

// Legacy extensions
struct BasedPointer {
  TUPLE_CLASS_BOILERPLATE(BasedPointer);
  std::tuple<ObjectName, ObjectName, std::optional<ArraySpec>> t;
};
WRAPPER_CLASS(BasedPointerStmt, std::list<BasedPointer>);

struct Union;
struct StructureDef;

struct StructureField {
  UNION_CLASS_BOILERPLATE(StructureField);
  std::variant<Statement<DataComponentDefStmt>,
      common::Indirection<StructureDef>, common::Indirection<Union>>
      u;
};

struct Map {
  EMPTY_CLASS(MapStmt);
  EMPTY_CLASS(EndMapStmt);
  TUPLE_CLASS_BOILERPLATE(Map);
  std::tuple<Statement<MapStmt>, std::list<StructureField>,
      Statement<EndMapStmt>>
      t;
};

struct Union {
  EMPTY_CLASS(UnionStmt);
  EMPTY_CLASS(EndUnionStmt);
  TUPLE_CLASS_BOILERPLATE(Union);
  std::tuple<Statement<UnionStmt>, std::list<Map>, Statement<EndUnionStmt>> t;
};

struct StructureStmt {
  TUPLE_CLASS_BOILERPLATE(StructureStmt);
  std::tuple<std::optional<Name>, std::list<EntityDecl>> t;
};

struct StructureDef {
  EMPTY_CLASS(EndStructureStmt);
  TUPLE_CLASS_BOILERPLATE(StructureDef);
  std::tuple<Statement<StructureStmt>, std::list<StructureField>,
      Statement<EndStructureStmt>>
      t;
};

// Old style PARAMETER statement without parentheses.
// Types are determined entirely from the right-hand sides, not the names.
WRAPPER_CLASS(OldParameterStmt, std::list<NamedConstantDef>);

// Deprecations
struct ArithmeticIfStmt {
  TUPLE_CLASS_BOILERPLATE(ArithmeticIfStmt);
  std::tuple<Expr, Label, Label, Label> t;
};

struct AssignStmt {
  TUPLE_CLASS_BOILERPLATE(AssignStmt);
  std::tuple<Label, Name> t;
};

struct AssignedGotoStmt {
  TUPLE_CLASS_BOILERPLATE(AssignedGotoStmt);
  std::tuple<Name, std::list<Label>> t;
};

WRAPPER_CLASS(PauseStmt, std::optional<StopCode>);

// Parse tree nodes for OpenMP directives and clauses

// --- Common definitions

#define INHERITED_WRAPPER_CLASS_BOILERPLATE(classname, basename) \
  BOILERPLATE(classname); \
  using basename::basename; \
  classname(basename &&base) : basename(std::move(base)) {} \
  using WrapperTrait = std::true_type

struct OmpClause;
struct OmpDirectiveSpecification;

struct OmpDirectiveName {
  // No boilerplates: this class should be copyable, movable, etc.
  constexpr OmpDirectiveName() = default;
  constexpr OmpDirectiveName(const OmpDirectiveName &) = default;
  constexpr OmpDirectiveName(llvm::omp::Directive x) : v(x) {}
  // Construct from an already parsed text. Use Verbatim for this because
  // Verbatim's source corresponds to an actual source location.
  // This allows "construct<OmpDirectiveName>(Verbatim("<name>"))".
  OmpDirectiveName(const Verbatim &name);
  using WrapperTrait = std::true_type;

  bool IsExecutionPart() const; // Is allowed in the execution part

  CharBlock source;
  llvm::omp::Directive v{llvm::omp::Directive::OMPD_unknown};
};

// 2.1 Directives or clauses may accept a list or extended-list.
//     A list item is a variable, array section or common block name (enclosed
//     in slashes). An extended list item is a list item or a procedure Name.
// variable-name | / common-block / | array-sections
struct OmpObject {
  UNION_CLASS_BOILERPLATE(OmpObject);
  std::variant<Designator, /*common block*/ Name> u;
};

WRAPPER_CLASS(OmpObjectList, std::list<OmpObject>);

// Ref: [4.5:201-207], [5.0:293-299], [5.1:325-331], [5.2:124]
//
// reduction-identifier ->
//    base-language-identifier |                    // since 4.5
//    - |                                           // since 4.5, until 5.2
//    + | * | .AND. | .OR. | .EQV. | .NEQV. |       // since 4.5
//    MIN | MAX | IAND | IOR | IEOR                 // since 4.5
struct OmpReductionIdentifier {
  UNION_CLASS_BOILERPLATE(OmpReductionIdentifier);
  std::variant<DefinedOperator, ProcedureDesignator> u;
};

// Ref: [4.5:222:6], [5.0:305:27], [5.1:337:19], [5.2:126:3-4], [6.0:240:27-28]
//
// combiner-expression ->                           // since 4.5
//    assignment-statement |
//    function-reference
struct OmpReductionCombiner {
  UNION_CLASS_BOILERPLATE(OmpReductionCombiner);
  std::variant<AssignmentStmt, FunctionReference> u;
};

inline namespace arguments {
struct OmpTypeSpecifier {
  UNION_CLASS_BOILERPLATE(OmpTypeSpecifier);
  std::variant<TypeSpec, DeclarationTypeSpec> u;
};

WRAPPER_CLASS(OmpTypeNameList, std::list<OmpTypeSpecifier>);

struct OmpLocator {
  UNION_CLASS_BOILERPLATE(OmpLocator);
  std::variant<OmpObject, FunctionReference> u;
};

WRAPPER_CLASS(OmpLocatorList, std::list<OmpLocator>);

// Ref: [5.0:326:10-16], [5.1:359:5-11], [5.2:163:2-7], [6.0:293:16-21]
//
// mapper-specifier ->
//    [mapper-identifier :] type :: var |           // since 5.0
//    DEFAULT type :: var
struct OmpMapperSpecifier {
  // Absent mapper-identifier is equivalent to DEFAULT.
  TUPLE_CLASS_BOILERPLATE(OmpMapperSpecifier);
  std::tuple<std::string, TypeSpec, Name> t;
};

// Ref: [4.5:222:1-5], [5.0:305:20-27], [5.1:337:11-19], [5.2:139:18-23],
// [6.0:260:16-20]
//
// reduction-specifier ->
//    reduction-identifier : typename-list
//        : combiner-expression                     // since 4.5, until 5.2
//    reduction-identifier : typename-list          // since 6.0
struct OmpReductionSpecifier {
  TUPLE_CLASS_BOILERPLATE(OmpReductionSpecifier);
  std::tuple<OmpReductionIdentifier, OmpTypeNameList,
      std::optional<OmpReductionCombiner>>
      t;
};

struct OmpArgument {
  CharBlock source;
  UNION_CLASS_BOILERPLATE(OmpArgument);
  std::variant<OmpLocator, // {variable, extended, locator}-list-item
      OmpMapperSpecifier, OmpReductionSpecifier>
      u;
};

struct OmpArgumentList {
  WRAPPER_CLASS_BOILERPLATE(OmpArgumentList, std::list<OmpArgument>);
  CharBlock source;
};
} // namespace arguments

inline namespace traits {
// trait-property-name ->
//    identifier | string-literal
//
// This is a bit of a problematic case. The spec says that a word in quotes,
// and the same word without quotes are equivalent. We currently parse both
// as a string, but it's likely just a temporary solution.
//
// The problem is that trait-property can be (among other things) a
// trait-property-name or a trait-property-expression. A simple identifier
// can be either, there is no reasonably simple way of telling them apart
// in the parser. There is a similar issue with extensions. Some of that
// disambiguation may need to be done in the "canonicalization" pass and
// then some of those AST nodes would be rewritten into different ones.
//
struct OmpTraitPropertyName {
  CharBlock source;
  WRAPPER_CLASS_BOILERPLATE(OmpTraitPropertyName, std::string);
};

// trait-score ->
//    SCORE(non-negative-const-integer-expression)
struct OmpTraitScore {
  CharBlock source;
  WRAPPER_CLASS_BOILERPLATE(OmpTraitScore, ScalarIntExpr);
};

// trait-property-extension ->
//    trait-property-name |
//    scalar-expr |
//    trait-property-name (trait-property-extension, ...)
//
struct OmpTraitPropertyExtension {
  CharBlock source;
  UNION_CLASS_BOILERPLATE(OmpTraitPropertyExtension);
  struct Complex { // name (prop-ext, prop-ext, ...)
    CharBlock source;
    TUPLE_CLASS_BOILERPLATE(Complex);
    std::tuple<OmpTraitPropertyName,
        std::list<common::Indirection<OmpTraitPropertyExtension>>>
        t;
  };

  std::variant<OmpTraitPropertyName, ScalarExpr, Complex> u;
};

// trait-property ->
//    trait-property-name | OmpClause |
//    trait-property-expression | trait-property-extension
// trait-property-expression ->
//    scalar-logical-expression | scalar-integer-expression
//
// The parser for a logical expression will accept an integer expression,
// and if it's not logical, it will flag an error later. The same thing
// will happen if the scalar integer expression sees a logical expresion.
// To avoid this, parse all expressions as scalar expressions.
struct OmpTraitProperty {
  CharBlock source;
  UNION_CLASS_BOILERPLATE(OmpTraitProperty);
  std::variant<OmpTraitPropertyName, common::Indirection<OmpClause>,
      ScalarExpr, // trait-property-expresion
      OmpTraitPropertyExtension>
      u;
};

// trait-selector-name ->
//    KIND |              DT       // name-list (host, nohost, +/add-def-doc)
//    ISA |               DT       // name-list (isa_name, ... /impl-defined)
//    ARCH |              DT       // name-list (arch_name, ... /impl-defined)
//    directive-name |    C        // no properties
//    SIMD |              C        // clause-list (from declare_simd)
//                                 // (at least simdlen, inbranch/notinbranch)
//    DEVICE_NUM |        T        // device-number
//    UID |               T        // unique-string-id /impl-defined
//    VENDOR |            I        // name-list (vendor-id /add-def-doc)
//    EXTENSION |         I        // name-list (ext_name /impl-defined)
//    ATOMIC_DEFAULT_MEM_ORDER I | // clause-list (value of admo)
//    REQUIRES |          I        // clause-list (from requires)
//    CONDITION           U        // logical-expr
//    <other name>        I        // treated as extension
//
// Trait-set-selectors:
//    [D]evice, [T]arget_device, [C]onstruct, [I]mplementation, [U]ser.
struct OmpTraitSelectorName {
  std::string ToString() const;
  CharBlock source;
  UNION_CLASS_BOILERPLATE(OmpTraitSelectorName);
  ENUM_CLASS(Value, Arch, Atomic_Default_Mem_Order, Condition, Device_Num,
      Extension, Isa, Kind, Requires, Simd, Uid, Vendor)
  std::variant<Value, llvm::omp::Directive, std::string> u;
};

// trait-selector ->
//    trait-selector-name |
//    trait-selector-name ([trait-score:] trait-property, ...)
struct OmpTraitSelector {
  CharBlock source;
  TUPLE_CLASS_BOILERPLATE(OmpTraitSelector);
  struct Properties {
    TUPLE_CLASS_BOILERPLATE(Properties);
    std::tuple<std::optional<OmpTraitScore>, std::list<OmpTraitProperty>> t;
  };
  std::tuple<OmpTraitSelectorName, std::optional<Properties>> t;
};

// trait-set-selector-name ->
//    CONSTRUCT | DEVICE | IMPLEMENTATION | USER |  // since 5.0
//    TARGET_DEVICE                                 // since 5.1
struct OmpTraitSetSelectorName {
  std::string ToString() const;
  CharBlock source;
  ENUM_CLASS(Value, Construct, Device, Implementation, Target_Device, User)
  WRAPPER_CLASS_BOILERPLATE(OmpTraitSetSelectorName, Value);
};

// trait-set-selector ->
//    trait-set-selector-name = {trait-selector, ...}
struct OmpTraitSetSelector {
  CharBlock source;
  TUPLE_CLASS_BOILERPLATE(OmpTraitSetSelector);
  std::tuple<OmpTraitSetSelectorName, std::list<OmpTraitSelector>> t;
};

// context-selector-specification ->
//    trait-set-selector, ...
struct OmpContextSelectorSpecification { // Modifier
  CharBlock source;
  WRAPPER_CLASS_BOILERPLATE(
      OmpContextSelectorSpecification, std::list<OmpTraitSetSelector>);
};
} // namespace traits

#define MODIFIER_BOILERPLATE(...) \
  struct Modifier { \
    using Variant = std::variant<__VA_ARGS__>; \
    UNION_CLASS_BOILERPLATE(Modifier); \
    CharBlock source; \
    Variant u; \
  }

#define MODIFIERS() std::optional<std::list<Modifier>>

inline namespace modifier {
// For uniformity, in all keyword modifiers the name of the type defined
// by ENUM_CLASS is "Value", e.g.
// struct Foo {
//   ENUM_CLASS(Value, Keyword1, Keyword2);
// };

// Ref: [4.5:72-81], [5.0:110-119], [5.1:134-143], [5.2:169-170]
//
// alignment ->
//    scalar-integer-expression                     // since 4.5
struct OmpAlignment {
  WRAPPER_CLASS_BOILERPLATE(OmpAlignment, ScalarIntExpr);
};

// Ref: [5.1:184-185], [5.2:178-179]
//
// align-modifier ->
//    ALIGN(alignment)                              // since 5.1
struct OmpAlignModifier {
  WRAPPER_CLASS_BOILERPLATE(OmpAlignModifier, ScalarIntExpr);
};

// Ref: [5.0:158-159], [5.1:184-185], [5.2:178-179]
//
// allocator-simple-modifier ->
//    allocator                                     // since 5.0
struct OmpAllocatorSimpleModifier {
  WRAPPER_CLASS_BOILERPLATE(OmpAllocatorSimpleModifier, ScalarIntExpr);
};

// Ref: [5.1:184-185], [5.2:178-179]
//
// allocator-complex-modifier ->
//    ALLOCATOR(allocator)                          // since 5.1
struct OmpAllocatorComplexModifier {
  WRAPPER_CLASS_BOILERPLATE(OmpAllocatorComplexModifier, ScalarIntExpr);
};

// Ref: [4.5:216-219], [5.0:315-324], [5.1:347-355], [5.2:150-158],
// [6.0:279-288]
//
// always-modifier ->
//    ALWAYS                                        // since 4.5
//
// Until 5.2, it was a part of map-type-modifier. Since 6.0 the
// map-type-modifier has been split into individual modifiers.
struct OmpAlwaysModifier {
  ENUM_CLASS(Value, Always)
  WRAPPER_CLASS_BOILERPLATE(OmpAlwaysModifier, Value);
};

// Ref: [6.0:289-290]
//
// automap-modifier ->
//    automap                                       // since 6.0
//
struct OmpAutomapModifier {
  ENUM_CLASS(Value, Automap);
  WRAPPER_CLASS_BOILERPLATE(OmpAutomapModifier, Value);
};

// Ref: [5.2:252-254]
//
// chunk-modifier ->
//    SIMD                                          // since 5.2
//
// Prior to 5.2 "chunk-modifier" was a part of "modifier" on SCHEDULE clause.
struct OmpChunkModifier {
  ENUM_CLASS(Value, Simd)
  WRAPPER_CLASS_BOILERPLATE(OmpChunkModifier, Value);
};

// Ref: [4.5:216-219], [5.0:315-324], [5.1:347-355], [5.2:150-158],
// [6.0:279-288]
//
// close-modifier ->
//    CLOSE                                         // since 5.0
//
// Until 5.2, it was a part of map-type-modifier. Since 6.0 the
// map-type-modifier has been split into individual modifiers.
struct OmpCloseModifier {
  ENUM_CLASS(Value, Close)
  WRAPPER_CLASS_BOILERPLATE(OmpCloseModifier, Value);
};

// Ref: [4.5:216-219], [5.0:315-324], [5.1:347-355], [5.2:150-158],
// [6.0:279-288]
//
// delete-modifier ->
//    DELETE                                        // since 6.0
//
// Until 5.2, it was a part of map-type.
struct OmpDeleteModifier {
  ENUM_CLASS(Value, Delete)
  WRAPPER_CLASS_BOILERPLATE(OmpDeleteModifier, Value);
};

// Ref: [4.5:169-170], [5.0:255-256], [5.1:288-289]
//
// dependence-type ->
//    SINK | SOURCE |                               // since 4.5
//    IN | OUT | INOUT |                            // since 4.5, until 5.1
//    MUTEXINOUTSET | DEPOBJ |                      // since 5.0, until 5.1
//    INOUTSET                                      // since 5.1, until 5.1
//
// All of these, except SINK and SOURCE became task-dependence-type in 5.2.
//
// Keeping these two as separate types, since having them all together
// creates conflicts when parsing the DEPEND clause. For DEPEND(SINK: ...),
// the SINK may be parsed as 'task-dependence-type', and the list after
// the ':' would then be parsed as OmpObjectList (instead of the iteration
// vector). This would accept the vector "i, j, k" (although interpreted
// incorrectly), while flagging a syntax error for "i+1, j, k".
struct OmpDependenceType {
  ENUM_CLASS(Value, Sink, Source);
  WRAPPER_CLASS_BOILERPLATE(OmpDependenceType, Value);
};

// Ref: [5.0:170-176], [5.1:197-205], [5.2:276-277]
//
// device-modifier ->
//    ANCESTOR | DEVICE_NUM                         // since 5.0
struct OmpDeviceModifier {
  ENUM_CLASS(Value, Ancestor, Device_Num)
  WRAPPER_CLASS_BOILERPLATE(OmpDeviceModifier, Value);
};

// Ref: [5.2:72-73,230-323], in 4.5-5.1 it's scattered over individual
// directives that allow the IF clause.
//
// directive-name-modifier ->
//    PARALLEL | TARGET | TARGET DATA |
//    TARGET ENTER DATA | TARGET EXIT DATA |
//    TARGET UPDATE | TASK | TASKLOOP |             // since 4.5
//    CANCEL[*] | SIMD |                            // since 5.0
//    TEAMS                                         // since 5.2
//
// [*] The IF clause is allowed on CANCEL in OpenMP 4.5, but only without
// the directive-name-modifier. For the sake of uniformity CANCEL can be
// considered a valid value in 4.5 as well.
struct OmpDirectiveNameModifier : public OmpDirectiveName {
  INHERITED_WRAPPER_CLASS_BOILERPLATE(
      OmpDirectiveNameModifier, OmpDirectiveName);
};

// Ref: [5.1:205-209], [5.2:166-168]
//
// motion-modifier ->
//    PRESENT |                                     // since 5.0, until 5.0
//    mapper | iterator
// expectation ->
//    PRESENT                                       // since 5.1
//
// The PRESENT value was a part of motion-modifier in 5.1, and became a
// value of expectation in 5.2.
struct OmpExpectation {
  ENUM_CLASS(Value, Present);
  WRAPPER_CLASS_BOILERPLATE(OmpExpectation, Value);
};

// REF: [5.1:217-220], [5.2:293-294]
//
// OmpInteropRuntimeIdentifier ->                   // since 5.2
// CharLiteralConstant || ScalarIntConstantExpr
struct OmpInteropRuntimeIdentifier {
  UNION_CLASS_BOILERPLATE(OmpInteropRuntimeIdentifier);
  std::variant<CharLiteralConstant, ScalarIntConstantExpr> u;
};

// REF: [5.1:217-220], [5.2:293-294]
//
// OmpInteropPreference ->                          // since 5.2
// ([OmpRuntimeIdentifier, ...])
struct OmpInteropPreference {
  WRAPPER_CLASS_BOILERPLATE(
      OmpInteropPreference, std::list<OmpInteropRuntimeIdentifier>);
};

// REF: [5.1:217-220], [5.2:293-294]
//
// InteropType -> target || targetsync              // since 5.2
// There can be at most only two interop-type.
struct OmpInteropType {
  ENUM_CLASS(Value, Target, TargetSync)
  WRAPPER_CLASS_BOILERPLATE(OmpInteropType, Value);
};

// Ref: [5.0:47-49], [5.1:49-51], [5.2:67-69]
//
// iterator-specifier ->
//    [iterator-type] iterator-identifier
//        = range-specification |                   // since 5.0
//    [iterator-type ::] iterator-identifier
//        = range-specification                     // since 5.2
struct OmpIteratorSpecifier {
  TUPLE_CLASS_BOILERPLATE(OmpIteratorSpecifier);
  CharBlock source;
  std::tuple<TypeDeclarationStmt, SubscriptTriplet> t;
};

// Ref: [5.0:47-49], [5.1:49-51], [5.2:67-69]
//
// iterator-modifier ->
//    ITERATOR(iterator-specifier [, ...])          // since 5.0
struct OmpIterator {
  WRAPPER_CLASS_BOILERPLATE(OmpIterator, std::list<OmpIteratorSpecifier>);
};

// Ref: [5.0:288-290], [5.1:321-322], [5.2:115-117]
//
// lastprivate-modifier ->
//    CONDITIONAL                                   // since 5.0
struct OmpLastprivateModifier {
  ENUM_CLASS(Value, Conditional)
  WRAPPER_CLASS_BOILERPLATE(OmpLastprivateModifier, Value);
};

// Ref: [4.5:207-210], [5.0:290-293], [5.1:323-325], [5.2:117-120]
//
// linear-modifier ->
//    REF | UVAL | VAL                              // since 4.5
struct OmpLinearModifier {
  ENUM_CLASS(Value, Ref, Uval, Val);
  WRAPPER_CLASS_BOILERPLATE(OmpLinearModifier, Value);
};

// Ref: [5.0:176-180], [5.1:205-210], [5.2:149-150]
//
// mapper ->
//    identifier                                    // since 4.5
struct OmpMapper {
  WRAPPER_CLASS_BOILERPLATE(OmpMapper, Name);
};

// Ref: [4.5:216-219], [5.0:315-324], [5.1:347-355], [5.2:150-158],
// [6.0:279-288]
//
// map-type ->
//    ALLOC | DELETE | RELEASE |                    // since 4.5, until 5.2
//    FROM | TO | TOFROM |                          // since 4.5
//    STORAGE                                       // since 6.0
//
// Since 6.0 DELETE is a separate delete-modifier.
struct OmpMapType {
  ENUM_CLASS(Value, Alloc, Delete, From, Release, Storage, To, Tofrom);
  WRAPPER_CLASS_BOILERPLATE(OmpMapType, Value);
};

// Ref: [4.5:216-219], [5.0:315-324], [5.1:347-355], [5.2:150-158]
//
// map-type-modifier ->
//    ALWAYS |                                      // since 4.5, until 5.2
//    CLOSE |                                       // since 5.0, until 5.2
//    PRESENT                                       // since 5.1, until 5.2
// Since 6.0 the map-type-modifier has been split into individual modifiers.
//
struct OmpMapTypeModifier {
  ENUM_CLASS(Value, Always, Close, Present, Ompx_Hold)
  WRAPPER_CLASS_BOILERPLATE(OmpMapTypeModifier, Value);
};

// Ref: [4.5:56-63], [5.0:101-109], [5.1:126-133], [5.2:252-254]
//
// modifier ->
//    MONOTONIC | NONMONOTONIC | SIMD               // since 4.5, until 5.1
// ordering-modifier ->
//    MONOTONIC | NONMONOTONIC                      // since 5.2
//
// Until 5.1, the SCHEDULE clause accepted up to two instances of "modifier".
// Since 5.2 "modifier" was replaced with "ordering-modifier" and "chunk-
// modifier".
struct OmpOrderingModifier {
  ENUM_CLASS(Value, Monotonic, Nonmonotonic, Simd)
  WRAPPER_CLASS_BOILERPLATE(OmpOrderingModifier, Value);
};

// Ref: [5.1:125-126], [5.2:233-234]
//
// order-modifier ->
//    REPRODUCIBLE | UNCONSTRAINED                  // since 5.1
struct OmpOrderModifier {
  ENUM_CLASS(Value, Reproducible, Unconstrained)
  WRAPPER_CLASS_BOILERPLATE(OmpOrderModifier, Value);
};

// Ref: [5.1:166-171], [5.2:269-270]
//
// prescriptiveness ->
//    STRICT                                        // since 5.1
struct OmpPrescriptiveness {
  ENUM_CLASS(Value, Strict)
  WRAPPER_CLASS_BOILERPLATE(OmpPrescriptiveness, Value);
};

// Ref: [4.5:216-219], [5.0:315-324], [5.1:347-355], [5.2:150-158],
// [6.0:279-288]
//
// present-modifier ->
//    PRESENT                                       // since 5.1
//
// Until 5.2, it was a part of map-type-modifier. Since 6.0 the
// map-type-modifier has been split into individual modifiers.
struct OmpPresentModifier {
  ENUM_CLASS(Value, Present)
  WRAPPER_CLASS_BOILERPLATE(OmpPresentModifier, Value);
};

// Ref: [5.0:300-302], [5.1:332-334], [5.2:134-137]
//
// reduction-modifier ->
//    DEFAULT | INSCAN | TASK                       // since 5.0
struct OmpReductionModifier {
  ENUM_CLASS(Value, Default, Inscan, Task);
  WRAPPER_CLASS_BOILERPLATE(OmpReductionModifier, Value);
};

// Ref: [6.0:279-288]
//
// ref-modifier ->
//    REF_PTEE | REF_PTR | REF_PTR_PTEE             // since 6.0
//
struct OmpRefModifier {
  ENUM_CLASS(Value, Ref_Ptee, Ref_Ptr, Ref_Ptr_Ptee)
  WRAPPER_CLASS_BOILERPLATE(OmpRefModifier, Value);
};

// Ref: [6.0:279-288]
//
// self-modifier ->
//    SELF                                          // since 6.0
//
struct OmpSelfModifier {
  ENUM_CLASS(Value, Self)
  WRAPPER_CLASS_BOILERPLATE(OmpSelfModifier, Value);
};

// Ref: [5.2:117-120]
//
// step-complex-modifier ->
//    STEP(integer-expression)                      // since 5.2
struct OmpStepComplexModifier {
  WRAPPER_CLASS_BOILERPLATE(OmpStepComplexModifier, ScalarIntExpr);
};

// Ref: [4.5:207-210], [5.0:290-293], [5.1:323-325], [5.2:117-120]
//
// step-simple-modifier ->
//    integer-expresion                             // since 4.5
struct OmpStepSimpleModifier {
  WRAPPER_CLASS_BOILERPLATE(OmpStepSimpleModifier, ScalarIntExpr);
};

// Ref: [4.5:169-170], [5.0:254-256], [5.1:287-289], [5.2:321]
//
// task-dependence-type -> // "dependence-type" in 5.1 and before
//    IN | OUT | INOUT |                            // since 4.5
//    MUTEXINOUTSET | DEPOBJ |                      // since 5.0
//    INOUTSET                                      // since 5.2
struct OmpTaskDependenceType {
  ENUM_CLASS(Value, In, Out, Inout, Inoutset, Mutexinoutset, Depobj)
  WRAPPER_CLASS_BOILERPLATE(OmpTaskDependenceType, Value);
};

// Ref: [4.5:229-230], [5.0:324-325], [5.1:357-358], [5.2:161-162]
//
// variable-category ->
//    SCALAR |                                      // since 4.5
//    AGGREGATE | ALLOCATABLE | POINTER |           // since 5.0
//    ALL                                           // since 5.2
struct OmpVariableCategory {
  ENUM_CLASS(Value, Aggregate, All, Allocatable, Pointer, Scalar)
  WRAPPER_CLASS_BOILERPLATE(OmpVariableCategory, Value);
};

// Extension:
// https://openmp.llvm.org//openacc/OpenMPExtensions.html#ompx-hold
//
// ompx-hold-modifier ->
//    OMPX_HOLD                                     // since 4.5
//
// Until 5.2, it was a part of map-type-modifier. Since 6.0 the
// map-type-modifier has been split into individual modifiers.
struct OmpxHoldModifier {
  ENUM_CLASS(Value, Ompx_Hold)
  WRAPPER_CLASS_BOILERPLATE(OmpxHoldModifier, Value);
};

// context-selector
using OmpContextSelector = traits::OmpContextSelectorSpecification;
} // namespace modifier

// --- Clauses

using OmpDirectiveList = std::list<llvm::omp::Directive>;

// Ref: [5.2:214]
//
// absent-clause ->
//   ABSENT(directive-name[, directive-name])
struct OmpAbsentClause {
  WRAPPER_CLASS_BOILERPLATE(OmpAbsentClause, OmpDirectiveList);
};

struct OmpAdjustArgsClause {
  TUPLE_CLASS_BOILERPLATE(OmpAdjustArgsClause);
  struct OmpAdjustOp {
    ENUM_CLASS(Value, Nothing, Need_Device_Ptr)
    WRAPPER_CLASS_BOILERPLATE(OmpAdjustOp, Value);
  };
  std::tuple<OmpAdjustOp, OmpObjectList> t;
};

// Ref: [5.0:135-140], [5.1:161-166], [5.2:264-265]
//
// affinity-clause ->
//    AFFINITY([aff-modifier:] locator-list)        // since 5.0
// aff-modifier ->
//    interator-modifier                            // since 5.0
struct OmpAffinityClause {
  TUPLE_CLASS_BOILERPLATE(OmpAffinityClause);
  MODIFIER_BOILERPLATE(OmpIterator);
  std::tuple<MODIFIERS(), OmpObjectList> t;
};

// Ref: 5.2: [174]
struct OmpAlignClause {
  WRAPPER_CLASS_BOILERPLATE(OmpAlignClause, ScalarIntExpr);
};

// Ref: [4.5:72-81], [5.0:110-119], [5.1:134-143], [5.2:169-170]
//
// aligned-clause ->
//    ALIGNED(list [: alignment])                   // since 4.5
struct OmpAlignedClause {
  TUPLE_CLASS_BOILERPLATE(OmpAlignedClause);
  MODIFIER_BOILERPLATE(OmpAlignment);
  std::tuple<OmpObjectList, MODIFIERS()> t;
};

// Ref: [5.0:158-159], [5.1:184-185], [5.2:178-179]
//
// allocate-clause ->
//    ALLOCATE(
//        [allocator-simple-modifier:] list) |      // since 5.0
//    ALLOCATE([modifier...:] list)                 // since 5.1
// modifier ->
//    allocator-simple-modifier |
//    allocator-complex-modifier | align-modifier   // since 5.1
struct OmpAllocateClause {
  MODIFIER_BOILERPLATE(OmpAlignModifier, OmpAllocatorSimpleModifier,
      OmpAllocatorComplexModifier);
  TUPLE_CLASS_BOILERPLATE(OmpAllocateClause);
  std::tuple<MODIFIERS(), OmpObjectList> t;
};

struct OmpAppendArgsClause {
  struct OmpAppendOp {
    WRAPPER_CLASS_BOILERPLATE(OmpAppendOp, std::list<OmpInteropType>);
  };
  WRAPPER_CLASS_BOILERPLATE(OmpAppendArgsClause, std::list<OmpAppendOp>);
};

// Ref: [5.2:216-217 (sort of, as it's only mentioned in passing)
// AT(compilation|execution)
struct OmpAtClause {
  ENUM_CLASS(ActionTime, Compilation, Execution);
  WRAPPER_CLASS_BOILERPLATE(OmpAtClause, ActionTime);
};

// Ref: [5.0:60-63], [5.1:83-86], [5.2:210-213]
//
// atomic-default-mem-order-clause ->
//    ATOMIC_DEFAULT_MEM_ORDER(memory-order)        // since 5.0
// memory-order ->
//    SEQ_CST | ACQ_REL | RELAXED |                 // since 5.0
//    ACQUIRE | RELEASE                             // since 5.2
struct OmpAtomicDefaultMemOrderClause {
  using MemoryOrder = common::OmpMemoryOrderType;
  WRAPPER_CLASS_BOILERPLATE(OmpAtomicDefaultMemOrderClause, MemoryOrder);
};

// Ref: [5.0:128-131], [5.1:151-154], [5.2:258-259]
//
// bind-clause ->
//    BIND(binding)                                 // since 5.0
// binding ->
//    TEAMS | PARALLEL | THREAD                     // since 5.0
struct OmpBindClause {
  ENUM_CLASS(Binding, Parallel, Teams, Thread)
  WRAPPER_CLASS_BOILERPLATE(OmpBindClause, Binding);
};

// Artificial clause to represent a cancellable construct.
struct OmpCancellationConstructTypeClause {
  TUPLE_CLASS_BOILERPLATE(OmpCancellationConstructTypeClause);
  std::tuple<OmpDirectiveName, std::optional<ScalarLogicalExpr>> t;
};

// Ref: [5.2:214]
//
// contains-clause ->
//   CONTAINS(directive-name[, directive-name])
struct OmpContainsClause {
  WRAPPER_CLASS_BOILERPLATE(OmpContainsClause, OmpDirectiveList);
};

// Ref: [4.5:46-50], [5.0:74-78], [5.1:92-96], [5.2:109]
//
// When used as a data-sharing clause:
// default-clause ->
//    DEFAULT(data-sharing-attribute)               // since 4.5
// data-sharing-attribute ->
//    SHARED | NONE |                               // since 4.5
//    PRIVATE | FIRSTPRIVATE                        // since 5.0
//
// When used in METADIRECTIVE:
// default-clause ->
//    DEFAULT(directive-specification)              // since 5.0, until 5.1
// See also otherwise-clause.
struct OmpDefaultClause {
  ENUM_CLASS(DataSharingAttribute, Private, Firstprivate, Shared, None)
  UNION_CLASS_BOILERPLATE(OmpDefaultClause);
  std::variant<DataSharingAttribute,
      common::Indirection<OmpDirectiveSpecification>>
      u;
};

// Ref: [4.5:103-107], [5.0:324-325], [5.1:357-358], [5.2:161-162]
//
// defaultmap-clause ->
//    DEFAULTMAP(implicit-behavior
//        [: variable-category])                    // since 5.0
// implicit-behavior ->
//    TOFROM |                                      // since 4.5
//    ALLOC | TO | FROM | FIRSTPRIVATE | NONE |
//    DEFAULT |                                     // since 5.0
//    PRESENT                                       // since 5.1
struct OmpDefaultmapClause {
  TUPLE_CLASS_BOILERPLATE(OmpDefaultmapClause);
  ENUM_CLASS(ImplicitBehavior, Alloc, To, From, Tofrom, Firstprivate, None,
      Default, Present)
  MODIFIER_BOILERPLATE(OmpVariableCategory);
  std::tuple<ImplicitBehavior, MODIFIERS()> t;
};

// Ref: [4.5:169-172], [5.0:255-259], [5.1:288-292], [5.2:91-93]
//
// iteration-offset ->
//    +|- non-negative-constant                     // since 4.5
struct OmpIterationOffset {
  TUPLE_CLASS_BOILERPLATE(OmpIterationOffset);
  std::tuple<DefinedOperator, ScalarIntConstantExpr> t;
};

// Ref: [4.5:169-172], [5.0:255-259], [5.1:288-292], [5.2:91-93]
//
// iteration ->
//    induction-variable [iteration-offset]         // since 4.5
struct OmpIteration {
  TUPLE_CLASS_BOILERPLATE(OmpIteration);
  std::tuple<Name, std::optional<OmpIterationOffset>> t;
};

// Ref: [4.5:169-172], [5.0:255-259], [5.1:288-292], [5.2:91-93]
//
// iteration-vector ->
//    [iteration...]                                // since 4.5
WRAPPER_CLASS(OmpIterationVector, std::list<OmpIteration>);

// Extract this into a separate structure (instead of having it directly in
// OmpDoacrossClause), so that the context in TYPE_CONTEXT_PARSER can be set
// separately for OmpDependClause and OmpDoacrossClause.
//
// See: depend-clause, doacross-clause
struct OmpDoacross {
  OmpDependenceType::Value GetDepType() const;

  WRAPPER_CLASS(Sink, OmpIterationVector);
  EMPTY_CLASS(Source);
  UNION_CLASS_BOILERPLATE(OmpDoacross);
  std::variant<Sink, Source> u;
};

// Ref: [4.5:169-172], [5.0:255-259], [5.1:288-292], [5.2:323-326]
//
// depend-clause ->
//    DEPEND(SOURCE) |                              // since 4.5, until 5.1
//    DEPEND(SINK: iteration-vector) |              // since 4.5, until 5.1
//    DEPEND([depend-modifier,]
//           task-dependence-type: locator-list)    // since 4.5
//
// depend-modifier -> iterator-modifier             // since 5.0
struct OmpDependClause {
  UNION_CLASS_BOILERPLATE(OmpDependClause);
  struct TaskDep {
    OmpTaskDependenceType::Value GetTaskDepType() const;
    TUPLE_CLASS_BOILERPLATE(TaskDep);
    MODIFIER_BOILERPLATE(OmpIterator, OmpTaskDependenceType);
    std::tuple<MODIFIERS(), OmpObjectList> t;
  };
  std::variant<TaskDep, OmpDoacross> u;
};

// Ref: [5.2:326-328]
//
// doacross-clause ->
//    DOACROSS(dependence-type: iteration-vector)   // since 5.2
WRAPPER_CLASS(OmpDoacrossClause, OmpDoacross);

// Ref: [5.0:254-255], [5.1:287-288], [5.2:73]
//
// destroy-clause ->
//    DESTROY |                                     // since 5.0, until 5.1
//    DESTROY(variable)                             // since 5.2
WRAPPER_CLASS(OmpDestroyClause, OmpObject);

// Ref: [5.0:135-140], [5.1:161-166], [5.2:265-266]
//
// detach-clause ->
//    DETACH(event-handle)                          // since 5.0
struct OmpDetachClause {
  WRAPPER_CLASS_BOILERPLATE(OmpDetachClause, OmpObject);
};

// Ref: [4.5:103-107], [5.0:170-176], [5.1:197-205], [5.2:276-277]
//
// device-clause ->
//    DEVICE(scalar-integer-expression) |           // since 4.5
//    DEVICE([device-modifier:]
//        scalar-integer-expression)                // since 5.0
struct OmpDeviceClause {
  TUPLE_CLASS_BOILERPLATE(OmpDeviceClause);
  MODIFIER_BOILERPLATE(OmpDeviceModifier);
  std::tuple<MODIFIERS(), ScalarIntExpr> t;
};

// Ref: [5.0:180-185], [5.1:210-216], [5.2:275]
//
// device-type-clause ->
//    DEVICE_TYPE(ANY | HOST | NOHOST)              // since 5.0
struct OmpDeviceTypeClause {
  ENUM_CLASS(DeviceTypeDescription, Any, Host, Nohost)
  WRAPPER_CLASS_BOILERPLATE(OmpDeviceTypeClause, DeviceTypeDescription);
};

// Ref: [5.2:158-159], [6.0:289-290]
//
// enter-clause ->
//    ENTER(locator-list) |
//    ENTER(automap-modifier: locator-list) |         // since 6.0
struct OmpEnterClause {
  TUPLE_CLASS_BOILERPLATE(OmpEnterClause);
  MODIFIER_BOILERPLATE(OmpAutomapModifier);
  std::tuple<MODIFIERS(), OmpObjectList> t;
};

// OMP 5.2 15.8.3 extended-atomic, fail-clause ->
//    FAIL(memory-order)
struct OmpFailClause {
  using MemoryOrder = common::OmpMemoryOrderType;
  WRAPPER_CLASS_BOILERPLATE(OmpFailClause, MemoryOrder);
};

// Ref: [4.5:107-109], [5.0:176-180], [5.1:205-210], [5.2:167-168]
//
// from-clause ->
//    FROM(locator-list) |
//    FROM(mapper-modifier: locator-list) |         // since 5.0
//    FROM(motion-modifier[,] ...: locator-list)    // since 5.1
//  motion-modifier ->
//    PRESENT | mapper-modifier | iterator-modifier
struct OmpFromClause {
  TUPLE_CLASS_BOILERPLATE(OmpFromClause);
  MODIFIER_BOILERPLATE(OmpExpectation, OmpIterator, OmpMapper);
  std::tuple<MODIFIERS(), OmpObjectList, /*CommaSeparated=*/bool> t;
};

// Ref: [4.5:87-91], [5.0:140-146], [5.1:166-171], [5.2:269]
//
// grainsize-clause ->
//    GRAINSIZE(grain-size) |                       // since 4.5
//    GRAINSIZE([prescriptiveness:] grain-size)     // since 5.1
struct OmpGrainsizeClause {
  TUPLE_CLASS_BOILERPLATE(OmpGrainsizeClause);
  MODIFIER_BOILERPLATE(OmpPrescriptiveness);
  std::tuple<MODIFIERS(), ScalarIntExpr> t;
};

// Ref: [5.0:234-242], [5.1:266-275], [5.2:299], [6.0:472-473]
struct OmpHintClause {
  WRAPPER_CLASS_BOILERPLATE(OmpHintClause, ScalarIntConstantExpr);
};

// Ref: [5.2: 214]
//
// holds-clause ->
//   HOLDS(expr)
struct OmpHoldsClause {
  WRAPPER_CLASS_BOILERPLATE(OmpHoldsClause, common::Indirection<Expr>);
};

// Ref: [5.2: 209]
struct OmpIndirectClause {
  WRAPPER_CLASS_BOILERPLATE(
      OmpIndirectClause, std::optional<ScalarLogicalExpr>);
};

// Ref: [5.2:72-73], in 4.5-5.1 it's scattered over individual directives
// that allow the IF clause.
//
// if-clause ->
//    IF([directive-name-modifier:]
//        scalar-logical-expression)                // since 4.5
struct OmpIfClause {
  TUPLE_CLASS_BOILERPLATE(OmpIfClause);
  MODIFIER_BOILERPLATE(OmpDirectiveNameModifier);
  std::tuple<MODIFIERS(), ScalarLogicalExpr> t;
};

// Ref: [5.0:170-176], [5.1:197-205], [5.2:138-139]
//
// in-reduction-clause ->
//    IN_REDUCTION(reduction-identifier: list)      // since 5.0
struct OmpInReductionClause {
  TUPLE_CLASS_BOILERPLATE(OmpInReductionClause);
  MODIFIER_BOILERPLATE(OmpReductionIdentifier);
  std::tuple<MODIFIERS(), OmpObjectList> t;
};

// declare-reduction -> DECLARE REDUCTION (reduction-identifier : type-list
//                                              : combiner) [initializer-clause]
struct OmpInitializerProc {
  TUPLE_CLASS_BOILERPLATE(OmpInitializerProc);
  std::tuple<ProcedureDesignator, std::list<ActualArgSpec>> t;
};
// Initialization for declare reduction construct
struct OmpInitializerClause {
  UNION_CLASS_BOILERPLATE(OmpInitializerClause);
  std::variant<OmpInitializerProc, AssignmentStmt> u;
};

// Ref: [4.5:199-201], [5.0:288-290], [5.1:321-322], [5.2:115-117]
//
// lastprivate-clause ->
//    LASTPRIVATE(list) |                           // since 4.5
//    LASTPRIVATE([lastprivate-modifier:] list)     // since 5.0
struct OmpLastprivateClause {
  TUPLE_CLASS_BOILERPLATE(OmpLastprivateClause);
  MODIFIER_BOILERPLATE(OmpLastprivateModifier);
  std::tuple<MODIFIERS(), OmpObjectList> t;
};

// Ref: [4.5:207-210], [5.0:290-293], [5.1:323-325], [5.2:117-120]
//
// linear-clause ->
//    LINEAR(list [: step-simple-modifier]) |       // since 4.5
//    LINEAR(linear-modifier(list)
//        [: step-simple-modifier]) |               // since 4.5, until 5.2[*]
//    LINEAR(list [: linear-modifier,
//        step-complex-modifier])                   // since 5.2
// [*] Still allowed in 5.2 when on DECLARE SIMD, but deprecated.
struct OmpLinearClause {
  TUPLE_CLASS_BOILERPLATE(OmpLinearClause);
  MODIFIER_BOILERPLATE(
      OmpLinearModifier, OmpStepSimpleModifier, OmpStepComplexModifier);
  std::tuple<OmpObjectList, MODIFIERS(), /*PostModified=*/bool> t;
};

// Ref: [4.5:216-219], [5.0:315-324], [5.1:347-355], [5.2:150-158]
//
// map-clause ->
//    MAP([modifier...:] locator-list)              // since 4.5
// modifier ->
//    map-type-modifier [replaced] |                // since 4.5, until 5.2
//    always-modifier |                             // since 6.0
//    close-modifier |                              // since 6.0
//    delete-modifier |                             // since 6.0
//    present-modifier |                            // since 6.0
//    ref-modifier |                                // since 6.0
//    self-modifier |                               // since 6.0
//    mapper |                                      // since 5.0
//    iterator |                                    // since 5.1
//    map-type                                      // since 4.5
//    ompx-hold-modifier |                          // since 6.0
//
// Since 6.0 the map-type-modifier has been split into individual modifiers,
// and delete-modifier has been split from map-type.
struct OmpMapClause {
  TUPLE_CLASS_BOILERPLATE(OmpMapClause);
  MODIFIER_BOILERPLATE(OmpAlwaysModifier, OmpCloseModifier, OmpDeleteModifier,
      OmpMapTypeModifier, OmpPresentModifier, OmpRefModifier, OmpSelfModifier,
      OmpMapper, OmpIterator, OmpMapType, OmpxHoldModifier);
  std::tuple<MODIFIERS(), OmpObjectList, /*CommaSeparated=*/bool> t;
};

// Ref: [5.0:58-60], [5.1:63-68], [5.2:194-195]
//
// match-clause ->
//    MATCH (context-selector-specification)        // since 5.0
struct OmpMatchClause {
  // The context-selector is an argument.
  WRAPPER_CLASS_BOILERPLATE(
      OmpMatchClause, traits::OmpContextSelectorSpecification);
};

// Ref: [5.2:217-218]
// message-clause ->
//    MESSAGE("message-text")
struct OmpMessageClause {
  WRAPPER_CLASS_BOILERPLATE(OmpMessageClause, Expr);
};

// Ref: [5.2: 214]
//
// no_openmp_clause -> NO_OPENMP
EMPTY_CLASS(OmpNoOpenMPClause);

// Ref: [5.2: 214]
//
// no_openmp_routines_clause -> NO_OPENMP_ROUTINES
EMPTY_CLASS(OmpNoOpenMPRoutinesClause);

// Ref: [5.2: 214]
//
// no_parallelism_clause -> NO_PARALELISM
EMPTY_CLASS(OmpNoParallelismClause);

// Ref: [4.5:87-91], [5.0:140-146], [5.1:166-171], [5.2:270]
//
// num-tasks-clause ->
//    NUM_TASKS(num-tasks) |                        // since 4.5
//    NUM_TASKS([prescriptiveness:] num-tasks)      // since 5.1
struct OmpNumTasksClause {
  TUPLE_CLASS_BOILERPLATE(OmpNumTasksClause);
  MODIFIER_BOILERPLATE(OmpPrescriptiveness);
  std::tuple<MODIFIERS(), ScalarIntExpr> t;
};

// Ref: [5.0:101-109], [5.1:126-134], [5.2:233-234]
//
// order-clause ->
//    ORDER(CONCURRENT) |                           // since 5.0
//    ORDER([order-modifier:] CONCURRENT)           // since 5.1
struct OmpOrderClause {
  TUPLE_CLASS_BOILERPLATE(OmpOrderClause);
  ENUM_CLASS(Ordering, Concurrent)
  MODIFIER_BOILERPLATE(OmpOrderModifier);
  std::tuple<MODIFIERS(), Ordering> t;
};

// Ref: [5.0:56-57], [5.1:60-62], [5.2:191]
//
// otherwise-clause ->
//    DEFAULT ([directive-specification])           // since 5.0, until 5.1
// otherwise-clause ->
//    OTHERWISE ([directive-specification])]        // since 5.2
struct OmpOtherwiseClause {
  WRAPPER_CLASS_BOILERPLATE(OmpOtherwiseClause,
      std::optional<common::Indirection<OmpDirectiveSpecification>>);
};

// Ref: [4.5:46-50], [5.0:74-78], [5.1:92-96], [5.2:229-230]
//
// proc-bind-clause ->
//    PROC_BIND(affinity-policy)                    // since 4.5
// affinity-policy ->
//    CLOSE | PRIMARY | SPREAD |                    // since 4.5
//    MASTER                                        // since 4.5, until 5.2
struct OmpProcBindClause {
  ENUM_CLASS(AffinityPolicy, Close, Master, Spread, Primary)
  WRAPPER_CLASS_BOILERPLATE(OmpProcBindClause, AffinityPolicy);
};

// Ref: [4.5:201-207], [5.0:300-302], [5.1:332-334], [5.2:134-137]
//
// reduction-clause ->
//    REDUCTION(reduction-identifier: list) |       // since 4.5
//    REDUCTION([reduction-modifier,]
//        reduction-identifier: list)               // since 5.0
struct OmpReductionClause {
  TUPLE_CLASS_BOILERPLATE(OmpReductionClause);
  MODIFIER_BOILERPLATE(OmpReductionModifier, OmpReductionIdentifier);
  std::tuple<MODIFIERS(), OmpObjectList> t;
};

// Ref: [4.5:56-63], [5.0:101-109], [5.1:126-133], [5.2:252-254]
//
// schedule-clause ->
//    SCHEDULE([modifier[, modifier]:]
//        kind[, chunk-size])                       // since 4.5, until 5.1
// schedule-clause ->
//    SCHEDULE([ordering-modifier], chunk-modifier],
//        kind[, chunk_size])                       // since 5.2
struct OmpScheduleClause {
  TUPLE_CLASS_BOILERPLATE(OmpScheduleClause);
  ENUM_CLASS(Kind, Static, Dynamic, Guided, Auto, Runtime)
  MODIFIER_BOILERPLATE(OmpOrderingModifier, OmpChunkModifier);
  std::tuple<MODIFIERS(), Kind, std::optional<ScalarIntExpr>> t;
};

// REF: [5.2:217]
// severity-clause ->
//    SEVERITY(warning|fatal)
struct OmpSeverityClause {
  ENUM_CLASS(Severity, Fatal, Warning);
  WRAPPER_CLASS_BOILERPLATE(OmpSeverityClause, Severity);
};

// Ref: [5.0:232-234], [5.1:264-266], [5.2:137]
//
// task-reduction-clause ->
//    TASK_REDUCTION(reduction-identifier: list)    // since 5.0
struct OmpTaskReductionClause {
  TUPLE_CLASS_BOILERPLATE(OmpTaskReductionClause);
  MODIFIER_BOILERPLATE(OmpReductionIdentifier);
  std::tuple<MODIFIERS(), OmpObjectList> t;
};

// Ref: [4.5:107-109], [5.0:176-180], [5.1:205-210], [5.2:167-168]
//
// to-clause (in DECLARE TARGET) ->
//    TO(extended-list) |                           // until 5.1
// to-clause (in TARGET UPDATE) ->
//    TO(locator-list) |
//    TO(mapper-modifier: locator-list) |           // since 5.0
//    TO(motion-modifier[,] ...: locator-list)      // since 5.1
// motion-modifier ->
//    PRESENT | mapper-modifier | iterator-modifier
struct OmpToClause {
  TUPLE_CLASS_BOILERPLATE(OmpToClause);
  MODIFIER_BOILERPLATE(OmpExpectation, OmpIterator, OmpMapper);
  std::tuple<MODIFIERS(), OmpObjectList, /*CommaSeparated=*/bool> t;
};

// Ref: [5.0:254-255], [5.1:287-288], [5.2:321-322]
//
// In ATOMIC construct
// update-clause ->
//    UPDATE                                        // Since 4.5
//
// In DEPOBJ construct
// update-clause ->
//    UPDATE(dependence-type)                       // since 5.0, until 5.1
// update-clause ->
//    UPDATE(task-dependence-type)                  // since 5.2
struct OmpUpdateClause {
  UNION_CLASS_BOILERPLATE(OmpUpdateClause);
  // The dependence type is an argument here, not a modifier.
  std::variant<OmpDependenceType, OmpTaskDependenceType> u;
};

// Ref: [5.0:56-57], [5.1:60-62], [5.2:190-191]
//
// when-clause ->
//    WHEN (context-selector :
//        [directive-specification])                // since 5.0
struct OmpWhenClause {
  TUPLE_CLASS_BOILERPLATE(OmpWhenClause);
  MODIFIER_BOILERPLATE(OmpContextSelector);
  std::tuple<MODIFIERS(),
      std::optional<common::Indirection<OmpDirectiveSpecification>>>
      t;
};

// REF: [5.1:217-220], [5.2:293-294]
//
// init-clause -> INIT ([interop-modifier,] [interop-type,]
//                              interop-type: interop-var)
// interop-modifier: prefer_type(preference-list)
// interop-type: target, targetsync
// interop-var: Ompobject
// There can be at most only two interop-type.
struct OmpInitClause {
  TUPLE_CLASS_BOILERPLATE(OmpInitClause);
  MODIFIER_BOILERPLATE(OmpInteropPreference, OmpInteropType);
  std::tuple<MODIFIERS(), OmpObject> t;
};

// REF: [5.1:217-220], [5.2:294]
//
// 14.1.3 use-clause -> USE (interop-var)
WRAPPER_CLASS(OmpUseClause, OmpObject);

// OpenMP Clauses
struct OmpClause {
  UNION_CLASS_BOILERPLATE(OmpClause);
  llvm::omp::Clause Id() const;

#define GEN_FLANG_CLAUSE_PARSER_CLASSES
#include "llvm/Frontend/OpenMP/OMP.inc"

  CharBlock source;

  std::variant<
#define GEN_FLANG_CLAUSE_PARSER_CLASSES_LIST
#include "llvm/Frontend/OpenMP/OMP.inc"
      >
      u;
};

struct OmpClauseList {
  WRAPPER_CLASS_BOILERPLATE(OmpClauseList, std::list<OmpClause>);
  CharBlock source;
};

// --- Directives and constructs

struct OmpDirectiveSpecification {
  ENUM_CLASS(Flags, None, DeprecatedSyntax);
  TUPLE_CLASS_BOILERPLATE(OmpDirectiveSpecification);
  const OmpDirectiveName &DirName() const {
    return std::get<OmpDirectiveName>(t);
  }
  llvm::omp::Directive DirId() const { //
    return DirName().v;
  }
  const OmpArgumentList &Arguments() const;
  const OmpClauseList &Clauses() const;

  CharBlock source;
  std::tuple<OmpDirectiveName, std::optional<OmpArgumentList>,
      std::optional<OmpClauseList>, Flags>
      t;
};

struct OmpMetadirectiveDirective {
  TUPLE_CLASS_BOILERPLATE(OmpMetadirectiveDirective);
  std::tuple<Verbatim, OmpClauseList> t;
  CharBlock source;
};

// Ref: [5.1:89-90], [5.2:216]
//
// nothing-directive ->
//    NOTHING                                     // since 5.1
struct OmpNothingDirective {
  using EmptyTrait = std::true_type;
  COPY_AND_ASSIGN_BOILERPLATE(OmpNothingDirective);
  CharBlock source;
};

// Ref: OpenMP [5.2:216-218]
// ERROR AT(compilation|execution) SEVERITY(fatal|warning) MESSAGE("msg-str)
struct OmpErrorDirective {
  TUPLE_CLASS_BOILERPLATE(OmpErrorDirective);
  CharBlock source;
  std::tuple<Verbatim, OmpClauseList> t;
};

struct OpenMPUtilityConstruct {
  UNION_CLASS_BOILERPLATE(OpenMPUtilityConstruct);
  CharBlock source;
  std::variant<OmpErrorDirective, OmpNothingDirective> u;
};

// Ref: [5.2: 213-216]
//
// assumes-construct ->
//   ASSUMES absent-clause | contains-clause | holds-clause | no-openmp-clause |
//          no-openmp-routines-clause | no-parallelism-clause
struct OpenMPDeclarativeAssumes {
  TUPLE_CLASS_BOILERPLATE(OpenMPDeclarativeAssumes);
  std::tuple<Verbatim, OmpClauseList> t;
  CharBlock source;
};

struct OmpAssumeDirective {
  TUPLE_CLASS_BOILERPLATE(OmpAssumeDirective);
  std::tuple<Verbatim, OmpClauseList> t;
  CharBlock source;
};

struct OmpEndAssumeDirective {
  WRAPPER_CLASS_BOILERPLATE(OmpEndAssumeDirective, Verbatim);
  CharBlock source;
};

// Ref: [5.2: 213-216]
//
// assume-construct ->
//   ASSUME absent-clause | contains-clause | holds_clause | no-openmp-clause
//          no-openmp-routines-clause | no-parallelism-clause
//       block
//   [END ASSUME]
struct OpenMPAssumeConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPAssumeConstruct);
  std::tuple<OmpAssumeDirective, Block, std::optional<OmpEndAssumeDirective>> t;
  CharBlock source;
};

// 2.7.2 SECTIONS
// 2.11.2 PARALLEL SECTIONS
struct OmpSectionsDirective {
  WRAPPER_CLASS_BOILERPLATE(OmpSectionsDirective, llvm::omp::Directive);
  CharBlock source;
};

struct OmpBeginSectionsDirective {
  TUPLE_CLASS_BOILERPLATE(OmpBeginSectionsDirective);
  std::tuple<OmpSectionsDirective, OmpClauseList> t;
  CharBlock source;
};
struct OmpEndSectionsDirective {
  TUPLE_CLASS_BOILERPLATE(OmpEndSectionsDirective);
  std::tuple<OmpSectionsDirective, OmpClauseList> t;
  CharBlock source;
};

// [!$omp section]
//    structured-block
// [!$omp section
//    structured-block]
// ...
struct OpenMPSectionConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPSectionConstruct);
  std::tuple<std::optional<OmpDirectiveSpecification>, Block> t;
  CharBlock source;
};

struct OpenMPSectionsConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPSectionsConstruct);
  CharBlock source;
  // Each of the OpenMPConstructs in the list below contains an
  // OpenMPSectionConstruct. This is guaranteed by the parser.
  std::tuple<OmpBeginSectionsDirective, std::list<OpenMPConstruct>,
      OmpEndSectionsDirective>
      t;
};

// OpenMP directive beginning or ending a block
struct OmpBlockDirective {
  WRAPPER_CLASS_BOILERPLATE(OmpBlockDirective, llvm::omp::Directive);
  CharBlock source;
};

struct OmpDeclareVariantDirective {
  TUPLE_CLASS_BOILERPLATE(OmpDeclareVariantDirective);
  CharBlock source;
  std::tuple<Verbatim, std::optional<Name>, Name, OmpClauseList> t;
};

// 2.10.6 declare-target -> DECLARE TARGET (extended-list) |
//                          DECLARE TARGET [declare-target-clause[ [,]
//                                          declare-target-clause]...]
struct OmpDeclareTargetWithList {
  WRAPPER_CLASS_BOILERPLATE(OmpDeclareTargetWithList, OmpObjectList);
  CharBlock source;
};

struct OmpDeclareTargetWithClause {
  WRAPPER_CLASS_BOILERPLATE(OmpDeclareTargetWithClause, OmpClauseList);
  CharBlock source;
};

struct OmpDeclareTargetSpecifier {
  UNION_CLASS_BOILERPLATE(OmpDeclareTargetSpecifier);
  std::variant<OmpDeclareTargetWithList, OmpDeclareTargetWithClause> u;
};

struct OpenMPDeclareTargetConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPDeclareTargetConstruct);
  CharBlock source;
  std::tuple<Verbatim, OmpDeclareTargetSpecifier> t;
};

// OMP v5.2: 5.8.8
//  declare-mapper -> DECLARE MAPPER ([mapper-name :] type :: var) map-clauses
struct OpenMPDeclareMapperConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPDeclareMapperConstruct);
  CharBlock source;
  std::tuple<Verbatim, OmpMapperSpecifier, OmpClauseList> t;
};

// ref: 5.2: Section 5.5.11 139-141
// 2.16 declare-reduction -> DECLARE REDUCTION (reduction-identifier : type-list
//                                              : combiner) [initializer-clause]
struct OpenMPDeclareReductionConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPDeclareReductionConstruct);
  CharBlock source;
  std::tuple<Verbatim, common::Indirection<OmpReductionSpecifier>,
      std::optional<OmpClauseList>>
      t;
};

// 2.8.2 declare-simd -> DECLARE SIMD [(proc-name)] [declare-simd-clause[ [,]
//                                                   declare-simd-clause]...]
struct OpenMPDeclareSimdConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPDeclareSimdConstruct);
  CharBlock source;
  std::tuple<Verbatim, std::optional<Name>, OmpClauseList> t;
};

// 2.4 requires -> REQUIRES requires-clause[ [ [,] requires-clause]...]
struct OpenMPRequiresConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPRequiresConstruct);
  CharBlock source;
  std::tuple<Verbatim, OmpClauseList> t;
};

// 2.15.2 threadprivate -> THREADPRIVATE (variable-name-list)
struct OpenMPThreadprivate {
  TUPLE_CLASS_BOILERPLATE(OpenMPThreadprivate);
  CharBlock source;
  std::tuple<Verbatim, OmpObjectList> t;
};

// 2.11.3 allocate -> ALLOCATE (variable-name-list) [clause]
struct OpenMPDeclarativeAllocate {
  TUPLE_CLASS_BOILERPLATE(OpenMPDeclarativeAllocate);
  CharBlock source;
  std::tuple<Verbatim, OmpObjectList, OmpClauseList> t;
};

struct OpenMPDeclarativeConstruct {
  UNION_CLASS_BOILERPLATE(OpenMPDeclarativeConstruct);
  CharBlock source;
  std::variant<OpenMPDeclarativeAllocate, OpenMPDeclarativeAssumes,
      OpenMPDeclareMapperConstruct, OpenMPDeclareReductionConstruct,
      OpenMPDeclareSimdConstruct, OpenMPDeclareTargetConstruct,
      OmpDeclareVariantDirective, OpenMPThreadprivate, OpenMPRequiresConstruct,
      OpenMPUtilityConstruct, OmpMetadirectiveDirective>
      u;
};

// 2.13.2 CRITICAL [Name] <block> END CRITICAL [Name]
struct OmpCriticalDirective {
  TUPLE_CLASS_BOILERPLATE(OmpCriticalDirective);
  CharBlock source;
  std::tuple<Verbatim, std::optional<Name>, OmpClauseList> t;
};
struct OmpEndCriticalDirective {
  TUPLE_CLASS_BOILERPLATE(OmpEndCriticalDirective);
  CharBlock source;
  std::tuple<Verbatim, std::optional<Name>> t;
};
struct OpenMPCriticalConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPCriticalConstruct);
  std::tuple<OmpCriticalDirective, Block, OmpEndCriticalDirective> t;
};

// 2.11.3 allocate -> ALLOCATE [(variable-name-list)] [clause]
//        [ALLOCATE (variable-name-list) [clause] [...]]
//        allocate-statement
//        clause -> allocator-clause
struct OpenMPExecutableAllocate {
  TUPLE_CLASS_BOILERPLATE(OpenMPExecutableAllocate);
  CharBlock source;
  std::tuple<Verbatim, std::optional<OmpObjectList>, OmpClauseList,
      std::optional<std::list<OpenMPDeclarativeAllocate>>,
      Statement<AllocateStmt>>
      t;
};

// Ref: [5.2:180-181], [6.0:315]
//
// allocators-construct ->
//    ALLOCATORS [allocate-clause...]
//    block
//    [END ALLOCATORS]
struct OpenMPAllocatorsConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPAllocatorsConstruct);
  CharBlock source;
  std::tuple<OmpDirectiveSpecification, Block,
      std::optional<OmpDirectiveSpecification>>
      t;
};

// 2.17.7 Atomic construct/2.17.8 Flush construct [OpenMP 5.0]
//        memory-order-clause -> acq_rel
//                               acquire
//                               release
//                               relaxed
//                               seq_cst
struct OmpMemoryOrderClause {
  WRAPPER_CLASS_BOILERPLATE(OmpMemoryOrderClause, OmpClause);
  CharBlock source;
};

struct OpenMPAtomicConstruct {
  llvm::omp::Clause GetKind() const;
  bool IsCapture() const;
  bool IsCompare() const;
  TUPLE_CLASS_BOILERPLATE(OpenMPAtomicConstruct);
  CharBlock source;
  std::tuple<OmpDirectiveSpecification, Block,
      std::optional<OmpDirectiveSpecification>>
      t;

  // Information filled out during semantic checks to avoid duplication
  // of analyses.
  struct Analysis {
    static constexpr int None = 0;
    static constexpr int Read = 1;
    static constexpr int Write = 2;
    static constexpr int Update = Read | Write;
    static constexpr int Action = 3; // Bitmask for None, Read, Write, Update
    static constexpr int IfTrue = 4;
    static constexpr int IfFalse = 8;
    static constexpr int Condition = 12; // Bitmask for IfTrue, IfFalse

    struct Op {
      int what;
      AssignmentStmt::TypedAssignment assign;
    };
    TypedExpr atom, cond;
    Op op0, op1;
  };

  mutable Analysis analysis;
};

// OpenMP directives that associate with loop(s)
struct OmpLoopDirective {
  WRAPPER_CLASS_BOILERPLATE(OmpLoopDirective, llvm::omp::Directive);
  CharBlock source;
};

// 2.14.2 cancellation-point -> CANCELLATION POINT construct-type-clause
struct OpenMPCancellationPointConstruct {
  WRAPPER_CLASS_BOILERPLATE(
      OpenMPCancellationPointConstruct, OmpDirectiveSpecification);
  CharBlock source;
};

// 2.14.1 cancel -> CANCEL construct-type-clause [ [,] if-clause]
struct OpenMPCancelConstruct {
  WRAPPER_CLASS_BOILERPLATE(OpenMPCancelConstruct, OmpDirectiveSpecification);
  CharBlock source;
};

// Ref: [5.0:254-255], [5.1:287-288], [5.2:322-323]
//
// depobj-construct -> DEPOBJ(depend-object) depobj-clause  // since 5.0
// depobj-clause -> depend-clause |                         // until 5.2
//                  destroy-clause |
//                  update-clause
struct OpenMPDepobjConstruct {
  WRAPPER_CLASS_BOILERPLATE(OpenMPDepobjConstruct, OmpDirectiveSpecification);
  CharBlock source;
};

// Ref: [5.2: 200-201]
//
// dispatch-construct -> DISPATCH dispatch-clause
// dispatch-clause -> depend-clause |
//                    device-clause |
//                    is_device_ptr-clause |
//                    nocontext-clause |
//                    novariants-clause |
//                    nowait-clause
struct OpenMPDispatchConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPDispatchConstruct);
  CharBlock source;
  std::tuple<OmpDirectiveSpecification, Block,
      std::optional<OmpDirectiveSpecification>>
      t;
};

// [4.5:162-165], [5.0:242-246], [5.1:275-279], [5.2:315-316], [6.0:498-500]
//
// flush-construct ->
//    FLUSH [(list)]                                // since 4.5, until 4.5
// flush-construct ->
//    FLUSH [memory-order-clause] [(list)]          // since 5.0, until 5.1
// flush-construct ->
//    FLUSH [(list)] [clause-list]                  // since 5.2
//
// memory-order-clause ->                           // since 5.0, until 5.1
//    ACQ_REL | RELEASE | ACQUIRE |                 // since 5.0
//    SEQ_CST                                       // since 5.1
struct OpenMPFlushConstruct {
  WRAPPER_CLASS_BOILERPLATE(OpenMPFlushConstruct, OmpDirectiveSpecification);
  CharBlock source;
};

// Ref: [5.1:217-220], [5.2:291-292]
//
// interop -> INTEROP clause[ [ [,] clause]...]
struct OpenMPInteropConstruct {
  WRAPPER_CLASS_BOILERPLATE(OpenMPInteropConstruct, OmpDirectiveSpecification);
  CharBlock source;
};

struct OpenMPSimpleStandaloneConstruct {
  WRAPPER_CLASS_BOILERPLATE(
      OpenMPSimpleStandaloneConstruct, OmpDirectiveSpecification);
  CharBlock source;
};

struct OpenMPStandaloneConstruct {
  UNION_CLASS_BOILERPLATE(OpenMPStandaloneConstruct);
  CharBlock source;
  std::variant<OpenMPSimpleStandaloneConstruct, OpenMPFlushConstruct,
      OpenMPCancelConstruct, OpenMPCancellationPointConstruct,
      OpenMPDepobjConstruct, OmpMetadirectiveDirective, OpenMPInteropConstruct>
      u;
};

struct OmpBeginLoopDirective {
  TUPLE_CLASS_BOILERPLATE(OmpBeginLoopDirective);
  std::tuple<OmpLoopDirective, OmpClauseList> t;
  CharBlock source;
};

struct OmpEndLoopDirective {
  TUPLE_CLASS_BOILERPLATE(OmpEndLoopDirective);
  std::tuple<OmpLoopDirective, OmpClauseList> t;
  CharBlock source;
};

struct OmpBeginBlockDirective {
  TUPLE_CLASS_BOILERPLATE(OmpBeginBlockDirective);
  std::tuple<OmpBlockDirective, OmpClauseList> t;
  CharBlock source;
};

struct OmpEndBlockDirective {
  TUPLE_CLASS_BOILERPLATE(OmpEndBlockDirective);
  std::tuple<OmpBlockDirective, OmpClauseList> t;
  CharBlock source;
};

struct OpenMPBlockConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPBlockConstruct);
  std::tuple<OmpBeginBlockDirective, Block, std::optional<OmpEndBlockDirective>>
      t;
};

// OpenMP directives enclosing do loop
using NestedConstruct =
    std::variant<DoConstruct, common::Indirection<OpenMPLoopConstruct>>;
struct OpenMPLoopConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenMPLoopConstruct);
  OpenMPLoopConstruct(OmpBeginLoopDirective &&a)
      : t({std::move(a), std::nullopt, std::nullopt}) {}
  std::tuple<OmpBeginLoopDirective, std::optional<NestedConstruct>,
      std::optional<OmpEndLoopDirective>>
      t;
};

// Lookahead class to identify execution-part OpenMP constructs without
// parsing the entire OpenMP construct.
struct OpenMPExecDirective {
  WRAPPER_CLASS_BOILERPLATE(OpenMPExecDirective, OmpDirectiveName);
  CharBlock source;
};

struct OpenMPConstruct {
  UNION_CLASS_BOILERPLATE(OpenMPConstruct);
  std::variant<OpenMPStandaloneConstruct, OpenMPSectionsConstruct,
      OpenMPSectionConstruct, OpenMPLoopConstruct, OpenMPBlockConstruct,
      OpenMPAtomicConstruct, OpenMPDeclarativeAllocate, OpenMPDispatchConstruct,
      OpenMPUtilityConstruct, OpenMPExecutableAllocate,
      OpenMPAllocatorsConstruct, OpenMPAssumeConstruct, OpenMPCriticalConstruct>
      u;
};

// Parse tree nodes for OpenACC 3.3 directives and clauses

struct AccObject {
  UNION_CLASS_BOILERPLATE(AccObject);
  std::variant<Designator, /*common block*/ Name> u;
};

WRAPPER_CLASS(AccObjectList, std::list<AccObject>);

// OpenACC directive beginning or ending a block
struct AccBlockDirective {
  WRAPPER_CLASS_BOILERPLATE(AccBlockDirective, llvm::acc::Directive);
  CharBlock source;
};

struct AccLoopDirective {
  WRAPPER_CLASS_BOILERPLATE(AccLoopDirective, llvm::acc::Directive);
  CharBlock source;
};

struct AccStandaloneDirective {
  WRAPPER_CLASS_BOILERPLATE(AccStandaloneDirective, llvm::acc::Directive);
  CharBlock source;
};

// 2.11 Combined constructs
struct AccCombinedDirective {
  WRAPPER_CLASS_BOILERPLATE(AccCombinedDirective, llvm::acc::Directive);
  CharBlock source;
};

struct AccDeclarativeDirective {
  WRAPPER_CLASS_BOILERPLATE(AccDeclarativeDirective, llvm::acc::Directive);
  CharBlock source;
};

// OpenACC Clauses
struct AccBindClause {
  UNION_CLASS_BOILERPLATE(AccBindClause);
  std::variant<Name, ScalarDefaultCharExpr> u;
  CharBlock source;
};

struct AccDefaultClause {
  WRAPPER_CLASS_BOILERPLATE(AccDefaultClause, llvm::acc::DefaultValue);
  CharBlock source;
};

struct AccDataModifier {
  ENUM_CLASS(Modifier, ReadOnly, Zero)
  WRAPPER_CLASS_BOILERPLATE(AccDataModifier, Modifier);
  CharBlock source;
};

struct AccObjectListWithModifier {
  TUPLE_CLASS_BOILERPLATE(AccObjectListWithModifier);
  std::tuple<std::optional<AccDataModifier>, AccObjectList> t;
};

struct AccObjectListWithReduction {
  TUPLE_CLASS_BOILERPLATE(AccObjectListWithReduction);
  std::tuple<ReductionOperator, AccObjectList> t;
};

struct AccWaitArgument {
  TUPLE_CLASS_BOILERPLATE(AccWaitArgument);
  std::tuple<std::optional<ScalarIntExpr>, std::list<ScalarIntExpr>> t;
};

struct AccDeviceTypeExpr {
  WRAPPER_CLASS_BOILERPLATE(
      AccDeviceTypeExpr, Fortran::common::OpenACCDeviceType);
  CharBlock source;
};

struct AccDeviceTypeExprList {
  WRAPPER_CLASS_BOILERPLATE(
      AccDeviceTypeExprList, std::list<AccDeviceTypeExpr>);
};

struct AccTileExpr {
  TUPLE_CLASS_BOILERPLATE(AccTileExpr);
  CharBlock source;
  std::tuple<std::optional<ScalarIntConstantExpr>> t; // if null then *
};

struct AccTileExprList {
  WRAPPER_CLASS_BOILERPLATE(AccTileExprList, std::list<AccTileExpr>);
};

struct AccSizeExpr {
  WRAPPER_CLASS_BOILERPLATE(AccSizeExpr, std::optional<ScalarIntExpr>);
};

struct AccSizeExprList {
  WRAPPER_CLASS_BOILERPLATE(AccSizeExprList, std::list<AccSizeExpr>);
};

struct AccSelfClause {
  UNION_CLASS_BOILERPLATE(AccSelfClause);
  std::variant<std::optional<ScalarLogicalExpr>, AccObjectList> u;
  CharBlock source;
};

// num, dim, static
struct AccGangArg {
  UNION_CLASS_BOILERPLATE(AccGangArg);
  WRAPPER_CLASS(Num, ScalarIntExpr);
  WRAPPER_CLASS(Dim, ScalarIntExpr);
  WRAPPER_CLASS(Static, AccSizeExpr);
  std::variant<Num, Dim, Static> u;
  CharBlock source;
};

struct AccGangArgList {
  WRAPPER_CLASS_BOILERPLATE(AccGangArgList, std::list<AccGangArg>);
};

struct AccCollapseArg {
  TUPLE_CLASS_BOILERPLATE(AccCollapseArg);
  std::tuple<bool, ScalarIntConstantExpr> t;
};

struct AccClause {
  UNION_CLASS_BOILERPLATE(AccClause);

#define GEN_FLANG_CLAUSE_PARSER_CLASSES
#include "llvm/Frontend/OpenACC/ACC.inc"

  CharBlock source;

  std::variant<
#define GEN_FLANG_CLAUSE_PARSER_CLASSES_LIST
#include "llvm/Frontend/OpenACC/ACC.inc"
      >
      u;
};

struct AccClauseList {
  WRAPPER_CLASS_BOILERPLATE(AccClauseList, std::list<AccClause>);
  CharBlock source;
};

struct OpenACCRoutineConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCRoutineConstruct);
  CharBlock source;
  std::tuple<Verbatim, std::optional<Name>, AccClauseList> t;
};

struct OpenACCCacheConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCCacheConstruct);
  CharBlock source;
  std::tuple<Verbatim, AccObjectListWithModifier> t;
};

struct OpenACCWaitConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCWaitConstruct);
  CharBlock source;
  std::tuple<Verbatim, std::optional<AccWaitArgument>, AccClauseList> t;
};

struct AccBeginLoopDirective {
  TUPLE_CLASS_BOILERPLATE(AccBeginLoopDirective);
  std::tuple<AccLoopDirective, AccClauseList> t;
  CharBlock source;
};

struct AccBeginBlockDirective {
  TUPLE_CLASS_BOILERPLATE(AccBeginBlockDirective);
  CharBlock source;
  std::tuple<AccBlockDirective, AccClauseList> t;
};

struct AccEndBlockDirective {
  CharBlock source;
  WRAPPER_CLASS_BOILERPLATE(AccEndBlockDirective, AccBlockDirective);
};

// ACC END ATOMIC
EMPTY_CLASS(AccEndAtomic);

// ACC ATOMIC READ
struct AccAtomicRead {
  TUPLE_CLASS_BOILERPLATE(AccAtomicRead);
  std::tuple<Verbatim, AccClauseList, Statement<AssignmentStmt>,
      std::optional<AccEndAtomic>>
      t;
};

// ACC ATOMIC WRITE
struct AccAtomicWrite {
  TUPLE_CLASS_BOILERPLATE(AccAtomicWrite);
  std::tuple<Verbatim, AccClauseList, Statement<AssignmentStmt>,
      std::optional<AccEndAtomic>>
      t;
};

// ACC ATOMIC UPDATE
struct AccAtomicUpdate {
  TUPLE_CLASS_BOILERPLATE(AccAtomicUpdate);
  std::tuple<std::optional<Verbatim>, AccClauseList, Statement<AssignmentStmt>,
      std::optional<AccEndAtomic>>
      t;
};

// ACC ATOMIC CAPTURE
struct AccAtomicCapture {
  TUPLE_CLASS_BOILERPLATE(AccAtomicCapture);
  WRAPPER_CLASS(Stmt1, Statement<AssignmentStmt>);
  WRAPPER_CLASS(Stmt2, Statement<AssignmentStmt>);
  std::tuple<Verbatim, AccClauseList, Stmt1, Stmt2, AccEndAtomic> t;
};

struct OpenACCAtomicConstruct {
  UNION_CLASS_BOILERPLATE(OpenACCAtomicConstruct);
  std::variant<AccAtomicRead, AccAtomicWrite, AccAtomicCapture, AccAtomicUpdate>
      u;
  CharBlock source;
};

struct OpenACCBlockConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCBlockConstruct);
  std::tuple<AccBeginBlockDirective, Block, AccEndBlockDirective> t;
};

struct OpenACCStandaloneDeclarativeConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCStandaloneDeclarativeConstruct);
  CharBlock source;
  std::tuple<AccDeclarativeDirective, AccClauseList> t;
};

struct AccBeginCombinedDirective {
  TUPLE_CLASS_BOILERPLATE(AccBeginCombinedDirective);
  CharBlock source;
  std::tuple<AccCombinedDirective, AccClauseList> t;
};

struct AccEndCombinedDirective {
  WRAPPER_CLASS_BOILERPLATE(AccEndCombinedDirective, AccCombinedDirective);
  CharBlock source;
};

struct OpenACCCombinedConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCCombinedConstruct);
  CharBlock source;
  OpenACCCombinedConstruct(AccBeginCombinedDirective &&a)
      : t({std::move(a), std::nullopt, std::nullopt}) {}
  std::tuple<AccBeginCombinedDirective, std::optional<DoConstruct>,
      std::optional<AccEndCombinedDirective>>
      t;
};

struct OpenACCDeclarativeConstruct {
  UNION_CLASS_BOILERPLATE(OpenACCDeclarativeConstruct);
  CharBlock source;
  std::variant<OpenACCStandaloneDeclarativeConstruct, OpenACCRoutineConstruct>
      u;
};

// OpenACC directives enclosing do loop
EMPTY_CLASS(AccEndLoop);
struct OpenACCLoopConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCLoopConstruct);
  OpenACCLoopConstruct(AccBeginLoopDirective &&a)
      : t({std::move(a), std::nullopt, std::nullopt}) {}
  std::tuple<AccBeginLoopDirective, std::optional<DoConstruct>,
      std::optional<AccEndLoop>>
      t;
};

struct OpenACCEndConstruct {
  WRAPPER_CLASS_BOILERPLATE(OpenACCEndConstruct, llvm::acc::Directive);
  CharBlock source;
};

struct OpenACCStandaloneConstruct {
  TUPLE_CLASS_BOILERPLATE(OpenACCStandaloneConstruct);
  CharBlock source;
  std::tuple<AccStandaloneDirective, AccClauseList> t;
};

struct OpenACCConstruct {
  UNION_CLASS_BOILERPLATE(OpenACCConstruct);
  std::variant<OpenACCBlockConstruct, OpenACCCombinedConstruct,
      OpenACCLoopConstruct, OpenACCStandaloneConstruct, OpenACCCacheConstruct,
      OpenACCWaitConstruct, OpenACCAtomicConstruct, OpenACCEndConstruct>
      u;
};

// CUF-kernel-do-construct ->
//   !$CUF KERNEL DO [ (scalar-int-constant-expr) ]
//      <<< grid, block [, stream] >>>
//      [ cuf-reduction... ]
//      do-construct
// star-or-expr -> * | scalar-int-expr
// grid -> * | scalar-int-expr | ( star-or-expr-list )
// block -> * | scalar-int-expr | ( star-or-expr-list )
// stream -> 0, scalar-int-expr | STREAM = scalar-int-expr
// cuf-reduction -> [ REDUCE | REDUCTION ] (
//                  reduction-op : scalar-variable-list )

struct CUFReduction {
  TUPLE_CLASS_BOILERPLATE(CUFReduction);
  using Operator = ReductionOperator;
  std::tuple<Operator, std::list<Scalar<Variable>>> t;
};

struct CUFKernelDoConstruct {
  TUPLE_CLASS_BOILERPLATE(CUFKernelDoConstruct);
  WRAPPER_CLASS(StarOrExpr, std::optional<ScalarIntExpr>);
  struct LaunchConfiguration {
    TUPLE_CLASS_BOILERPLATE(LaunchConfiguration);
    std::tuple<std::list<StarOrExpr>, std::list<StarOrExpr>,
        std::optional<ScalarIntExpr>>
        t;
  };
  struct Directive {
    TUPLE_CLASS_BOILERPLATE(Directive);
    CharBlock source;
    std::tuple<std::optional<ScalarIntConstantExpr>,
        std::optional<LaunchConfiguration>, std::list<CUFReduction>>
        t;
  };
  std::tuple<Directive, std::optional<DoConstruct>> t;
};

} // namespace Fortran::parser
#endif // FORTRAN_PARSER_PARSE_TREE_H_
