/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Header file for the Virtual DataBase Engine (VDBE)
**
** This header defines the interface to the virtual database engine
** or VDBE.  The VDBE implements an abstract machine that runs a
** simple program to access and modify the underlying database.
**
** $Id: vdbe.h,v 1.41 2002/02/03 03:34:09 drh Exp $
*/
#ifndef _SQLITE_VDBE_H_
#define _SQLITE_VDBE_H_
#include <stdio.h>

/*
** A single VDBE is an opaque structure named "Vdbe".  Only routines
** in the source file sqliteVdbe.c are allowed to see the insides
** of this structure.
*/
typedef struct Vdbe Vdbe;

/*
** A single instruction of the virtual machine has an opcode
** and as many as three operands.  The instruction is recorded
** as an instance of the following structure:
*/
struct VdbeOp {
  int opcode;         /* What operation to perform */
  int p1;             /* First operand */
  int p2;             /* Second parameter (often the jump destination) */
  char *p3;           /* Third parameter */
  int p3type;         /* P3_STATIC, P3_DYNAMIC or P3_POINTER */
};
typedef struct VdbeOp VdbeOp;

/*
** Allowed values of VdbeOp.p3type
*/
#define P3_NOTUSED    0   /* The P3 parameter is not used */
#define P3_DYNAMIC    1   /* Pointer to a string obtained from sqliteMalloc() */
#define P3_STATIC   (-1)  /* Pointer to a static string */
#define P3_POINTER  (-2)  /* P3 is a pointer to some structure or object */

/*
** The following macro converts a relative address in the p2 field
** of a VdbeOp structure into a negative number so that 
** sqliteVdbeAddOpList() knows that the address is relative.  Calling
** the macro again restores the address.
*/
#define ADDR(X)  (-1-(X))

/*
** These are the available opcodes.
**
** If any of the values changes or if opcodes are added or removed,
** be sure to also update the zOpName[] array in sqliteVdbe.c to
** mirror the change.
**
** The source tree contains an AWK script named renumberOps.awk that
** can be used to renumber these opcodes when new opcodes are inserted.
*/
#define OP_Transaction         1
#define OP_Checkpoint          2
#define OP_Commit              3
#define OP_Rollback            4

#define OP_ReadCookie          5
#define OP_SetCookie           6
#define OP_VerifyCookie        7

#define OP_Open                8
#define OP_OpenTemp            9
#define OP_OpenWrite          10
#define OP_OpenAux            11
#define OP_OpenWrAux          12
#define OP_Close              13
#define OP_MoveTo             14
#define OP_NewRecno           15
#define OP_PutIntKey          16
#define OP_PutStrKey          17
#define OP_Distinct           18
#define OP_Found              19
#define OP_NotFound           20
#define OP_IsUnique           21
#define OP_NotExists          22
#define OP_Delete             23
#define OP_Column             24
#define OP_KeyAsData          25
#define OP_Recno              26
#define OP_FullKey            27
#define OP_Rewind             28
#define OP_Next               29

#define OP_Destroy            30
#define OP_Clear              31
#define OP_CreateIndex        32
#define OP_CreateTable        33
#define OP_SanityCheck        34

#define OP_IdxPut             35
#define OP_IdxDelete          36
#define OP_IdxRecno           37
#define OP_IdxGT              38
#define OP_IdxGE              39

#define OP_MemLoad            40
#define OP_MemStore           41

#define OP_ListWrite          42
#define OP_ListRewind         43
#define OP_ListRead           44
#define OP_ListReset          45

#define OP_SortPut            46
#define OP_SortMakeRec        47
#define OP_SortMakeKey        48
#define OP_Sort               49
#define OP_SortNext           50
#define OP_SortCallback       51
#define OP_SortReset          52

#define OP_FileOpen           53
#define OP_FileRead           54
#define OP_FileColumn         55

#define OP_AggReset           56
#define OP_AggFocus           57
#define OP_AggIncr            58
#define OP_AggNext            59
#define OP_AggSet             60
#define OP_AggGet             61

#define OP_SetInsert          62
#define OP_SetFound           63
#define OP_SetNotFound        64

#define OP_MakeRecord         65
#define OP_MakeKey            66
#define OP_MakeIdxKey         67
#define OP_IncrKey            68

#define OP_Goto               69
#define OP_If                 70
#define OP_Halt               71

#define OP_ColumnCount        72
#define OP_ColumnName         73
#define OP_Callback           74
#define OP_NullCallback       75

#define OP_Integer            76
#define OP_String             77
#define OP_Pop                78
#define OP_Dup                79
#define OP_Pull               80
#define OP_Push               81
#define OP_MustBeInt          82

#define OP_Add                83
#define OP_AddImm             84
#define OP_Subtract           85
#define OP_Multiply           86
#define OP_Divide             87
#define OP_Remainder          88
#define OP_BitAnd             89
#define OP_BitOr              90
#define OP_BitNot             91
#define OP_ShiftLeft          92
#define OP_ShiftRight         93
#define OP_AbsValue           94
#define OP_Precision          95
#define OP_Min                96
#define OP_Max                97
#define OP_Like               98
#define OP_Glob               99
#define OP_Eq                100
#define OP_Ne                101
#define OP_Lt                102
#define OP_Le                103
#define OP_Gt                104
#define OP_Ge                105
#define OP_IsNull            106
#define OP_NotNull           107
#define OP_Negative          108
#define OP_And               109
#define OP_Or                110
#define OP_Not               111
#define OP_Concat            112
#define OP_Noop              113

#define OP_Strlen            114
#define OP_Substr            115

#define OP_Limit             116

#define OP_MAX               116

/*
** Prototypes for the VDBE interface.  See comments on the implementation
** for a description of what each of these routines does.
*/
Vdbe *sqliteVdbeCreate(sqlite*);
void sqliteVdbeCreateCallback(Vdbe*, int*);
int sqliteVdbeAddOp(Vdbe*,int,int,int);
int sqliteVdbeAddOpList(Vdbe*, int nOp, VdbeOp const *aOp);
void sqliteVdbeChangeP1(Vdbe*, int addr, int P1);
void sqliteVdbeChangeP2(Vdbe*, int addr, int P2);
void sqliteVdbeChangeP3(Vdbe*, int addr, const char *zP1, int N);
void sqliteVdbeDequoteP3(Vdbe*, int addr);
int sqliteVdbeMakeLabel(Vdbe*);
void sqliteVdbeDelete(Vdbe*);
int sqliteVdbeOpcode(const char *zName);
int sqliteVdbeExec(Vdbe*,sqlite_callback,void*,char**,void*,
                   int(*)(void*,const char*,int));
int sqliteVdbeList(Vdbe*,sqlite_callback,void*,char**);
void sqliteVdbeResolveLabel(Vdbe*, int);
int sqliteVdbeCurrentAddr(Vdbe*);
void sqliteVdbeTrace(Vdbe*,FILE*);
void sqliteVdbeCompressSpace(Vdbe*,int);

#endif
