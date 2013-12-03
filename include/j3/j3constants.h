#ifndef _J3_CONSTANTS_H_
#define _J3_CONSTANTS_H_

#include <stdint.h>
#include <sys/types.h>

namespace llvm {
	class Type;
}

namespace vmkit {
	class Name;
	class Names;
}

namespace j3 {
	class J3Class;
	class J3;

	class J3Cst {
	public:
		static const char* opcodeNames[];

		static const int MAGIC = 0xcafebabe;

		static wchar_t codeAttr[];
		static wchar_t constantValueAttr[];

		static wchar_t clinitName[];
		static wchar_t clinitSign[];
		static wchar_t initName[];
		static char    nativePrefix[];

		static void initialize(vmkit::Names* names);

		static const int CONSTANT_Utf8 =               1;
		static const int CONSTANT_Integer =            3;
		static const int CONSTANT_Float =              4;
		static const int CONSTANT_Long =               5;
		static const int CONSTANT_Double =             6;
		static const int CONSTANT_Class =              7;
		static const int CONSTANT_String =             8;
		static const int CONSTANT_Fieldref =           9;
		static const int CONSTANT_Methodref =          10;
		static const int CONSTANT_InterfaceMethodref = 11;
		static const int CONSTANT_NameAndType =        12;
		static const int CONSTANT_MethodHandle =       15;
		static const int CONSTANT_MethodType =         16;
		static const int CONSTANT_InvokeDynamic =      18;

#define DO_IS(name, flag_name, value)															\
		static const uint16_t flag_name = value;											\
		static bool name(uint16_t flag) { return flag & flag_name; }

		DO_IS(isPublic,        ACC_PUBLIC,       0x0001);
		DO_IS(isPrivate,       ACC_PRIVATE,      0x0002);
		DO_IS(isProtected,     ACC_PROTECTED,    0x0004);
		DO_IS(isStatic,        ACC_STATIC,       0x0008);
		DO_IS(isFinal,         ACC_FINAL,        0x0010);
		DO_IS(isSuper,         ACC_SUPER,        0x0020);
		DO_IS(isSynchronized,  ACC_SYNCHRONIZED, 0x0020);
		DO_IS(isNative,        ACC_NATIVE,       0x0100);
		DO_IS(isVolatile,      ACC_VOLATILE,     0x0040);
		DO_IS(isTransient,     ACC_TRANSIENT,    0x0080);
		DO_IS(isInterface,     ACC_INTERFACE,    0x0200);
		DO_IS(isAbstract,      ACC_ABSTRACT,     0x0400);
		DO_IS(isStrict,        ACC_STRICT,       0x0800);

#undef DO_IS

		static const wchar_t ID_Void =      'V';
		static const wchar_t ID_Byte =      'B';
		static const wchar_t ID_Char =      'C';
		static const wchar_t ID_Double =    'D';
		static const wchar_t ID_Float =     'F';
		static const wchar_t ID_Integer =   'I';
		static const wchar_t ID_Long =      'J';
		static const wchar_t ID_Classname = 'L';
		static const wchar_t ID_End =       ';';
		static const wchar_t ID_Short =     'S';
		static const wchar_t ID_Boolean =   'Z';
		static const wchar_t ID_Array =     '[';
		static const wchar_t ID_Left =      '(';
		static const wchar_t ID_Right =     ')';

		static const uint8_t T_BOOLEAN =    4;
		static const uint8_t T_CHAR =       5;
		static const uint8_t T_FLOAT =      6;
		static const uint8_t T_DOUBLE =     7;
		static const uint8_t T_BYTE =       8;
		static const uint8_t T_SHORT =      9;
		static const uint8_t T_INT =        10;
		static const uint8_t T_LONG =       11;

		static uint32_t round(uint32_t value, uint32_t bound) {
			return ((value - 1) & (-bound)) + bound;
		}

		static const vmkit::Name* rewrite(J3* vm, const vmkit::Name* clName, const vmkit::Name* orig);

#define defOpcode(ID, val, effect)											\
		static const uint8_t BC_##ID = val;
#include "j3bc.def"
#undef defOpcode
	};
}
#endif