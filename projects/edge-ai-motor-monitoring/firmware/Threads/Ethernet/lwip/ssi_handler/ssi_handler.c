/*
 * ssi_handler.c
 *
 *  Created on: Apr 10, 2026
 */


#include "../../../Ethernet/lwip/ssi_handler/ssi_handler.h"

#include "stdio.h"
#include "string.h"
#include "current_data.h"
#include "vibration_data.h"
#include "../../../Ethernet/src/include/lwip/apps/httpd.h"
#include "../../../Ethernet/src/include/lwip/arch.h"
#include "../../../Ethernet/src/include/lwip/def.h"
#include "../../../Ethernet/src/include/lwip/opt.h"
// your global variables
extern float temp;
extern uint16_t speed;
extern INA260_AvgData ina_data;
extern LIS3DH_Axis_t vib_data;
extern uint8_t similarity;

extern uint16_t speed_snapshot;
extern float features[14];

extern char mt_status[16];
extern char mt_state[48];
extern char mt_class[32];
extern float mt_class_conf;
extern float mt_mse;
extern float mt_threshold;
extern float mt_aprob;
extern float mt_severity_ratio;
extern char mt_severity[16];
// SSI tag
static const char *ssi_tags[] = {"temp", "speed", "current", "volt","vibration", "score", "data"};

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen)
{
	if (iIndex == 0)   // temp
	    {
	        snprintf(pcInsert, iInsertLen, "%.4f C", temp);
	    }
	    else if (iIndex == 1) // speed
	    {
	        snprintf(pcInsert, iInsertLen, "%d", speed);
	    }
	    else if (iIndex == 2) // current
	    {
	        snprintf(pcInsert, iInsertLen, "%.3f A", ina_data.avg_current);
	    }
	    else if (iIndex == 3) // volt
	    {
	        snprintf(pcInsert, iInsertLen, "%.3f V", ina_data.avg_voltage);
	    }
	    else if (iIndex == 4) // vibration
		{
			snprintf(pcInsert, iInsertLen, " %.3f  %.3f  %.3f", vib_data.x,vib_data.y, vib_data.z);
		}
	    else if(iIndex == 5){
	    	snprintf(pcInsert, iInsertLen, "%d %%", similarity);
	    }
	    else if (iIndex == 6)   /* data: 14 dashboard features */
	      {
	          snprintf(pcInsert, iInsertLen,
	                   "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f",
	                   features[0],
	                   features[1],
	                   features[2],
	                   features[3],
	                   features[4],
	                   features[5],
	                   features[6],
	                   features[7],
	                   features[8],
	                   features[9],
	                   features[10],
	                   features[11],
	                   features[12],
					   features[13]);

	      }
	    else if (iIndex == 7)   /* data: AI dashboard features */
	   	      {
	   	          snprintf(pcInsert, iInsertLen,
	   	                   "%s,%s,%s,%.2f,%.6f,%.6f,%.2f,%.2f,%s",
						  mt_status,
						  mt_state,
						  mt_class,
						  mt_class_conf,
						  mt_mse,
						  mt_threshold,
						  mt_aprob,
						  mt_severity_ratio,
						  mt_severity);

	   	      }
	    else
	    {
	        snprintf(pcInsert, iInsertLen, "N/A");
	    }
	    return strlen(pcInsert);
}

void SSI_Init(void)
{
    http_set_ssi_handler(ssi_handler, (char const**)ssi_tags, sizeof(ssi_tags));
}
