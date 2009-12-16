//===------ VirtualTables.cpp - Virtual methods for J3 objects ------------===//
//
//                          The VMKit project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// 
// This file contains GC specific tracing functions. It is used by the
// GCMmap2 garbage collector and may be of use for other GCs. Boehm GC does
// not use these functions.
//
// The file is divided into four parts:
// (1) Declaration of internal GC classes.
// (2) Tracing Java objects: regular object, native array, object array.
// (3) Tracing a class loader, which involves tracing the Java objects
//     referenced by classes.
// (4) Tracing the roots of a program: the JVM and the threads.
//
//===----------------------------------------------------------------------===//

#include "JavaArray.h"
#include "JavaClass.h"
#include "JavaObject.h"
#include "JavaString.h"
#include "JavaThread.h"
#include "JavaUpcalls.h"
#include "Jnjvm.h"
#include "JnjvmClassLoader.h"
#include "LockedMap.h"

using namespace j3;

//===----------------------------------------------------------------------===//
// List of classes that will be GC-allocated. One should try to keep this
// list as minimal as possible, and a GC class must be defined only if
// absolutely necessary. If there is an easy way to avoid it, do it! Only
// Java classes should be GC classes.
// Having many GC classes gives more work to the GC for the scanning phase
// and for the relocation phase (for copying collectors.
//
// In JnJVM, there is only one internal gc object, the class loader.
// We decided that this was the best solution because
// otherwise it would involve hacks on the java.lang.Classloader class.
// Therefore, we create a new GC class with a finalize method that will
// delete the internal class loader when the Java object class loader is
// not reachable anymore. This also relies on the java.lang.Classloader class
// referencing an object of type VMClassLoader (this is the case in GNU
// Classpath with the vmdata field).
//===----------------------------------------------------------------------===//

VirtualTable VMClassLoader::VT((uintptr_t)VMClassLoader::staticDestructor,
                               (uintptr_t)VMClassLoader::staticDestructor,
                               (uintptr_t)VMClassLoader::staticTracer);

//===----------------------------------------------------------------------===//
// Empty tracer for static tracers of classes that do not declare static
// variables.
//===----------------------------------------------------------------------===//

extern "C" void EmptyTracer(void*) {}

//===----------------------------------------------------------------------===//
// Trace methods for Java objects. There are four types of objects:
// (1) Base object whose class is not an array: needs to trace the classloader
//     and the lock.
// (1) Object whose class is not an array: needs to trace the classloader, the
//     lock and all the virtual fields.
// (2) Object whose class is an array of objects: needs to trace root (1) and
//     all elements in the array.
// (3) Object whose class is a native array: only needs to trace the lock. The
//     classloader is the bootstrap loader and is traced by the JVM.
//===----------------------------------------------------------------------===//


/// Method for scanning the root of an object.
extern "C" void JavaObjectTracer(JavaObject* obj) {
  CommonClass* cl = obj->getClass();
  assert(cl && "No class");
  mvm::Collector::markAndTraceRoot(cl->classLoader->getJavaClassLoaderPtr());
}

/// Method for scanning an array whose elements are JavaObjects. This method is
/// called by all non-native Java arrays.
extern "C" void ArrayObjectTracer(ArrayObject* obj) {
  CommonClass* cl = obj->getClass();
  assert(cl && "No class");
  mvm::Collector::markAndTraceRoot(cl->classLoader->getJavaClassLoaderPtr());
  

  for (sint32 i = 0; i < obj->size; i++) {
    if (obj->elements[i]) {
      mvm::Collector::markAndTrace(obj, obj->elements + i);
    }
  } 
}

/// Method for scanning a native array. Only scan the lock. The classloader of
/// the class is the bootstrap loader and therefore does not need to be
/// scanned here.
extern "C" void JavaArrayTracer(JavaArray* obj) {
}

/// Method for scanning regular objects.
extern "C" void RegularObjectTracer(JavaObject* obj) {
  Class* cl = obj->getClass()->asClass();
  assert(cl && "Not a class in regular tracer");
  mvm::Collector::markAndTraceRoot(cl->classLoader->getJavaClassLoaderPtr());

  while (cl->super != 0) {
    for (uint32 i = 0; i < cl->nbVirtualFields; ++i) {
      JavaField& field = cl->virtualFields[i];
      if (field.isReference()) {
        JavaObject** ptr = field.getObjectFieldPtr(obj);
        mvm::Collector::markAndTrace(obj, ptr);
      }
    }
    cl = cl->super;
  }
}


//===----------------------------------------------------------------------===//
// Support for scanning Java objects referenced by classes. All classes must
// trace:
// (1) The classloader of the parents (super and interfaces) as well as its
//     own class loader.
// (2) The delegatee object (java.lang.Class) if it exists.
//
// Additionaly, non-primitive and non-array classes must trace:
// (3) The bytes that represent the class file.
// (4) The static instance.
//===----------------------------------------------------------------------===//


void CommonClass::tracer() {
  
  if (super && super->classLoader) {
    JavaObject** Obj = super->classLoader->getJavaClassLoaderPtr();
    if (*Obj) mvm::Collector::markAndTraceRoot(Obj);
  
    for (uint32 i = 0; i < nbInterfaces; ++i) {
      if (interfaces[i]->classLoader) {
        JavaObject** Obj = interfaces[i]->classLoader->getJavaClassLoaderPtr();
        if (*Obj) mvm::Collector::markAndTraceRoot(Obj);
      }
    }
  }

  if (classLoader)
    mvm::Collector::markAndTraceRoot(classLoader->getJavaClassLoaderPtr());

  for (uint32 i = 0; i < NR_ISOLATES; ++i) {
    // If the delegatee was static allocated, we want to trace its fields.
    if (delegatee[i]) {
      delegatee[i]->tracer();
      mvm::Collector::markAndTraceRoot(delegatee + i);
    }
  }
}

void Class::tracer() {
  CommonClass::tracer();
  if (classLoader != classLoader->bootstrapLoader)
    mvm::Collector::markAndTraceRoot(&bytes);
  
  for (uint32 i =0; i < NR_ISOLATES; ++i) {
    TaskClassMirror &M = IsolateInfo[i];
    if (M.staticInstance) {
      for (uint32 i = 0; i < nbStaticFields; ++i) {
        JavaField& field = staticFields[i];
        if (field.isReference()) {
          JavaObject** ptr = field.getObjectFieldPtr(M.staticInstance);
          mvm::Collector::markAndTraceRoot(ptr);
        }
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// Support for scanning a classloader. A classloader must trace:
// (1) All the classes it has loaded (located in the classmap).
// (2) All the class it has initiated loading and therefore references (located
//     in the classmap).
// (3) All the strings referenced in class files.
//
// The class loader does not need to trace its java.lang.Classloader Java object
// because if we end up here, this means that the Java object is already being
// scanned. Only the Java object traces the class loader.
//
// Additionaly, the bootstrap loader must trace:
// (4) The delegatees of native array classes. Since these classes are not in
//     the class map and they are not GC-allocated, we must trace the objects
//     referenced by the delegatees.
//===----------------------------------------------------------------------===//

void JnjvmClassLoader::tracer() {
  
  for (ClassMap::iterator i = classes->map.begin(), e = classes->map.end();
       i!= e; ++i) {
    CommonClass* cl = i->second;
    if (cl->isClass()) cl->asClass()->tracer();
    else cl->tracer();
  }
  
  StringList* end = strings;
  while (end) {
    for (uint32 i = 0; i < end->length; ++i) {
      JavaString** obj = end->strings + i;
      mvm::Collector::markAndTraceRoot(obj);
    }
    end = end->prev;
  }
}

void JnjvmBootstrapLoader::tracer() {
 
  JnjvmClassLoader::tracer();

#define TRACE_DELEGATEE(prim) \
  prim->tracer();

  TRACE_DELEGATEE(upcalls->OfVoid);
  TRACE_DELEGATEE(upcalls->OfBool);
  TRACE_DELEGATEE(upcalls->OfByte);
  TRACE_DELEGATEE(upcalls->OfChar);
  TRACE_DELEGATEE(upcalls->OfShort);
  TRACE_DELEGATEE(upcalls->OfInt);
  TRACE_DELEGATEE(upcalls->OfFloat);
  TRACE_DELEGATEE(upcalls->OfLong);
  TRACE_DELEGATEE(upcalls->OfDouble);
#undef TRACE_DELEGATEE
}

//===----------------------------------------------------------------------===//
// Support for scanning the roots of a program: JVM and threads. The JVM
// must trace:
// (1) The bootstrap class loader: where core classes live.
// (2) The applicative class loader: the JVM may be the ony one referencing it.
// (3) Global references from JNI.
//
// The threads must trace:
// (1) Their stack (already done by the GC in the case of GCMmap2 or Boehm)
// (2) Their pending exception if there is one.
// (3) The java.lang.Thread delegate.
//===----------------------------------------------------------------------===//


void Jnjvm::tracer() {
  
  VirtualMachine::tracer();
  bootstrapLoader->tracer();
  
  if (appClassLoader) 
    mvm::Collector::markAndTraceRoot(appClassLoader->getJavaClassLoaderPtr());
  
  JNIGlobalReferences* start = &globalRefs;
  while (start) {
    for (uint32 i = 0; i < start->length; ++i) {
      JavaObject** obj = start->globalReferences + i;
      mvm::Collector::markAndTraceRoot(obj);
    }
    start = start->next;
  }
  
  for (StringMap::iterator i = hashStr.map.begin(), e = hashStr.map.end();
       i!= e; ++i) {
    JavaString** str = &(i->second);
    mvm::Collector::markAndTraceRoot(str);
  }

#if defined(ISOLATE_SHARING)
  mvm::Collector::markAndTraceRoot(&JnjvmSharedLoader::sharedLoader);
#endif
  
#ifdef SERVICE
  parent->tracer();
#endif
}

void JavaThread::tracer() {
  if (pendingException) mvm::Collector::markAndTraceRoot(&pendingException);
  mvm::Collector::markAndTraceRoot(&javaThread);
#ifdef SERVICE
  mvm::Collector::markAndTraceRoot(&ServiceException);
#endif
  
  JNILocalReferences* end = localJNIRefs;
  while (end) {
    for (uint32 i = 0; i < end->length; ++i) {
      JavaObject** obj = end->localReferences + i;
      mvm::Collector::markAndTraceRoot(obj);
    }
    end = end->prev;
  }
}
