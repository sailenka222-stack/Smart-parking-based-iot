#ifndef WE10_H_
#define WE10_H_

#include "main.h"

// Expose global variables
extern char current_user_name[32];
extern char current_user_vehicle[32];

// Function Prototypes
void WE10_Init(char *SSID, char *PASSWD);
void send_http_cmd(const char* cmd);  // <--- Ensure this is exactly matching!
uint8_t WE10_CheckUser(char *uid);
void Firebase_SetData(const char* path, const char* json_data);
void Firebase_GetData(const char* path);
void Firebase_Process_Incoming_Buffer(void);
uint8_t WE10_SyncStatus(const char *uid, const char *status, int wallet, const char *entry_time);
uint8_t WE10_LogEvent(const char *uid, const char *event, int fee);
#endif /* WE10_H_ */
