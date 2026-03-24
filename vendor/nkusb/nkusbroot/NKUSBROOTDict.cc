// Do NOT change. Changes will be lost next time file is generated

#define R__DICTIONARY_FILENAME NKUSBROOTDict
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
#include "NKUSBROOT.h"

// Header files passed via #pragma extra_include

// The generated code does not explicitly qualify STL entities
namespace std {} using namespace std;

namespace ROOT {
   static void *new_NKUSBROOT(void *p = nullptr);
   static void *newArray_NKUSBROOT(Long_t size, void *p);
   static void delete_NKUSBROOT(void *p);
   static void deleteArray_NKUSBROOT(void *p);
   static void destruct_NKUSBROOT(void *p);
   static void streamer_NKUSBROOT(TBuffer &buf, void *obj);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::NKUSBROOT*)
   {
      ::NKUSBROOT *ptr = nullptr;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::NKUSBROOT >(nullptr);
      static ::ROOT::TGenericClassInfo 
         instance("NKUSBROOT", ::NKUSBROOT::Class_Version(), "NKUSBROOT.h", 44,
                  typeid(::NKUSBROOT), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::NKUSBROOT::Dictionary, isa_proxy, 18,
                  sizeof(::NKUSBROOT) );
      instance.SetNew(&new_NKUSBROOT);
      instance.SetNewArray(&newArray_NKUSBROOT);
      instance.SetDelete(&delete_NKUSBROOT);
      instance.SetDeleteArray(&deleteArray_NKUSBROOT);
      instance.SetDestructor(&destruct_NKUSBROOT);
      instance.SetStreamerFunc(&streamer_NKUSBROOT);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::NKUSBROOT*)
   {
      return GenerateInitInstanceLocal(static_cast<::NKUSBROOT*>(nullptr));
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal(static_cast<const ::NKUSBROOT*>(nullptr)); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

//______________________________________________________________________________
atomic_TClass_ptr NKUSBROOT::fgIsA(nullptr);  // static to hold class pointer

//______________________________________________________________________________
const char *NKUSBROOT::Class_Name()
{
   return "NKUSBROOT";
}

//______________________________________________________________________________
const char *NKUSBROOT::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::NKUSBROOT*)nullptr)->GetImplFileName();
}

//______________________________________________________________________________
int NKUSBROOT::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::NKUSBROOT*)nullptr)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *NKUSBROOT::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::NKUSBROOT*)nullptr)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *NKUSBROOT::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::NKUSBROOT*)nullptr)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
void NKUSBROOT::Streamer(TBuffer &R__b)
{
   // Stream an object of class NKUSBROOT.

   TObject::Streamer(R__b);
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_NKUSBROOT(void *p) {
      return  p ? new(p) ::NKUSBROOT : new ::NKUSBROOT;
   }
   static void *newArray_NKUSBROOT(Long_t nElements, void *p) {
      return p ? new(p) ::NKUSBROOT[nElements] : new ::NKUSBROOT[nElements];
   }
   // Wrapper around operator delete
   static void delete_NKUSBROOT(void *p) {
      delete (static_cast<::NKUSBROOT*>(p));
   }
   static void deleteArray_NKUSBROOT(void *p) {
      delete [] (static_cast<::NKUSBROOT*>(p));
   }
   static void destruct_NKUSBROOT(void *p) {
      typedef ::NKUSBROOT current_t;
      (static_cast<current_t*>(p))->~current_t();
   }
   // Wrapper around a custom streamer member function.
   static void streamer_NKUSBROOT(TBuffer &buf, void *obj) {
      ((::NKUSBROOT*)obj)->::NKUSBROOT::Streamer(buf);
   }
} // end of namespace ROOT for class ::NKUSBROOT

namespace {
  void TriggerDictionaryInitialization_NKUSBROOTDict_Impl() {
    static const char* headers[] = {
"NKUSBROOT.h",
nullptr
    };
    static const char* includePaths[] = {
"/usr/local/notice/include",
"/home/kds/Software/root_build_6.32.00/include/",
"/usr/local/notice/src/nkusb/nkusbroot/",
nullptr
    };
    static const char* fwdDeclCode = R"DICTFWDDCLS(
#line 1 "NKUSBROOTDict dictionary forward declarations' payload"
#pragma clang diagnostic ignored "-Wkeyword-compat"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern int __Cling_AutoLoading_Map;
class __attribute__((annotate("$clingAutoload$NKUSBROOT.h")))  NKUSBROOT;
)DICTFWDDCLS";
    static const char* payloadCode = R"DICTPAYLOAD(
#line 1 "NKUSBROOTDict dictionary payload"


#define _BACKWARD_BACKWARD_WARNING_H
// Inline headers
#include "NKUSBROOT.h"

#undef  _BACKWARD_BACKWARD_WARNING_H
)DICTPAYLOAD";
    static const char* classesHeaders[] = {
"NKUSBROOT", payloadCode, "@",
nullptr
};
    static bool isInitialized = false;
    if (!isInitialized) {
      TROOT::RegisterModule("NKUSBROOTDict",
        headers, includePaths, payloadCode, fwdDeclCode,
        TriggerDictionaryInitialization_NKUSBROOTDict_Impl, {}, classesHeaders, /*hasCxxModule*/false);
      isInitialized = true;
    }
  }
  static struct DictInit {
    DictInit() {
      TriggerDictionaryInitialization_NKUSBROOTDict_Impl();
    }
  } __TheDictionaryInitializer;
}
void TriggerDictionaryInitialization_NKUSBROOTDict() {
  TriggerDictionaryInitialization_NKUSBROOTDict_Impl();
}
