/* dxpServer.h -- communication between record and device support and
      MPF server for DXP record */

/* Note:  These parameters must be different from any in mca.h, since the
          DXP record uses the codes from mca.h in addition to these codes */
#define MSG_DXP_SET_SHORT_PARAM          100
#define MSG_DXP_SET_LONG_PARAM           101
#define MSG_DXP_CALIBRATE                102
#define MSG_DXP_READ_PARAMS              103
#define MSG_DXP_DOWNLOAD_FIPPI           104

#define MODEL_DXP4C 0
#define MODEL_DXP2X 1
