#include "dvl_uart.h"
#include <string.h>
#include <stdlib.h>
#include "main.h"

#define DVL_RX_BUFFER_SIZE 128

static DVL_Data_t g_dvl_uart_data;
static char g_dvl_rx_buffer[DVL_RX_BUFFER_SIZE];
static uint16_t g_dvl_rx_index;

static bool Dvl_ChecksumOk(const uint8_t *frame)
{
	//ToDo
	return true;
}

void DvlUart_Init(void)
{
    memset(&g_dvl_uart_data, 0, sizeof(g_dvl_uart_data));
    memset(g_dvl_rx_buffer, 0, sizeof(g_dvl_rx_buffer));
    g_dvl_rx_index = 0U;
}

static void DvlUart_ParseFrame(const uint8_t *frame)
{
	if(strncmp((const char *)frame, ":BI,", 4) != 0){
		return;
	}
	
	if(!Dvl_ChecksumOk(frame)){
		g_dvl_uart_data.checksum_error_count++;
		return;
	}
	
	const char *p = (char *)(frame + 4);
	g_dvl_uart_data.vx = strtof(p, (char **)&p); if (*p == ',') p++;
	g_dvl_uart_data.vy = strtof(p, (char **)&p); if (*p == ',') p++;
	g_dvl_uart_data.vz = strtof(p, (char **)&p); if (*p == ',') p++;
	g_dvl_uart_data.ve = strtof(p, (char **)&p); if (*p == ',') p++;
	
	while(*p == ' ')p++;
	if (*p != '\0' && *p != '*') g_dvl_uart_data.status = *p;
	
	g_dvl_uart_data.frame_count++;
	g_dvl_uart_data.last_update_tick = HAL_GetTick();
}

void DvlUart_InputBytes(const uint8_t *data, uint16_t len){
	if (data == NULL) return;
	
	for (uint16_t i=0;i<len;i++){
		char byte = (char)data[i];
		
		if(byte == ':'){
			g_dvl_rx_index = 0;
			g_dvl_rx_buffer[g_dvl_rx_index++] = byte;
			continue;
		}
		
		if(byte == '\n'){
			if(g_dvl_rx_index > 0){
				g_dvl_rx_buffer[g_dvl_rx_index] = '\0';
				DvlUart_ParseFrame((const uint8_t *)g_dvl_rx_buffer);
			}
			g_dvl_rx_index = 0;
			continue;
		}
		if (g_dvl_rx_index < (DVL_RX_BUFFER_SIZE - 1)) {
			if (byte != '\r'){
				g_dvl_rx_buffer[g_dvl_rx_index++] = byte;
			}
		}else{
			g_dvl_rx_index = 0;
		}
	}
}

void DvlUart_GetData(DVL_Data_t *out_data)
{
    if (out_data == NULL) return;
    *out_data = g_dvl_uart_data;
}
