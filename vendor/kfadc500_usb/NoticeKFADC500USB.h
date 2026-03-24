#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libusb.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KFADC500_VID (0x0547)
#define KFADC500_PID (0x1080)

// request command
#define NKPROGRAMMER_CMD_INIT          (10)
#define NKPROGRAMMER_CMD_CHECK_DONE    (11)

// Request target 
#define NKPROGRAMMER_TARGET_USB3            (0xF3)

extern void USB3Init(void);
extern void USB3Exit(void);
extern int KFADC500open(int sid);
extern void KFADC500close(int sid);
extern void KFADC500read_DATA(int sid, int nkbyte, char *data);
extern void KFADC500reset(int sid);
extern void KFADC500write_RM(int sid, int st, int se, int sr, int ss);
extern int KFADC500read_RM(int sid);
extern void KFADC500start(int sid);
extern void KFADC500stop(int sid);
extern int KFADC500read_Run(int sid);
extern void KFADC500write_AMODE(int sid, int data);
extern int KFADC500read_AMODE(int sid);
extern void KFADC500write_RL(int sid, int data);
extern int KFADC500read_RL(int sid);
extern void KFADC500write_TLT(int sid, int tlt, int mode);
extern int KFADC500read_TLT(int sid);
extern void KFADC500write_TOW(int sid,  int data);
extern int KFADC500read_TOW(int sid);
extern void KFADC500send_TRIG(int sid);
extern void KFADC500write_DRAMON(int sid, int data);
extern int KFADC500read_DRAMON(int sid);
extern int KFADC500read_BCOUNT(int sid);
extern long long KFADC500read_TIMER(int sid);
extern int KFADC500read_ENUM(int sid);
extern int KFADC500read_TNUM(int sid);
extern void KFADC500write_DACOFF(int sid, int ch, int data);
extern int KFADC500read_DACOFF(int sid, int ch);
extern void KFADC500measure_PED(int sid, int ch);
extern int KFADC500read_PED(int sid, int ch);
extern void KFADC500write_DLY(int sid, int ch, int data);
extern int KFADC500read_DLY(int sid, int ch);
extern void KFADC500write_POL(int sid, int ch, int data);
extern int KFADC500read_POL(int sid, int ch);
extern void KFADC500write_THR(int sid, int ch, int data);
extern int KFADC500read_THR(int sid, int ch);
extern void KFADC500write_TM(int sid, int ch, int ew, int en);
extern int KFADC500read_TM(int sid, int ch);
extern void KFADC500write_PCT(int sid, int ch, int data);
extern int KFADC500read_PCT(int sid, int ch);
extern void KFADC500write_PCI(int sid, int ch, int data);
extern int KFADC500read_PCI(int sid, int ch);
extern void KFADC500write_PWT(int sid, int ch, int data);
extern int KFADC500read_PWT(int sid, int ch);
extern void KFADC500write_DT(int sid, int ch, int data);
extern int KFADC500read_DT(int sid, int ch);
extern void KFADC500write_CW(int sid, int ch, int data);
extern int KFADC500read_CW(int sid, int ch);
extern int KFADC500read_ADCDLY(int sid, int ch);
extern void KFADC500calibrate(int sid);

#ifdef __cplusplus
}
#endif

