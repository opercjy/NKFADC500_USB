// Do NOT change. Changes will be lost next time file is generated

#define R__DICTIONARY_FILENAME usb3comrootDict
#define R__NO_DEPRECATION

/*******************************************************************/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define G__DICTIONARY
#include "ROOT/RConfig.hxx"
#include "TClass.h"
#include "TDictAttributeMap.h"
#include "TInterpreter.h"
#include "TROOT.h"
#include "TBuffer.h"
#include "TMemberInspector.h"
#include "TInterpreter.h"
#include "TVirtualMutex.h"
#include "TError.h"

#ifndef G__ROOT
#define G__ROOT
#endif

#include "RtypesImp.h"
#include "TIsAProxy.h"
#include "TFileMergeInfo.h"
#include <algorithm>
#include "TCollectionProxyInfo.h"
/*******************************************************************/

#include "TDataMember.h"

// Header files passed as explicit arguments
#include "usb3comroot.h"

// Header files passed via #pragma extra_include

// The generated code does not explicitly qualify STL entities
namespace std {} using namespace std;

namespace ROOT {
   static void *new_usb3comroot(void *p = nullptr);
   static void *newArray_usb3comroot(Long_t size, void *p);
   static void delete_usb3comroot(void *p);
   static void deleteArray_usb3comroot(void *p);
   static void destruct_usb3comroot(void *p);
   static void streamer_usb3comroot(TBuffer &buf, void *obj);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::usb3comroot*)
   {
      ::usb3comroot *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::usb3comroot >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("usb3comroot", ::usb3comroot::Class_Version(), "usb3comroot.h", 14,
                  typeid(::usb3comroot), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::usb3comroot::Dictionary, isa_proxy, 18,
                  sizeof(::usb3comroot) );
      instance.SetNew(&new_usb3comroot);
      instance.SetNewArray(&newArray_usb3comroot);
      instance.SetDelete(&delete_usb3comroot);
      instance.SetDeleteArray(&deleteArray_usb3comroot);
      instance.SetDestructor(&destruct_usb3comroot);
      instance.SetStreamerFunc(&streamer_usb3comroot);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::usb3comroot*)
   {
      return GenerateInitInstanceLocal(static_cast<::usb3comroot*>(nullptr));
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal(static_cast<const ::usb3comroot*>(nullptr)); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

//______________________________________________________________________________
atomic_TClass_ptr usb3comroot::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *usb3comroot::Class_Name()
{
   return "usb3comroot";
}

//______________________________________________________________________________
const char *usb3comroot::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::usb3comroot*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int usb3comroot::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::usb3comroot*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *usb3comroot::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::usb3comroot*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *usb3comroot::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::usb3comroot*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
void usb3comroot::Streamer(TBuffer &R__b)
{
   // Stream an object of class usb3comroot.

   TObject::Streamer(R__b);
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_usb3comroot(void *p) {
      return  p ? new(p) ::usb3comroot : new ::usb3comroot;
   }
   static void *newArray_usb3comroot(Long_t nElements, void *p) {
      return p ? new(p) ::usb3comroot[nElements] : new ::usb3comroot[nElements];
   }
   // Wrapper around operator delete
   static void delete_usb3comroot(void *p) {
      delete (static_cast<::usb3comroot*>(p));
   }
   static void deleteArray_usb3comroot(void *p) {
      delete [] (static_cast<::usb3comroot*>(p));
   }
   static void destruct_usb3comroot(void *p) {
      typedef ::usb3comroot current_t;
      (static_cast<current_t*>(p))->~current_t();
   }
   // Wrapper around a custom streamer member function.
   static void streamer_usb3comroot(TBuffer &buf, void *obj) {
      ((::usb3comroot*)obj)->::usb3comroot::Streamer(buf);
   }
} // end of namespace ROOT for class ::usb3comroot

namespace {
  void TriggerDictionaryInitialization_usb3comrootDict_Impl() {
    static const char* headers[] = {
"usb3comroot.h",
nullptr
    };
    static const char* includePaths[] = {
"/usr/local/notice/include",
"/home/kds/Software/root_build_6.32.00/include/",
"/usr/local/notice/src/usb3com/usb3comroot/",
nullptr
    };
    static const char* fwdDeclCode = R"DICTFWDDCLS(
#line 1 "usb3comrootDict dictionary forward declarations' payload"
#pragma clang diagnostic ignored "-Wkeyword-compat"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern int __Cling_AutoLoading_Map;
class __attribute__((annotate("$clingAutoload$usb3comroot.h")))  usb3comroot;
)DICTFWDDCLS";
    static const char* payloadCode = R"DICTPAYLOAD(
#line 1 "usb3comrootDict dictionary payload"


#define _BACKWARD_BACKWARD_WARNING_H
// Inline headers
#include "usb3comroot.h"

#undef  _BACKWARD_BACKWARD_WARNING_H
)DICTPAYLOAD";
    static const char* classesHeaders[] = {
"usb3comroot", payloadCode, "@",
nullptr
};
    static bool isInitialized = false;
    if (!isInitialized) {
      TROOT::RegisterModule("usb3comrootDict",
        headers, includePaths, payloadCode, fwdDeclCode,
        TriggerDictionaryInitialization_usb3comrootDict_Impl, {}, classesHeaders, /*hasCxxModule*/false);
      isInitialized = true;
    }
  }
  static struct DictInit {
    DictInit() {
      TriggerDictionaryInitialization_usb3comrootDict_Impl();
    }
  } __TheDictionaryInitializer;
}
void TriggerDictionaryInitialization_usb3comrootDict() {
  TriggerDictionaryInitialization_usb3comrootDict_Impl();
}
