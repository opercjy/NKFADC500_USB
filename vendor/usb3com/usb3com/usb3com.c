#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nkusb.h"
#include "usb3com.h"

#define NBURST (16)

///////////////////////////////////////////////////////////////////////////////
int USB3Init(libusb_context **ctx)
{
  return nkusb_init(ctx);
}

///////////////////////////////////////////////////////////////////////////////
int USB3Open(uint16_t vendor_id, uint16_t product_id, unsigned char sid, libusb_context *ctx) 
{
  return nkusb_open_device(vendor_id, product_id, sid, ctx);
}

///////////////////////////////////////////////////////////////////////////////
void USB3PrintOpenDevices(void) 
{
  nkusb_print_open_devices();
}

///////////////////////////////////////////////////////////////////////////////
int USB3ClaimInterface(uint16_t vendor_id, uint16_t product_id, unsigned char sid, int interface) 
{
  return nkusb_claim_interface(vendor_id, product_id, sid, interface);
}

///////////////////////////////////////////////////////////////////////////////
int USB3ReleaseInterface(uint16_t vendor_id, uint16_t product_id, unsigned char sid, int interface) 
{
  return nkusb_release_interface(vendor_id, product_id, sid, interface);
}
  
///////////////////////////////////////////////////////////////////////////////
void USB3Close(uint16_t vendor_id, uint16_t product_id, unsigned char sid) 
{
  nkusb_close_device(vendor_id, product_id, sid);
}

///////////////////////////////////////////////////////////////////////////////
void USB3Exit(libusb_context *ctx)
{ 
  nkusb_exit(ctx);
}

///////////////////////////////////////////////////////////////////////////////
void USB3Reset(uint16_t vendor_id, uint16_t product_id, unsigned char sid)
{
  unsigned char data;

  USB3WriteControl(vendor_id, product_id, sid, 0xD6, 0, 0, &data, 0);
}

///////////////////////////////////////////////////////////////////////////////
int USB3Write(uint16_t vendor_id, uint16_t product_id, unsigned char sid, uint32_t addr, uint32_t data)
{
  int transferred = 0;  
  const unsigned int timeout = 1000;
  int length = 8;
  unsigned char *buffer;
  int stat = 0;
  
  if (!(buffer = (unsigned char *)malloc(length))) {
    fprintf(stderr, "USB3Write: Could not allocate memory (size = %d\n)", length);
    return -1;
  }
  
  buffer[0] = data & 0xFF;
  buffer[1] = (data >> 8)  & 0xFF;
  buffer[2] = (data >> 16)  & 0xFF;
  buffer[3] = (data >> 24)  & 0xFF;
  
  buffer[4] = addr & 0xFF;
  buffer[5] = (addr >> 8)  & 0xFF;
  buffer[6] = (addr >> 16)  & 0xFF;
  buffer[7] = (addr >> 24)  & 0x7F;
  
  libusb_device_handle *devh = nkusb_get_device_handle(vendor_id, product_id, sid);
  if (!devh) {
    fprintf(stderr, "USB3Write: Could not get device handle for the device.\n");
    return -1;
  }
  
  if ((stat = libusb_bulk_transfer(devh, USB3_SF_WRITE, buffer, length, &transferred, timeout)) < 0) {
    fprintf(stderr, "USB3Write: Could not make write request; error = %d\n", stat);
    USB3Reset(vendor_id, product_id, sid);
    free(buffer);
    return stat;
  }
  
  free(buffer);

  usleep(1000);

  return stat;
}

///////////////////////////////////////////////////////////////////////////////
int USB3Read(uint16_t vendor_id, uint16_t product_id, unsigned char sid, uint32_t count, uint32_t addr, unsigned char *data)
{
  const unsigned int timeout = 1000; // Wait forever
  int length = 8;
  int transferred;
  unsigned char *buffer;
  int stat = 0;
  int nbulk;
  int remains;
  int loop;
  int size = 16384; // 16 kB

  nbulk = count / 4096;
  remains = count % 4096;

  if (!(buffer = (unsigned char *)malloc(size))) {
    fprintf(stderr, "USB3Read: Could not allocate memory (size = %d\n)", size);
    return -1;
  }
  
  buffer[0] = count & 0xFF;
  buffer[1] = (count >> 8)  & 0xFF;
  buffer[2] = (count >> 16)  & 0xFF;
  buffer[3] = (count >> 24)  & 0xFF;
  
  buffer[4] = addr & 0xFF;
  buffer[5] = (addr >> 8)  & 0xFF;
  buffer[6] = (addr >> 16)  & 0xFF;
  buffer[7] = (addr >> 24)  & 0x7F;
  buffer[7] = buffer[7] | 0x80;

  libusb_device_handle *devh = nkusb_get_device_handle(vendor_id, product_id, sid);
  if (!devh) {
    fprintf(stderr, "USB3Write: Could not get device handle for the device.\n");
    return -1;
  }

  if ((stat = libusb_bulk_transfer(devh, USB3_SF_WRITE, buffer, length, &transferred, timeout)) < 0) {
    fprintf(stderr, "USB3Read: Could not make write request; error = %d\n", stat);
    USB3Reset(vendor_id, product_id, sid);
    free(buffer);
    return stat;
  }

  for (loop = 0; loop < nbulk; loop++) {
    if ((stat = libusb_bulk_transfer(devh, USB3_SF_READ, buffer, size, &transferred, timeout)) < 0) {
      fprintf(stderr, "USB3Read: Could not make read request; error = %d\n", stat);
      USB3Reset(vendor_id, product_id, sid);
      return 1;
    }
    memcpy(data + loop * size, buffer, size);
  }

  if (remains) {
    if ((stat = libusb_bulk_transfer(devh, USB3_SF_READ, buffer, remains * 4, &transferred, timeout)) < 0) {
      fprintf(stderr, "USB3Read: Could not make read request; error = %d\n", stat);
      USB3Reset(vendor_id, product_id, sid);
      return 1;
    }
    memcpy(data + nbulk * size, buffer, remains * 4);
  }

  free(buffer);
  
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
int USB3WriteControl(uint16_t vendor_id, uint16_t product_id, unsigned char sid, uint8_t bRequest, uint16_t wValue,
		       uint16_t wIndex, unsigned char *data, uint16_t wLength)

{
  const unsigned int timeout = 1000;
  int stat = 0;
  
  libusb_device_handle *devh = nkusb_get_device_handle(vendor_id, product_id, sid);
  if (!devh) {
    fprintf(stderr, "USB3Write: Could not get device handle for the device.\n");
    return -1;
  }
  
  if ((stat = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT, bRequest, wValue, wIndex, data, wLength, timeout)) < 0) {
    fprintf(stderr, "USB3WriteControl:  Could not make write request; error = %d\n", stat);
    return stat;
  }
  
  return stat;
}

///////////////////////////////////////////////////////////////////////////////
int USB3ReadControl(uint16_t vendor_id, uint16_t product_id, unsigned char sid, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength)
{
  const unsigned int timeout = 1000;
  int stat = 0;
  
  libusb_device_handle *devh = nkusb_get_device_handle(vendor_id, product_id, sid);
  if (!devh) {
    fprintf(stderr, "USB3ReadControl: Could not get device handle for the device.\n");
    return -1;
  }
  
  if ((stat = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, bRequest, wValue, wIndex, data, wLength, timeout)) < 0) {
    fprintf(stderr, "USB3ReadControl: Could not make read request; error = %d\n", stat);
    return stat;
  }
  //fprintf(stdout, "ADCRead: bytes transferred = %d\n", transferred);
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
unsigned int USB3ReadReg(uint16_t vendor_id, uint16_t product_id, unsigned char sid, uint32_t addr)
{
  unsigned char data[4];
  unsigned int value;
  unsigned int tmp;

  USB3Read(vendor_id, product_id, sid, 1, addr, data);

  value = data[0] & 0xFF;
  tmp = data[1] & 0xFF;
  value = value + (unsigned int)(tmp << 8);
  tmp = data[2] & 0xFF;
  value = value + (unsigned int)(tmp << 16);
  tmp = data[3] & 0xFF;
  value = value + (unsigned int)(tmp << 24);

  return value;
}  

///////////////////////////////////////////////////////////////////////////////
unsigned long USB3ReadRegL(uint16_t vendor_id, uint16_t product_id, unsigned char sid, uint32_t addr)
{
  unsigned char data[8];
  unsigned long value;
  unsigned long tmp;

  USB3Read(vendor_id, product_id, sid, 2, addr, data);

  value = data[0] & 0xFF;
  tmp = data[1] & 0xFF;
  value = value + (unsigned long)(tmp << 8);
  tmp = data[2] & 0xFF;
  value = value + (unsigned long)(tmp << 16);
  tmp = data[3] & 0xFF;
  value = value + (unsigned long)(tmp << 24);
  tmp = data[4] & 0xFF;
  value = value + (unsigned long)(tmp << 32);
  tmp = data[5] & 0xFF;
  value = value + (unsigned long)(tmp << 40);
  tmp = data[6] & 0xFF;
  value = value + (unsigned long)(tmp << 48);
  tmp = data[7] & 0xFF;
  value = value + (unsigned long)(tmp << 56);

  return value;
}  

///////////////////////////////////////////////////////////////////////////////
unsigned char USB3CheckFPGADone(uint16_t vendor_id, uint16_t product_id, unsigned char sid)
{
   const uint8_t bRequest = NKPROGRAMMER_TARGET_USB3;
   const uint16_t wValue = NKPROGRAMMER_CMD_CHECK_DONE; 
   const uint16_t wIndex = 0;
   const uint16_t wLength = 1;

   unsigned char data;

   USB3ReadControl(vendor_id, product_id, sid, bRequest, wValue, wIndex, &data, wLength);

   return data;
}

///////////////////////////////////////////////////////////////////////////////
void USB3InitFPGA(uint16_t vendor_id, uint16_t product_id, unsigned char sid)
{
   const uint8_t bRequest = NKPROGRAMMER_TARGET_USB3;
   const uint16_t wValue = NKPROGRAMMER_CMD_INIT;
   const uint16_t wIndex = 0;
   const uint16_t wLength = 1;
   unsigned char data = 0;

   USB3WriteControl(vendor_id, product_id, sid, bRequest, wValue, wIndex, &data, wLength);
}

///////////////////////////////////////////////////////////////////////////////
unsigned char USB3CheckFPGADoneNoAVR(uint16_t vendor_id, uint16_t product_id, unsigned char sid)
{
   const uint8_t bRequest = NKPROGRAMMER_CHECK_DONE;
   const uint16_t wValue = 0; 
   const uint16_t wIndex = 0;
   const uint16_t wLength = 1;

   unsigned char data;

   USB3ReadControl(vendor_id, product_id, sid, bRequest, wValue, wIndex, &data, wLength);

   return data;
}

///////////////////////////////////////////////////////////////////////////////
void USB3InitFPGANoAVR(uint16_t vendor_id, uint16_t product_id, unsigned char sid)
{
   const uint8_t bRequest = NKPROGRAMMER_RESET_FIFO;
   const uint16_t wValue = 0;
   const uint16_t wIndex = 0;
   const uint16_t wLength = 1;
   unsigned char data = 0;

   USB3WriteControl(vendor_id, product_id, sid, bRequest, wValue, wIndex, &data, wLength);
}

///////////////////////////////////////////////////////////////////////////////
void USB3ResetEP2(uint16_t vendor_id, uint16_t product_id, unsigned char sid)

{
  const uint8_t bRequest = NKPROGRAMMER_RESET_EP2;
  const uint16_t wValue = 0;
  const uint16_t wIndex = 0;
  const uint16_t wLength = 0;
  unsigned char data = 0;
  
  USB3WriteControl(vendor_id, product_id, sid, bRequest, wValue, wIndex, &data, wLength);
}

///////////////////////////////////////////////////////////////////////////////
void USB3ResetEP6(uint16_t vendor_id, uint16_t product_id, unsigned char sid)

{
  const uint8_t bRequest = NKPROGRAMMER_RESET_EP6;
  const uint16_t wValue = 0;
  const uint16_t wIndex = 0;
  const uint16_t wLength = 0;
  unsigned char data = 0;
  
  USB3WriteControl(vendor_id, product_id, sid, bRequest, wValue, wIndex, &data, wLength);
}
