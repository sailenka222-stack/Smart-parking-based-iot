#include "we-10.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart1; // WE10 module UART
extern UART_HandleTypeDef huart2; // Debug PC UART

// --- BACKEND CONFIG ---
// This talks to your own small backend server (see parking-backend/server.js)
// over PLAIN HTTP. That server stores all the parking data directly (it
// replaces Firebase entirely), so there's no HTTPS handshake anywhere in
// this flow - which sidesteps the WE10's limited TLS stack completely.
//
// IMPORTANT: replace this with the actual IP of the machine/VM running the
// backend server (find it with `hostname -I` on that machine).
#define PROXY_HOST   "192.168.29.168"
#define PROXY_PORT   "80"

char current_user_name[32] = {0};
char current_user_vehicle[32] = {0};
int current_user_wallet = 0;
char current_user_entry_time[16] = {0};
char current_user_status[16] = {0};

// Forward declaration - implemented elsewhere (unchanged from original)
extern void send_http_cmd(const char* cmd_buffer);

/**
  * @brief  Sends a CMD+ style command to the WE10 module and waits for a
  *         terminated response instead of a fixed byte count.
  *
  *         NOTE: this used to be a GCC nested function defined inside
  *         WE10_Init(). It has been pulled out to file scope so it is
  *         portable across toolchains/linker configs (nested functions
  *         rely on stack trampolines, which break if the stack is ever
  *         marked execute-never) and so it can be reused elsewhere.
  */
static void send_cmd(const char* cmd) {
    char buffer[128];

    // Clear any leftover byte(s) sitting in the UART buffer from the
    // previous command's response - without this, one stray character from
    // the prior response gets picked up as the first byte of this one
    // (e.g. "RRSP=00" instead of "RSP=00").
    __HAL_UART_FLUSH_DRREGISTER(&huart1);
    HAL_Delay(50);

    HAL_UART_Transmit(&huart1, (uint8_t*)cmd, strlen(cmd), 1000);
    HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 1000);

    memset(buffer, 0, sizeof(buffer));
    uint16_t idx = 0;
    uint8_t rx_byte;
    uint8_t got_response = 0;
    uint32_t start_tick = HAL_GetTick();

    // Poll byte-by-byte for up to 2 seconds instead of blocking for a
    // fixed 127-byte read (which will never arrive for a short "OK\r\n").
    while ((HAL_GetTick() - start_tick) < 2000) {
        if (HAL_UART_Receive(&huart1, &rx_byte, 1, 20) == HAL_OK) {
            if (idx < sizeof(buffer) - 1) {
                buffer[idx++] = (char)rx_byte;
                buffer[idx] = '\0';
            }
            got_response = 1;

            // Stop early once a line terminator shows up so we don't
            // needlessly wait out the full timeout on every command.
            if (rx_byte == '\n' && idx > 1) {
                break;
            }
        }
    }

    HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), 1000);

    if (!got_response) {
        printf("Error receiving response for %s\n", cmd);
    } else {
        printf("Response for %s: %s\n", cmd, buffer);
    }
}

void WE10_Init(char *SSID, char *PASSWD) {
    char buffer[128];

    /********* CMD+RESET **********/
    send_cmd("CMD+RESET\r\n");
    HAL_Delay(5000); // Extended delay for reset

    /********* CMD+WIFIMODE=1 **********/
    send_cmd("CMD+WIFIMODE=1\r\n");
    HAL_Delay(2000);

    /********* CMD+CONTOAP=SSID,PASSWD **********/
    sprintf(buffer, "CMD+CONTOAP=%s,%s\r\n", SSID, PASSWD);
    send_cmd(buffer);
    HAL_Delay(5000); // Adding a delay to give module time to connect

    /********* CMD?WIFI **********/
    send_cmd("CMD?WIFI\r\n");
    HAL_Delay(2000);
}

/**
  * @brief  Sends data through the local proxy (POST /api/set), which the
  *         proxy then writes to Firebase over HTTPS on the module's behalf.
  *         Body format expected by the proxy: {"path":"<path>","value":<json_data>}
  */
void Firebase_SetData(const char* path, const char* json_data) {
    char cmd_buffer[1024];
    char body[700];

    snprintf(body, sizeof(body), "{\"path\":\"%s\",\"value\":%s}", path, json_data);

    // method 2 = POST, ssl_en = 0 (plain HTTP to the local proxy, not Firebase directly)
    snprintf(cmd_buffer, sizeof(cmd_buffer),
             "CMD+HTTP=[%s],[/api/set],[2],[%s],[0],[%s],[application/json]\r\n",
             PROXY_HOST, PROXY_PORT, body);
    printf("\r\nSending Data via Proxy...\r\n");
    send_http_cmd(cmd_buffer);
}

/**
  * @brief  Fetches data from a specific node via the local proxy (GET /api/user/:uid)
  *         instead of hitting Firebase's HTTPS endpoint directly.
  */
void Firebase_GetData(const char* path) {
    char cmd_buffer[512];

    // method 1 = GET, ssl_en = 0 (plain HTTP to the local proxy)
    snprintf(cmd_buffer, sizeof(cmd_buffer),
             "CMD+HTTP=[%s],[/%s],[1],[%s],[0]\r\n", PROXY_HOST, path, PROXY_PORT);

    printf("\r\nFetching Data via Proxy...\r\n");
    send_http_cmd(cmd_buffer);
}

/**
  * @brief  Fetches user object from cloud and dynamically parses attributes.
  * @retval 1 if User found, 0 if User not found or connection error.
  */
uint8_t WE10_CheckUser(char *uid)
{
    char cmd[350];
    static char response[1500];
    uint16_t resp_idx = 0;
    uint8_t rx_byte;

    memset(current_user_name, 0, sizeof(current_user_name));
    memset(current_user_vehicle, 0, sizeof(current_user_vehicle));
    memset(response, 0, sizeof(response));

    // Per Celium WE10 Command Document (REV 6.1), section 4.4.1: every CMD+HTTP
    // parameter must be wrapped in literal square brackets. Omitting them causes
    // the module to reject the command with RSP=02 (Invalid Parameter).
    //
    // NOTE: this now goes through the local proxy (see firebase-proxy/server.js)
    // over plain HTTP instead of hitting Firebase's HTTPS endpoint directly,
    // since the WE10's embedded TLS stack cannot reliably complete a handshake
    // with Google's frontend. The proxy exposes GET /api/user/<uid> and does
    // the actual HTTPS call to Firebase on the module's behalf.
    // Format: CMD+HTTP=[host],[path],[method(1=GET)],[port],[ssl_enable(0=plain HTTP)]
    snprintf(cmd, sizeof(cmd),
             "CMD+HTTP=[%s],[/api/user/%s],[1],[%s],[0]\r\n",
             PROXY_HOST, uid, PROXY_PORT);

    printf("\r\nChecking RFID Tag in Database...\r\n");

    // Clear hardware flags and wait a moment for the radio stack to be idle
    __HAL_UART_FLUSH_DRREGISTER(&huart1);
    HAL_Delay(200);

    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);

    // Collect response string
    uint32_t start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < 6000) { // 6-second window for secure handshakes
        if (HAL_UART_Receive(&huart1, &rx_byte, 1, 10) == HAL_OK) {
            HAL_UART_Transmit(&huart2, &rx_byte, 1, 10); // Mirror to PC screen

            if (resp_idx < sizeof(response) - 1) {
                response[resp_idx++] = (char)rx_byte;
                response[resp_idx] = '\0';
            }

            // Look for completion event
            if (strstr(response, "EVT+HTTPSTATUS=") != NULL && rx_byte == '\n') {
                HAL_Delay(300); // Wait for remaining trailing characters
                while(HAL_UART_Receive(&huart1, &rx_byte, 1, 10) == HAL_OK) {
                    if (resp_idx < sizeof(response) - 1) {
                        response[resp_idx++] = (char)rx_byte;
                        response[resp_idx] = '\0';
                    }
                }
                break;
            }
        }
    }

    // --- CRITICAL DEBUG PRINT ---
    printf("\r\n--- RAW MODULE RESPONSE START ---\r\n");
    printf("%s", response);
    printf("\r\n--- RAW MODULE RESPONSE END ---\r\n");
    // ----------------------------

    // The proxy returns {"error":"not_found"} for a missing user, and "null"
    // if it ever forwards a raw Firebase response directly.
    if (resp_idx == 0 || strstr(response, "\"error\"") != NULL || strstr(response, "null") != NULL) {
        printf("\r\n[Firebase Info]: User Tag not found in database.\r\n");
        return 0;
    }

    // Parse Name
    char *name_ptr = strstr(response, "\"name\":\"");
    if (name_ptr != NULL) {
        name_ptr += 8;
        char *end_ptr = strchr(name_ptr, '"');
        if (end_ptr != NULL) {
            size_t len = end_ptr - name_ptr;
            if(len > 31) len = 31;
            strncpy(current_user_name, name_ptr, len);
            current_user_name[len] = '\0';
        }
    }

    // Parse Vehicle
    char *vehicle_ptr = strstr(response, "\"vehicle\":\"");
    if (vehicle_ptr != NULL) {
        vehicle_ptr += 11;
        char *end_ptr = strchr(vehicle_ptr, '"');
        if (end_ptr != NULL) {
            size_t len = end_ptr - vehicle_ptr;
            if(len > 31) len = 31;
            strncpy(current_user_vehicle, vehicle_ptr, len);
            current_user_vehicle[len] = '\0';
        }
    }

    // Parse Wallet (numeric field, not quoted)
    char *wallet_ptr = strstr(response, "\"wallet\":");
    if (wallet_ptr != NULL) {
        wallet_ptr += 9;
        current_user_wallet = atoi(wallet_ptr);
    }


    // Parse EntryTime - stored on the backend when a user enters, so the
    // exit-side fee calculation survives an STM32 reset in between.
    memset(current_user_entry_time, 0, sizeof(current_user_entry_time));
    char *entry_ptr = strstr(response, "\"entryTime\":\"");
    if (entry_ptr != NULL) {
        entry_ptr += 13;
        char *end_ptr = strchr(entry_ptr, '"');
        if (end_ptr != NULL) {
            size_t len = end_ptr - entry_ptr;
            if (len > 15) len = 15;
            strncpy(current_user_entry_time, entry_ptr, len);
            current_user_entry_time[len] = '\0';
        }
    }

    // Parse Status ("INSIDE" or "OUTSIDE") - this is the source of truth for
    // whether this specific card is currently inside, since it's persisted
    // on the backend and survives an STM32 reset (unlike a local variable).
    memset(current_user_status, 0, sizeof(current_user_status));
    char *status_ptr = strstr(response, "\"status\":\"");
    if (status_ptr != NULL) {
        status_ptr += 10;
        char *end_ptr = strchr(status_ptr, '"');
        if (end_ptr != NULL) {
            size_t len = end_ptr - status_ptr;
            if (len > 15) len = 15;
            strncpy(current_user_status, status_ptr, len);
            current_user_status[len] = '\0';
        }
    }


    if (strlen(current_user_name) > 0) {
        printf("\r\n>>> MATCH FOUND! User: %s | Vehicle: %s\r\n", current_user_name, current_user_vehicle);
        return 1;
    }

    return 0;
}

/**
  * @brief  Generic helper: sends an HTTP command and waits for either
  *         RSP=00 + EVT+HTTPSTATUS (success) or a failure event, without
  *         needing to parse the response body. Used by functions that only
  *         need to know "did this write succeed", not read data back.
  * @retval 1 if the request completed (EVT+HTTPSTATUS seen), 0 on failure/timeout.
  */
static uint8_t send_http_and_wait(const char* cmd) {
    static char response[512];
    uint16_t resp_idx = 0;
    uint8_t rx_byte;

    memset(response, 0, sizeof(response));

    __HAL_UART_FLUSH_DRREGISTER(&huart1);
    HAL_Delay(200);

    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);

    uint32_t start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < 4000) {
        if (HAL_UART_Receive(&huart1, &rx_byte, 1, 10) == HAL_OK) {
            HAL_UART_Transmit(&huart2, &rx_byte, 1, 10);

            if (resp_idx < sizeof(response) - 1) {
                response[resp_idx++] = (char)rx_byte;
                response[resp_idx] = '\0';
            }

            if ((strstr(response, "EVT+HTTPSTATUS=") != NULL ||
                 strstr(response, "EVT+HTTPCONFAIL") != NULL) && rx_byte == '\n') {
                break;
            }
        }
    }

    printf("\r\n--- HTTP WRITE RESPONSE ---\r\n%s\r\n---------------------------\r\n", response);

    return (strstr(response, "EVT+HTTPSTATUS=") != NULL);
}

/**
  * @brief  Pushes a user's updated status ("INSIDE"/"OUTSIDE") and wallet
  *         balance to the backend. Reuses the existing PUT /api/user/:uid
  *         endpoint, which merges fields rather than overwriting the whole
  *         record - so name/vehicle/type are preserved automatically.
  * @retval 1 on success, 0 on failure.
  */
uint8_t WE10_SyncStatus(const char *uid, const char *status, int wallet, const char *entry_time) {
    char cmd[500];
    char body[200];

    if (entry_time != NULL && strlen(entry_time) > 0) {
        snprintf(body, sizeof(body), "{\"status\":\"%s\",\"wallet\":%d,\"entryTime\":\"%s\"}", status, wallet, entry_time);
    } else {
        // On exit, clear entryTime so a stale value can't leak into the next session.
        snprintf(body, sizeof(body), "{\"status\":\"%s\",\"wallet\":%d,\"entryTime\":\"\"}", status, wallet);
    }

    snprintf(cmd, sizeof(cmd),
             "CMD+HTTP=[%s],[/api/user/%s],[3],[%s],[0],[%s],[application/json]\r\n",
             PROXY_HOST, uid, PROXY_PORT, body);

    printf("\r\nSyncing status to backend...\r\n");
    return send_http_and_wait(cmd);
}

/**
  * @brief  Logs an ENTRY or EXIT event (with fee, if any) to the backend's
  *         history log via POST /api/log.
  * @retval 1 on success, 0 on failure.
  */
uint8_t WE10_LogEvent(const char *uid, const char *event, int fee) {
    char cmd[400];
    char body[100];

    snprintf(body, sizeof(body), "{\"uid\":\"%s\",\"event\":\"%s\",\"fee\":%d}", uid, event, fee);

    snprintf(cmd, sizeof(cmd),
             "CMD+HTTP=[%s],[/api/log],[2],[%s],[0],[%s],[application/json]\r\n",
             PROXY_HOST, PROXY_PORT, body);

    printf("\r\nLogging event to backend...\r\n");
    return send_http_and_wait(cmd);
}

/**
  * @brief  Pushes live slot1/slot2 occupancy to the backend so
  *         visualization.html can show real-time car placement.
  *         Reuses the existing PUT /api/slots endpoint.
  * @retval 1 on success, 0 on failure.
  */
uint8_t WE10_SyncSlots(uint8_t slot1_occupied, uint8_t slot2_occupied) {
    char cmd[300];
    char body[80];

    snprintf(body, sizeof(body), "{\"slot1\":%s,\"slot2\":%s}",
             slot1_occupied ? "true" : "false",
             slot2_occupied ? "true" : "false");

    snprintf(cmd, sizeof(cmd),
             "CMD+HTTP=[%s],[/api/slots],[3],[%s],[0],[%s],[application/json]\r\n",
             PROXY_HOST, PROXY_PORT, body);

    return send_http_and_wait(cmd);
}

/**
  * @brief  Processes HTTP updates asynchronously
  */
void Firebase_Process_Incoming_Buffer(void) {
    static char rx_buffer[1024];
    static uint16_t rx_index = 0;
    uint8_t single_byte;

    while (HAL_UART_Receive(&huart1, &single_byte, 1, 0) == HAL_OK) {
        HAL_UART_Transmit(&huart2, &single_byte, 1, 10);

        if (rx_index < sizeof(rx_buffer) - 1) {
            rx_buffer[rx_index++] = (char)single_byte;
            rx_buffer[rx_index] = '\0';
        }

        if (single_byte == '\n') {
            if (strstr(rx_buffer, "EVT+HTTPSTATUS=") != NULL) {
                char *payload_ptr = NULL;
                int comma_count = 0;

                for (int i = 0; rx_buffer[i] != '\0'; i++) {
                    if (rx_buffer[i] == ',') {
                        comma_count++;
                        if (comma_count == 2) {
                            payload_ptr = &rx_buffer[i + 1];
                            break;
                        }
                    }
                }

                if (payload_ptr != NULL) {
                    printf("\r\n[Firebase Data Received]: %s\r\n", payload_ptr);

                    if (strstr(payload_ptr, "\"ON\"") != NULL || strstr(payload_ptr, "ON") != NULL) {
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
                        printf("[Action] LEDs turned ON\r\n");
                    }
                    else if (strstr(payload_ptr, "\"OFF\"") != NULL || strstr(payload_ptr, "OFF") != NULL) {
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
                        printf("[Action] LEDs turned OFF\r\n");
                    }
                }
            }
            rx_index = 0;
        }
    }
}
