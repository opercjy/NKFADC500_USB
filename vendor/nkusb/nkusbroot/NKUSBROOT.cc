/*
 *
 */
#include "NKUSBROOT.h"
#include "nkusb.h"
#include "libusb.h"

ClassImp(NKUSBROOT)


///////////////////////////////////////////////////////////////////////////////
NKUSBROOT::NKUSBROOT(libusb_context **ctx)
{
   ::nkusb_init(ctx);
}

///////////////////////////////////////////////////////////////////////////////
NKUSBROOT::~NKUSBROOT()
{
}

///////////////////////////////////////////////////////////////////////////////
//Int_t NKUSBROOT::init(libusb_context **ctx)
//{
//   return ::nkusb_init(ctx);
//}

///////////////////////////////////////////////////////////////////////////////
Int_t NKUSBROOT::open(UShort_t vendor_id, UShort_t product_id, Int_t sid, libusb_context *ctx)
{
   return ::nkusb_open_device(vendor_id, product_id, sid, ctx);
}

///////////////////////////////////////////////////////////////////////////////
Int_t NKUSBROOT::claimInterface(UShort_t vendor_id, UShort_t product_id, Int_t sid, Int_t interface){
   return ::nkusb_claim_interface(vendor_id, product_id, sid, interface);
}

///////////////////////////////////////////////////////////////////////////////
Int_t NKUSBROOT::releaseInterface(UShort_t vendor_id, UShort_t product_id, Int_t sid, Int_t interface)
{
   return ::nkusb_release_interface(vendor_id, product_id, sid, interface);
}

///////////////////////////////////////////////////////////////////////////////
void NKUSBROOT::close(UShort_t vendor_id, UShort_t product_id, Int_t sid)
{
   ::nkusb_close_device(vendor_id, product_id, sid);
}

///////////////////////////////////////////////////////////////////////////////
void NKUSBROOT::exit(libusb_context *ctx)
{
   ::nkusb_exit(ctx);
}

///////////////////////////////////////////////////////////////////////////////
void NKUSBROOT::printOpenDevices()
{
   ::nkusb_print_open_devices();
}

///////////////////////////////////////////////////////////////////////////////
Int_t NKUSBROOT::isDeviceOpen(UShort_t vendor_id, UShort_t product_id, Int_t sid)
{
   return ::nkusb_is_device_open(vendor_id, product_id, sid);
}
