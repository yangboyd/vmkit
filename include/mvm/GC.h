//===----------- GC.h - Garbage Collection Interface -----------------------===//
//
//                     The Micro Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#ifndef _GC_H_
#define _GC_H_

#include <stdint.h>
#include "ObjectHeader.h"

namespace mvm {

	class VirtualMachine;
	class VirtualTable;
	class gc;

	class gcRoot {
	public:
		virtual           ~gcRoot() {}
		virtual void      tracer(uintptr_t closure) {}
		uintptr_t header;
  
		/// getVirtualTable - Returns the virtual table of this object.
		///
		VirtualTable* getVirtualTable() const {
			return ((VirtualTable**)(this))[0];
		}
  
		/// setVirtualTable - Sets the virtual table of this object.
		///
		void setVirtualTable(VirtualTable* VT) {
			((VirtualTable**)(this))[0] = VT;
		}
	};

	// WARNING: if you modify this class, you must also change the generation of VT in JavaAOTCompiler
	class CommonVirtualTable {
	public:
		uintptr_t       destructor;
		uintptr_t       operatorDelete;
		uintptr_t       tracer;
		VirtualMachine* vm;

		uintptr_t* getFunctions() {
			return &destructor;
		}

		static int numberOfCommonEntries() { return 4; }

		static void emptyTracer(void*) {}
	};

} // namespace mvm

#include "MvmGC.h"

namespace mvm {
	class gc : public collectable {
	};

	class VirtualTable : public GCVirtualTable {};

  static const uint32_t HashBits = 8;
  static const uint64_t GCBitMask = ((1 << GCBits) - 1);
} // namespace mvm

#endif