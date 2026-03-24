/*
 *
 */
#ifndef NKUSBROOTROOT_H
#define NKUSBROOTROOT_H

#include "TObject.h"
#include "nkusbconst.h"

struct libusb_context;
struct libusb_device;
struct libusb_device_handle;

/*class nkusb_context : public TObject {
public:
   nkusb_context(libusb_context *ctx) {context = ctx;}
   operator libusb_context* (){ return context;}
private:
   libusb_context *context;

   ClassDef(nkusb_context, 0) // libusb_context wrapper class for root
};

class nkusb_device : public TObject {
public:
   nkusb_device(libusb_device *dev) {device = dev;}
   operator libusb_device* (){ return device;}
private:
   libusb_device *device;

   ClassDef(nkusb_device, 0) // libusb_device wrapper class for root
};

class nkusb_device_handle : public TObject {
public:
   nkusb_device_handle(libusb_device_handle *devh) {device_handle = devh;}
   operator libusb_device_handle* (){ return device_handle;}
private:
   libusb_device_handle *device_handle;

   ClassDef(nkusb_device_handle, 0) // libusb_device_handle wrapper class for root
};*/

class NKUSBROOT : public TObject {
public:

   NKUSBROOT(libusb_context **ctx = 0);
   virtual ~NKUSBROOT();
   //Int_t init(libusb_context **ctx = 0);
   virtual Int_t open(UShort_t vendor_id, UShort_t product_id, Int_t sid = NK_SID_ANY, libusb_context *ctx = 0);
   virtual Int_t claimInterface(UShort_t vendor_id, UShort_t product_id, Int_t sid = NK_SID_ANY, Int_t interface = 0);
   virtual Int_t releaseInterface(UShort_t vendor_id, UShort_t product_id, Int_t sid = NK_SID_ANY, Int_t interface = 0);
   virtual void close(UShort_t vendor_id, UShort_t product_id, Int_t sid = NK_SID_ANY);
   virtual void exit(libusb_context *ctx = 0);
   virtual void printOpenDevices();
   virtual Int_t isDeviceOpen(UShort_t vendor_id, UShort_t product_id, Int_t sid = NK_SID_ANY);

   ClassDef(NKUSBROOT, 0) // nkusb wrapper class for root
};

#endif
