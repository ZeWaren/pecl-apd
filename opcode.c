/* 
   +----------------------------------------------------------------------+
   | APD Profiler & Debugger
   +----------------------------------------------------------------------+
   | Copyright (c) 2001-2003 Community Connect Inc.
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
   |          George Schlossnagle <george@lethargy.org>                   |
   +----------------------------------------------------------------------+
*/

#include "opcode.h"
#include <assert.h>

static const char* opcodes[] = {
	"ZEND_NOP", // 0
	"ZEND_ADD", // 1
	"ZEND_SUB", // 2
	"ZEND_MUL", // 3
	"ZEND_DIV", // 4
	"ZEND_MOD", // 5
	"ZEND_SL", // 6
	"ZEND_SR", // 7
	"ZEND_CONCAT", // 8
	"ZEND_BW_OR", // 9
	"ZEND_BW_AND", // 10
	"ZEND_BW_XOR", // 11
	"ZEND_BW_NOT", // 12
	"ZEND_BOOL_NOT", // 13
	"ZEND_BOOL_XOR", // 14
	"ZEND_IS_IDENTICAL", // 15
	"ZEND_IS_NOT_IDENTICAL", // 16
	"ZEND_IS_EQUAL", // 17
	"ZEND_IS_NOT_EQUAL", // 18
	"ZEND_IS_SMALLER", // 19
	"ZEND_IS_SMALLER_OR_EQUAL", // 20
	"ZEND_CAST", // 21
	"ZEND_QM_ASSIGN", // 22
	"ZEND_ASSIGN_ADD", // 23
	"ZEND_ASSIGN_SUB", // 24
	"ZEND_ASSIGN_MUL", // 25
	"ZEND_ASSIGN_DIV", // 26
	"ZEND_ASSIGN_MOD", // 27
	"ZEND_ASSIGN_SL", // 28
	"ZEND_ASSIGN_SR", // 29
	"ZEND_ASSIGN_CONCAT", // 30
	"ZEND_ASSIGN_BW_OR", // 31
	"ZEND_ASSIGN_BW_AND", // 32
	"ZEND_ASSIGN_BW_XOR", // 33
	"ZEND_PRE_INC", // 34
	"ZEND_PRE_DEC", // 35
	"ZEND_POST_INC", // 36
	"ZEND_POST_DEC", // 37
	"ZEND_ASSIGN", // 38
	"ZEND_ASSIGN_REF", // 39
	"ZEND_ECHO", // 40
	"ZEND_PRINT", // 41
	"ZEND_JMP", // 42
	"ZEND_JMPZ", // 43
	"ZEND_JMPNZ", // 44
	"ZEND_JMPZNZ", // 45
	"ZEND_JMPZ_EX", // 46
	"ZEND_JMPNZ_EX", // 47
	"ZEND_CASE", // 48
	"ZEND_SWITCH_FREE", // 49
	"ZEND_BRK", // 50
	"ZEND_CONT", // 51
	"ZEND_BOOL", // 52
	"ZEND_INIT_STRING", // 53
	"ZEND_ADD_CHAR", // 54
	"ZEND_ADD_STRING", // 55
	"ZEND_ADD_VAR", // 56
	"ZEND_BEGIN_SILENCE", // 57
	"ZEND_END_SILENCE", // 58
	"ZEND_INIT_FCALL_BY_NAME", // 59
	"ZEND_DO_FCALL", // 60
	"ZEND_DO_FCALL_BY_NAME", // 61
	"ZEND_RETURN", // 62
	"ZEND_RECV", // 63
	"ZEND_RECV_INIT", // 64
	"ZEND_SEND_VAL", // 65
	"ZEND_SEND_VAR", // 66
	"ZEND_SEND_REF", // 67
	"ZEND_NEW", // 68
	"ZEND_JMP_NO_CTOR", // 69
	"ZEND_FREE", // 70
	"ZEND_INIT_ARRAY", // 71
	"ZEND_ADD_ARRAY_ELEMENT", // 72
	"ZEND_INCLUDE_OR_EVAL", // 73
	"ZEND_UNSET_VAR", // 74
	"ZEND_UNSET_DIM_OBJ", // 75
	"ZEND_ISSET_ISEMPTY", // 76
	"ZEND_FE_RESET", // 77
	"ZEND_FE_FETCH", // 78
	"ZEND_EXIT", // 79
	"ZEND_FETCH_R", // 80
	"ZEND_FETCH_DIM_R", // 81
	"ZEND_FETCH_OBJ_R", // 82
	"ZEND_FETCH_W", // 83
	"ZEND_FETCH_DIM_W", // 84
	"ZEND_FETCH_OBJ_W", // 85
	"ZEND_FETCH_RW", // 86
	"ZEND_FETCH_DIM_RW", // 87
	"ZEND_FETCH_OBJ_RW", // 88
	"ZEND_FETCH_IS", // 89
	"ZEND_FETCH_DIM_IS", // 90
	"ZEND_FETCH_OBJ_IS", // 91
	"ZEND_FETCH_FUNC_ARG", // 92
	"ZEND_FETCH_DIM_FUNC_ARG", // 93
	"ZEND_FETCH_OBJ_FUNC_ARG", // 94
	"ZEND_FETCH_UNSET", // 95
	"ZEND_FETCH_DIM_UNSET", // 96
	"ZEND_FETCH_OBJ_UNSET", // 97
	"ZEND_FETCH_DIM_TMP_VAR", // 98
	"ZEND_FETCH_CONSTANT", // 99
	"ZEND_DECLARE_FUNCTION_OR_CLASS", // 100
	"ZEND_EXT_STMT", // 101
	"ZEND_EXT_FCALL_BEGIN", // 102
	"ZEND_EXT_FCALL_END", // 103
	"ZEND_EXT_NOP", // 104
	"ZEND_TICKS", // 105
	"ZEND_SEND_VAR_NO_REF", // 106
};

static const int NUM_OPCODES = sizeof(opcodes) / sizeof(opcodes[0]);

const char* getOpcodeName(int op)
{
	if (op < 0 || op >= NUM_OPCODES) {
		return "(unknown)";
	}
	return opcodes[op];
}

/**
 * Local Variables:
 * indent-tabs-mode: t
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim600:fdm=marker
 * vim:noet:sw=4:ts=4
 */
