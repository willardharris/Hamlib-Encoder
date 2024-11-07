#ifndef RIG_CONTROL_H
#define RIG_CONTROL_H
#include <WiFi.h>  // Ensure WiFiClient is defined
#include <stdint.h>


// Declare rigClient as an external variable to avoid multiple definitions
extern WiFiClient rigClient;

// Define enums for Mode and VFO
typedef enum {
    USB, LSB, CW, CWR, RTTY, RTTYR, AM, FM, WFM, AMS,
    PKTLSB, DIGI, PKTFM, ECSSUSB, ECSSLSB, FA, SAM,
    SAL, SAH, DSB, MODE_UNSUPPORTED
} RigMode;

typedef enum {
    VFOA, VFOB, VFOC, CURRVFO, VFO, MEM, MAIN, SUB, VFO_TX, VFO_RX, VFO_UNSUPPORTED
} RigVFO;

// Function prototypes
int init_rig_socket();
void close_rig_socket();

void set_freq(float frequency);
int get_freq(float *frequency);

void set_mode(RigMode mode, int32_t passband);
int get_mode(RigMode *mode, int32_t *passband);

void set_vfo(RigVFO vfo);
int get_vfo(RigVFO *vfo);

#endif // RIG_CONTROL_H
