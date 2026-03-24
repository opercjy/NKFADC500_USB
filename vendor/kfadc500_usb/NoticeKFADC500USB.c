#include "NoticeKFADC500USB.h"

enum EManipInterface {kInterfaceClaim, kInterfaceRelease};

struct dev_open {
   libusb_device_handle *devh;
   uint16_t vendor_id;
   uint16_t product_id;
   int serial_id;
   struct dev_open *next;
} *ldev_open = 0;

// internal functions *********************************************************************************
static int is_device_open(libusb_device_handle *devh);
static unsigned char get_serial_id(libusb_device_handle *devh);
static void add_device(struct dev_open **list, libusb_device_handle *tobeadded, int sid);
static int handle_interface_id(struct dev_open **list, int sid, int interface, enum EManipInterface manip_type);
static void remove_device_id(struct dev_open **list, int sid);
libusb_device_handle* nkusb_get_device_handle(int sid);
int USB3Read_i(int sid, int count, int addr, char *data);
int USB3Write(int sid, int addr, int data);
int USB3Read(int sid, int addr);
int USB3Read_Block(int sid, int count, int addr, char *data);
int USB3WriteControl(int sid, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength);
int USB3ReadControl(int sid, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength);
void USB3InitFPGA(int sid);
unsigned char USB3CheckFPGADone(int sid);
void KFADC500send_ADCRST(int sid);
void KFADC500send_ADCCAL(int sid);
void KFADC500write_ADCDLY(int sid, int ch, int data);
void KFADC500write_ADCCONFIG(int sid, int ch, unsigned char caddr, int cdata);

static int is_device_open(libusb_device_handle *devh)
{
// See if the device handle "devh" is on the open device list

  struct dev_open *curr = ldev_open;
  libusb_device *dev, *dev_curr;
  int bus, bus_curr, addr, addr_curr;

  while (curr) {
    dev_curr = libusb_get_device(curr->devh);
    bus_curr = libusb_get_bus_number(dev_curr);
    addr_curr = libusb_get_device_address(dev_curr);

    dev = libusb_get_device(devh);
    bus = libusb_get_bus_number(dev);
    addr = libusb_get_device_address(dev);

    if (bus == bus_curr && addr == addr_curr) return 1;
    curr = curr->next;
  }

  return 0;
}

static unsigned char get_serial_id(libusb_device_handle *devh)
{
  int ret;
  if (!devh) {
    return 0;
  }
  unsigned char data[1];
  ret = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, 0xD2, 0, 0, data, 1, 1000);

  if (ret < 0) {
    fprintf(stdout, "Warning: get_serial_id: Could not get serial id.\n");
    return 0;
  }

  return data[0];
}

static void add_device(struct dev_open **list, libusb_device_handle *tobeadded, int sid)
{
  struct dev_open *curr;

  curr = (struct dev_open *)malloc(sizeof(struct dev_open));
  curr->devh = tobeadded;
  curr->vendor_id = KFADC500_VID;
  curr->product_id = KFADC500_PID;
  curr->serial_id = sid;
  curr->next  = *list;
  *list = curr;
}

static int handle_interface_id(struct dev_open **list, int sid, int interface, enum EManipInterface manip_type)
{
  int ret = 0;
  if (!*list) {
    ret = -1;
    return ret;
  }

  struct dev_open *curr = *list;
  struct libusb_device_descriptor desc;
  libusb_device *dev;

  while (curr) {
    dev =libusb_get_device(curr->devh);
    if (libusb_get_device_descriptor(dev, &desc) < 0) {
      fprintf(stdout, "Warning: remove_device: could not get device device descriptior."
                          " Ignoring.\n");
      continue;
    }
    if (desc.idVendor == KFADC500_VID && desc.idProduct == KFADC500_PID
    && (sid == 0xFF || sid == get_serial_id(curr->devh))) { 
      if (manip_type == kInterfaceClaim) {
        if ((ret = libusb_claim_interface(curr->devh, interface)) < 0) {
          fprintf(stdout, "Warning: handle_interface_id: Could not claim interface (%d) on device (%u, %u, %u)\n",
                  interface, KFADC500_VID, KFADC500_PID, sid);
        }
      }
      else if (manip_type == kInterfaceRelease) {
        if ((ret =libusb_release_interface(curr->devh, interface)) < 0) {
          fprintf(stdout, "Warning: handle_interface_id: Could not release interface (%d) on device (%u, %u, %u)\n",
                  interface, KFADC500_VID, KFADC500_PID, sid);
        }
      }
      else {
        fprintf(stderr, "Error: handle_interface_id: Unknown interface handle request: %d\n.",
                manip_type);
              
        ret = -1;
        return ret;
      }
    }

    curr = curr->next;
  }

  return ret;
}

static void remove_device_id(struct dev_open **list, int sid)
{
  if (!*list) return;

  struct dev_open *curr = *list;
  struct dev_open *prev = 0;
  struct libusb_device_descriptor desc;
  libusb_device *dev;

  while (curr) {
    dev =libusb_get_device(curr->devh);
    if (libusb_get_device_descriptor(dev, &desc) < 0) {
      fprintf(stdout, "Warning, remove_device: could not get device device descriptior." " Ignoring.\n");
      continue;
    }
    if (desc.idVendor == KFADC500_VID && desc.idProduct == KFADC500_PID
    && (sid == 0xFF || sid == get_serial_id(curr->devh))) { 
      if (*list == curr) { 
        *list = curr->next;
        libusb_close(curr->devh);
        free(curr); 
        curr = *list;
      }
      else {
        prev->next = curr->next;
        libusb_close(curr->devh);
        free(curr); 
        curr = prev->next;
      }
    }
    else {
      prev = curr;
      curr = curr->next;
    }    
  }
}

libusb_device_handle* nkusb_get_device_handle(int sid) 
{
  struct dev_open *curr = ldev_open;
  while (curr) {
    if (curr->vendor_id == KFADC500_VID && curr->product_id == KFADC500_PID) {
      if (sid == 0xFF)
        return curr->devh;
      else if (curr->serial_id == sid)
        return curr->devh;
    }

    curr = curr->next;
  }

  return 0;
}

int USB3Read_i(int sid, int count, int addr, char *data)
{
  const unsigned int timeout = 1000; 
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

  libusb_device_handle *devh = nkusb_get_device_handle(sid);
  if (!devh) {
    fprintf(stderr, "USB3Write: Could not get device handle for the device.\n");
    return -1;
  }

  if ((stat = libusb_bulk_transfer(devh, 0x06, buffer, length, &transferred, timeout)) < 0) {
    fprintf(stderr, "USB3Read: Could not make write request; error = %d\n", stat);
    free(buffer);
    return stat;
  }

  for (loop = 0; loop < nbulk; loop++) {
    if ((stat = libusb_bulk_transfer(devh, 0x82, buffer, size, &transferred, timeout)) < 0) {
      fprintf(stderr, "USB3Read: Could not make read request; error = %d\n", stat);
      return 1;
    }
    memcpy(data + loop * size, buffer, size);
  }

  if (remains) {
    if ((stat = libusb_bulk_transfer(devh, 0x82, buffer, remains * 4, &transferred, timeout)) < 0) {
      fprintf(stderr, "USB3Read: Could not make read request; error = %d\n", stat);
      return 1;
    }
    memcpy(data + nbulk * size, buffer, remains * 4);
  }

  free(buffer);
  
  return 0;
}

int USB3Write(int sid, int addr, int data)
{
  int transferred = 0;  
  const unsigned int timeout = 1000;
  int length = 8;
  unsigned char *buffer;
  int stat = 0;
  
  if (!(buffer = (unsigned char *)malloc(length))) {
    fprintf(stderr, "TCBWrite: Could not allocate memory (size = %d\n)", length)
;
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

  libusb_device_handle *devh = nkusb_get_device_handle(sid);
  if (!devh) {
    fprintf(stderr, "TCBWrite: Could not get device handle for the device.\n");
    return 0;
  }
  
  if ((stat = libusb_bulk_transfer(devh, 0x06, buffer, length, &transferred, timeout)) < 0) {
    fprintf(stderr, "TCBWrite: Could not make write request; error = %d\n", stat);
    free(buffer);
    return 0;
  }
  
  free(buffer);

  usleep(1000);

  return stat;
}

int USB3Read(int sid, int addr)
{
  unsigned char data[4];
  int value;
  int tmp;

  USB3Read_i(sid, 1, addr, data);

  value = data[0] & 0xFF;
  tmp = data[1] & 0xFF;
  tmp = tmp << 8;
  value = value + tmp;
  tmp = data[2] & 0xFF;
  tmp = tmp << 16;
  value = value + tmp;
  tmp = data[3] & 0xFF;
  tmp = tmp << 24;
  value = value + tmp;

  return value;
}

int USB3Read_Block(int sid, int count, int addr, char *data)
{
  return USB3Read_i(sid, count, addr, data);
}

int USB3WriteControl(int sid, uint8_t bRequest, uint16_t wValue,
		       uint16_t wIndex, unsigned char *data, uint16_t wLength)

{
  const unsigned int timeout = 1000;
  int stat = 0;
  
  libusb_device_handle *devh = nkusb_get_device_handle(sid);
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

int USB3ReadControl(int sid, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength)
{
  const unsigned int timeout = 1000;
  int stat = 0;
  
  libusb_device_handle *devh = nkusb_get_device_handle(sid);
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

void USB3InitFPGA(int sid)
{
   const uint8_t bRequest = NKPROGRAMMER_TARGET_USB3;
   const uint16_t wValue = NKPROGRAMMER_CMD_INIT;
   const uint16_t wIndex = 0;
   const uint16_t wLength = 1;
   unsigned char data = 0;

   USB3WriteControl(sid, bRequest, wValue, wIndex, &data, wLength);
}

unsigned char USB3CheckFPGADone(int sid)
{
   const uint8_t bRequest = NKPROGRAMMER_TARGET_USB3;
   const uint16_t wValue = NKPROGRAMMER_CMD_CHECK_DONE; 
   const uint16_t wIndex = 0;
   const uint16_t wLength = 1;
   unsigned char data;

   USB3ReadControl(sid, bRequest, wValue, wIndex, &data, wLength);

   return data;
}

// send ADC reset signal
void KFADC500send_ADCRST(int sid)
{
  USB3Write(sid, 0x5E, 0);
}

// send ADC calibration signal
void KFADC500send_ADCCAL(int sid)
{
  USB3Write(sid, 0x5F, 0);
}

// Write ADC calibration delay
void KFADC500write_ADCDLY(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x0B;
  USB3Write(sid, addr, data);
}

// Write ADC configuration data
void KFADC500write_ADCCONFIG(int sid, int ch, unsigned char caddr, int cdata)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x0C;
  data = (caddr << 8) | cdata;
  USB3Write(sid, addr, data);
}

// ******************************************************************************************************

// initialize libusb library
void USB3Init(void)
{
  if (libusb_init(0) < 0) {
    fprintf(stderr, "Failed to initialise LIBUSB\n");
    exit(1);
  }
}

// de-initialize libusb library
void USB3Exit(void)
{
  libusb_exit(0); 
}

// open KFADC500
int KFADC500open(int sid)
{
  struct libusb_device **devs;
  struct libusb_device *dev;
  struct libusb_device_handle *devh;
  size_t i = 0;
  int nopen_devices = 0; //number of open devices
  int r;
  int interface = 0;
  int sid_tmp;
  int speed;
  int status = 1;
  int ready;

  if (libusb_get_device_list(0, &devs) < 0) 
    fprintf(stderr, "Error: open_device: Could not get device list\n");

  fprintf(stdout, "Info: open_device: opening device Vendor ID = 0x%X, Product ID = 0x%X, Serial ID = %u\n", KFADC500_VID, KFADC500_PID, sid);

  while ((dev = devs[i++])) {
    struct libusb_device_descriptor desc;
    r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) {
      fprintf(stdout, "Warning, open_device: could not get device device descriptior." " Ignoring.\n");
      continue;
    }

    if (desc.idVendor == KFADC500_VID && desc.idProduct == KFADC500_PID)  {
      r = libusb_open(dev, &devh);
      if (r < 0) {
        fprintf(stdout, "Warning, open_device: could not open device." " Ignoring.\n");
        continue;
      }

      // do not open twice
      if (is_device_open(devh)) {
        fprintf(stdout, "Info, open_device: device already open." " Ignoring.\n");
        libusb_close(devh);
        continue;
      }

      // See if sid matches
      // Assume interface 0
      if (libusb_claim_interface(devh, interface) < 0) {
        fprintf(stdout, "Warning, open_device: could not claim interface 0 on the device." " Ignoring.\n");
        libusb_close(devh);
        continue;
      }

      sid_tmp = get_serial_id(devh);

      if (sid == 0xFF || sid == sid_tmp) {
        add_device(&ldev_open, devh, sid_tmp);
        nopen_devices++;
  
        // Print out the speed of just open device 
        speed = libusb_get_device_speed(dev);
        switch (speed) {
          case 4:
            fprintf(stdout, "Info: open_device: super speed device opened");
            break;
          case 3:
            fprintf(stdout, "Info: open_device: high speed device opened");
            break;
          case 2:
            fprintf(stdout, "Info: open_device: full speed device opened");
            break;
          case 1:
            fprintf(stdout, "Info: open_device: low speed device opened");
            break;
          case 0:
            fprintf(stdout, "Info: open_device: unknown speed device opened");
            break;
        }
        
        fprintf(stdout, " (bus = %d, address = %d, serial id = %u).\n",
                    libusb_get_bus_number(dev), libusb_get_device_address(dev), sid_tmp);
        libusb_release_interface(devh, interface);
        break;
      }
      else {
        status = 0;
        fprintf(stdout, "No module!!\n");
        libusb_release_interface(devh, interface);
        libusb_close(devh);
      }
    }
  }

  libusb_free_device_list(devs, 1);

  // claim interface
  handle_interface_id(&ldev_open, sid, 0, kInterfaceClaim);

  if (!nopen_devices)
    return -1;

  devh = nkusb_get_device_handle(sid);
  if (!devh) {
    fprintf(stderr, "Could not get device handle for the device.\n");
    return -1;
  }

  ready = USB3CheckFPGADone(sid);
  if (!ready)
    printf("Wait for KFADC500 booting-up.\n");

  while (!ready) 
    ready = USB3CheckFPGADone(sid);
  
  USB3InitFPGA(sid);

  printf("Now KFADC500 is ready.\n");

  return status;
}

// close KFADC500
void KFADC500close(int sid)
{
  handle_interface_id(&ldev_open, sid, 0, kInterfaceRelease);
  remove_device_id(&ldev_open, sid);
}

// read data, reads 16 kbytes from KFADC500 DRAM
// returns character raw data, needs sorting after data acquisition
void KFADC500read_DATA(int sid, int nkbyte, char *data)
{
  int count;

  // maximum data size is 64 Mbyte
  count = nkbyte * 256;
  USB3Read_Block(sid, count, 0x1000, data);  
}

// Send Reset signal
void KFADC500reset(int sid)
{
  USB3Write(sid, 0x50, 0);
} 

// Write Reset Mode
// st = timer reset, se = event number reset, sr = register setting reset
// ss = reset source, 0 = internal, 1 = external
void KFADC500write_RM(int sid, int st, int se, int sr, int ss)
{
  int data;
	
  data = 0;

  if (st)
    data = data | 0x1;
  if (se)
    data = data | 0x2;
  if (sr)
    data = data | 0x4;
  if (ss)
    data = data | 0x8;

  USB3Write(sid, 0x51, data);
}

// Read Reset Mode
int KFADC500read_RM(int sid) 
{ 
  return USB3Read(sid, 0x51);
}

// start data acquisition
void KFADC500start(int sid)
{
  USB3Write(sid, 0x52, 0x1);
}

// Stop data acquisition
void KFADC500stop(int sid)
{
  USB3Write(sid, 0x52, 0x0);
}

// Read DAQ status
int KFADC500read_Run(int sid)
{
  return USB3Read(sid, 0x52);
}

// write ADC mode
// 0 for bypassed mode, 1 for filtered mode
void KFADC500write_AMODE(int sid, int data)
{
  USB3Write(sid, 0x53, data);
}

// read ADC mode
int KFADC500read_AMODE(int sid)
{ 
  return USB3Read(sid, 0x53);
}

// Write Segment Setting
// 1 = 128 ns, 2 = 256 ns, 4 = 512 ns, 8 = 1 us,
// 16 = 2 us, 32 = 4 us, 64 = 8 us, 128 = 16 us, 256 = 32 us
void KFADC500write_RL(int sid, int data)
{
  USB3Write(sid, 0x54, data);
}

// Read Segment Setting
int KFADC500read_RL(int sid)
{
  return USB3Read(sid, 0x54);
}

// Write Trigger Lookup Table
// tlt = lookup table value for self trigger (or trigger output)
// mode = select self/external trigger, 0 = self, 1 = external
void KFADC500write_TLT(int sid, int tlt, int mode)
{
  int data;
	
  data = tlt | (mode << 16);
  USB3Write(sid, 0x55, data);
}

// Read Trigger Lookup Table
int KFADC500read_TLT(int sid)
{
  return USB3Read(sid, 0x55);
}

// Write Trigger Output Width in ns
void KFADC500write_TOW(int sid, int data)
{
  if (data < 24)
    data = 0;
  else
    data = (data / 8) - 2;

  USB3Write(sid, 0x56, data);
}

// Read Trigger Output Width
int KFADC500read_TOW(int sid)
{
  int data;
	
  data = USB3Read(sid, 0x56);
  data = (data + 2) * 8;

  return data;
}

// send software trigger
void KFADC500send_TRIG(int sid)
{
  USB3Write(sid, 0x57, 0);
}

// turn on/off DRAM
// 0 = off, 1 = on
void KFADC500write_DRAMON(int sid, int data)
{
  USB3Write(sid, 0x58, data);
}

// Read DRAM status
int KFADC500read_DRAMON(int sid)
{
  return USB3Read(sid, 0x58);
}

// Read data buffer count(in kbyte)
int KFADC500read_BCOUNT(int sid)
{
  int data;
	
  data = USB3Read(sid, 0x59);
  data = data * 16;
  
  return data;
}

// Read timer
long long KFADC500read_TIMER(int sid)
{
  long long data;
  int rdat;
  long long lval;

  rdat = USB3Read(sid, 0x5A);
  data = rdat;

  rdat = USB3Read(sid, 0x5B);
  lval = rdat;
  lval = lval << 32;
  
  data = data + lval;
  data = data * 8;
  
  return data;
}

// Read Event Number
int KFADC500read_ENUM(int sid)
{
  return USB3Read(sid, 0x5C);
}

// Read trigger number
int KFADC500read_TNUM(int sid)
{
  return USB3Read(sid, 0x5D);
}

// Write Offset Adjustment
void KFADC500write_DACOFF(int sid, int ch, int data)
{
  int addr;

  addr = (ch << 4) + 0x00;
  USB3Write(sid, addr, data);
}

// Read Offset Adjustment
int KFADC500read_DACOFF(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x00;
  data = USB3Read(sid, addr);

  return data;
}

// Measure Pedestal
void KFADC500measure_PED(int sid, int ch)
{
  int addr;
	
  addr = (ch << 4) + 0x01;
  USB3Write(sid, addr, 0);
}

// Read Pedestal
int KFADC500read_PED(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x01;
  data = USB3Read(sid, addr);

  return data;
}

// Write Input Delay
void KFADC500write_DLY(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x02;
  data = data / 8;

  USB3Write(sid, addr, data);
}

// Read Input Delay
int KFADC500read_DLY(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x02;
  data = USB3Read(sid, addr);
  data = data * 8;

  return data;
}

// Write Input Pulse Polarity
void KFADC500write_POL(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x03;
  USB3Write(sid, addr, data);
}

// Read Input Pulse Polarity
int KFADC500read_POL(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x03;
  data = USB3Read(sid, addr);
  
  return data;
}

// Write Discriminator Threshold
void KFADC500write_THR(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x04;
  USB3Write(sid, addr, data);
}

// Read Discriminator Threshold
int KFADC500read_THR(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x04;
  data = USB3Read(sid, addr);
  
  return data;
}

// Write Trigger Mode
void KFADC500write_TM(int sid, int ch, int ew, int en)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x05;
  data = 0;

  if (ew)
    data = data | 0x02;
  if (en)
    data = data | 0x01;

  USB3Write(sid, addr, data);
}

// Read Trigger Mode
int KFADC500read_TM(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x05;
  data = USB3Read(sid, addr);

  return data;
}

// Write Pulse Count Threshold
void KFADC500write_PCT(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x06;
  USB3Write(sid, addr, data);
}

// Read Pulse Count Threshold
int KFADC500read_PCT(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x06;
  data = USB3Read(sid, addr);

  return data;
}

// Write Pulse Count Interval
void KFADC500write_PCI(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x07;
  data = data / 32;

  USB3Write(sid, addr, data);
}

// Read Pulse Count Interval
int KFADC500read_PCI(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x07;
  data = USB3Read(sid, addr);
  data = data * 32;

  return data;
}

// Write Pulse Width Threshold
void KFADC500write_PWT(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x08;
  data = data / 2;
  USB3Write(sid, addr, data);
}

// Read Pulse Width Threshold
int KFADC500read_PWT(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x08;
  data = USB3Read(sid, addr);
  data = data * 2;

  return data;
}

// Write Deadtime
void KFADC500write_DT(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x09;
  data = data / 32768;
  USB3Write(sid, addr, data);
}

// Read Deadtime
int KFADC500read_DT(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x09;
  data = USB3Read(sid, addr);
  data = data * 32768;

  return data;
}

// Write Coincidence Window
void KFADC500write_CW(int sid, int ch, int data)
{
  int addr;
	
  addr = (ch << 4) + 0x0A;
  data = data / 128;
  USB3Write(sid, addr, data);
}

// Read Coincidence Window
int KFADC500read_CW(int sid, int ch)
{
  int addr;
  int data;
	
  addr = (ch << 4) + 0x0A;
  data = USB3Read(sid, addr);
  data = data * 128;

  return data;
}

// calibrate ADC
void KFADC500calibrate(int sid)
{
  int ch;
  int sum[4];
  int count[4];
  int fflag[4];
  int dly[4];
  int i;
  int j;
  char data[0x4000];
  int adcch[4][2044];
  int cdat;
  int gflag;

  // set ADC mode unfiltered
  KFADC500write_AMODE(sid, 0);
  
  // trigger lookup table to 0
  KFADC500write_TLT(sid, 0x0000, 0);

  // set recording length to 4 us
  KFADC500write_RL(sid, 32);

  // reset ADC
  KFADC500send_ADCRST(sid);

  usleep(1000000);

  // set ADC output mode to check pattern
  KFADC500write_ADCCONFIG(sid, 0, 0xC0, 4);

  // reset DAQ
  KFADC500reset(sid);

  for (ch = 1; ch <= 4; ch++) {
    sum[ch - 1] = 0;
    count[ch - 1] = 0;
    fflag[ch - 1] = 0;
    dly[ch - 1] = 0;
  }

  for (i = 0; i < 256; i++) {
    // set adc delay
    KFADC500write_ADCDLY(sid, 0, i);
    
    // send calibration signal
    KFADC500send_ADCCAL(sid);

    // run DAQ
    KFADC500start(sid);
    
    // send software trigger
    KFADC500send_TRIG(sid);
    
    // stop DAQ
    KFADC500stop(sid);

    // read data
    KFADC500read_DATA(sid, 16, data);

    // sorting data
    for (ch = 1; ch <= 4; ch++) {
      for (j = 0; j < 2044; j++) {
	adcch[ch - 1][j] = data[8 * j + ch * 2 + 31] & 0xFF;
	adcch[ch - 1][j] = adcch[ch - 1][j] << 8;
	adcch[ch - 1][j] = adcch[ch - 1][j] + (int)(data[8 * j + ch * 2 + 30] & 0xFF);
      }
    }
    
    // read dummy data
    KFADC500read_DATA(sid, 16, data);

    for (ch = 1; ch <= 4; ch++) {
      gflag = 1;
      
      cdat = adcch[ch - 1][0];	
      for (j = 1; j < 1022; j++)  {
	if (adcch[ch - 1][2 * j] != cdat)
	  gflag = 0;
      }
      
      if (!fflag[ch - 1]) {
	if (gflag) {
	  sum[ch - 1] = sum[ch - 1] + i;
	  count[ch - 1] = count[ch - 1] + 1;
	}
	if (!gflag) {
	  if (count[ch - 1] > 15) {
	    fflag[ch - 1] = 1;
	    dly[ch - 1] = sum[ch - 1] / count[ch - 1];
	  }
	  else {
	    sum[ch - 1] = 0;
	    count[ch - 1] = 0;
	  }
	}
      }
    }

    if (fflag[0] && fflag[1] && fflag[2] && fflag[3])
      i = 256;
  }

  // set to good delay
  for (ch = 1; ch <= 4; ch++) 
    KFADC500write_ADCDLY(sid, ch, dly[ch - 1]);

  // send calibration signal
  KFADC500send_ADCCAL(sid);
  
  // set ADC output mode to normal operation
  KFADC500write_ADCCONFIG(sid, 0, 0xC0, 0);
  
  // trigger lookup table to 0xFFFE
  KFADC500write_TLT(sid, 0xFFFE, 0);
}


