#include <stdio.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "esp_log.h"
#include "string.h"
#include "esp_timer.h"
#include <arpa/inet.h>
#include <stdio.h>

#define MAX_QUEUE_SIZE 10

QueueHandle_t twai_queue;
static const int RX_BUF_SIZE = 1024;
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)
#define COMMAND_LENGTH 15

static const char *TAG = "MAIN";
SemaphoreHandle_t xSemaphore;

// int packet_loss_count=0;

// functions Defenations
void send_can_message(const twai_message_t *message);
void TWAI_DEACTIVATE(void);

typedef struct
{
    uint32_t can_id;
    uint8_t extORstd;
    uint8_t payload_length;
    uint8_t payload[8];
    int ID;
    TickType_t interval;
} TwaiTaskParams;
TwaiTaskParams arr_of_sending_cmds_struct[10];
uint64_t lastSendTime[10] = {0};
int no_of_active_sending_cmd = -1;
int test_cmd_cnt = 0;


void sendTwaiMessage(int *ID)
{
    printf("IIIIIIIIIIIIIIIIIIIIIIIIIDDDDDDDDDDDDD%d esptimer time%lld\n", *ID,esp_timer_get_time());

    // Create a TWAI message
    twai_message_t message;
    message.identifier = arr_of_sending_cmds_struct[*ID].can_id;
    printf("CAN ID%lx",arr_of_sending_cmds_struct[*ID].can_id);
    message.extd = arr_of_sending_cmds_struct[*ID].extORstd;
    message.self = 0;
    message.data_length_code = arr_of_sending_cmds_struct[*ID].payload_length;
    memcpy(message.data, arr_of_sending_cmds_struct[*ID].payload, arr_of_sending_cmds_struct[*ID].payload_length);
    ESP_LOGI("TAG", "Configuring filter %lx payloadlength %d frame_formate %d payload first byte %d intervals %ld", arr_of_sending_cmds_struct[*ID].can_id, message.data_length_code, message.extd, message.data[0], arr_of_sending_cmds_struct[*ID].interval); // Change "04" to remove leading zeros

    // Send the TWAI message
    if (twai_transmit(&message, portMAX_DELAY) != ESP_OK)
    {
        printf("Failed to send TWAI message\n");
    }
    else
    {
        printf("Sent TWAI message with CAN ID %lx\n", message.identifier);
    }
}









void twai_transmit_task(void *pvParameters) {
    int temp;

    while (1) {
        // Check if there is a message in the queue
        if (xQueueReceive(twai_queue, &temp, portMAX_DELAY)) {
            // Transmit the TWAI message
            sendTwaiMessage(&temp);
        }
    }
}



void timer_callback(void *param)
{
    // printf("qwertyui\n");
    for (int i = 0; i < 10; i++)
    {

                // if (arr_of_sending_cmds_struct[i].ID > 0 && xTaskGetTickCount() % pdMS_TO_TICKS(arr_of_sending_cmds_struct[i].interval) == 0) {
                //     // printf("Printing arr_of_sending_cmds_struct[%d]:\n", i);
                //     // printf("  can_id: %lx\n", arr_of_sending_cmds_struct[i].can_id);
                //     // printf("  extORstd: %u\n", arr_of_sending_cmds_struct[i].extORstd);
                //     // printf("  payload_length: %u\n", arr_of_sending_cmds_struct[i].payload_length);

                //     // printf("  payload: ");
                //     // for (int j = 0; j < 8; j++) {
                //     //     printf("%u ", arr_of_sending_cmds_struct[i].payload[j]);
                //     // }
                //     // printf("\n");

                //     // printf("  ID: %d\n", arr_of_sending_cmds_struct[i].ID);
                //     // printf("  interval: %ld\n", arr_of_sending_cmds_struct[i].interval);

                //     // printf("\n");
                //     // Send TWAI message here using twaiParamsArray[i]
                //     timestamp =xTaskGetTickCount();
                //     ESP_LOGI("Inside TWAI Task Loop","Sending TWAI message ID %d  with intervals %ld and espxTaskGetTickCount %lld     ", arr_of_sending_cmds_struct[i].ID,arr_of_sending_cmds_struct[i].interval,timestamp);
                //     sendTwaiMessage(&arr_of_sending_cmds_struct[i]);
                //     test_cmd_cnt=test_cmd_cnt+1;
                //     printf("sent cmd_cnt%d\n",test_cmd_cnt);
                    
                // }


















        if (arr_of_sending_cmds_struct[i].ID > 0)
        {
            uint64_t currentTime = esp_timer_get_time();
            uint64_t intervalMicros = arr_of_sending_cmds_struct[i].interval * 1000; // Convert interval to microseconds

            if (currentTime - lastSendTime[i] >= intervalMicros)
            {



                    int temp=arr_of_sending_cmds_struct[i].ID;              
                    if (xQueueSend(twai_queue, (&temp), 0) != pdPASS) {
                    ESP_LOGI("timer_callback","Failed to enqueue the message (queue full)");
        
    }
    

                test_cmd_cnt = test_cmd_cnt + 1;
                // printf("sent cmd_cnt%d\n", test_cmd_cnt);

                // Update the last send time
                lastSendTime[i] = currentTime;
            }
        }
    }

}





// Function definations end
// TWAI INIT
void canbus_init(int filter_idd, int filter_mask, int baudrate, int extORstd)
{

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_21, GPIO_NUM_22, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = 100;
    g_config.rx_queue_len = 200;
    twai_filter_config_t f_config;
    if (extORstd == 1)
    {
        // t_config= TWAI_TIMING_CONFIG_1MBITS();
        // twai_timing_config_t t_config; = TWAI_TIMING_CONFIG_1MBITS();
        f_config.acceptance_code = (filter_idd << 3);
        f_config.acceptance_mask = ~(filter_mask << 3);
        f_config.single_filter = true;
    }
    else if (extORstd == 0)
    {
        // Configure filter and filter mask for CAN Standard Frame
        f_config.acceptance_code = filter_idd << 21;     // Shift the standard identifier
        f_config.acceptance_mask = ~(filter_mask << 21); // Shift the mask
        f_config.single_filter = true;
    }

    else
    {
        ESP_LOGI(TAG, "Write correct extORstd bit");
    }

    if (baudrate == 25000)
    {
        twai_timing_config_t t_config_25kb = TWAI_TIMING_CONFIG_25KBITS();
        // Install TWAI driver
        if (twai_driver_install(&g_config, &t_config_25kb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else if (baudrate == 50000)
    {
        twai_timing_config_t t_config_50kb = TWAI_TIMING_CONFIG_50KBITS();
        if (twai_driver_install(&g_config, &t_config_50kb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else if (baudrate == 100000)
    {
        twai_timing_config_t t_config_100kb = TWAI_TIMING_CONFIG_100KBITS();
        if (twai_driver_install(&g_config, &t_config_100kb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else if (baudrate == 125000)
    {
        twai_timing_config_t t_config_125kb = TWAI_TIMING_CONFIG_125KBITS();
        if (twai_driver_install(&g_config, &t_config_125kb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else if (baudrate == 250000)
    {
        twai_timing_config_t t_config_250kb = TWAI_TIMING_CONFIG_250KBITS();
        if (twai_driver_install(&g_config, &t_config_250kb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else if (baudrate == 500000)
    {
        twai_timing_config_t t_config_500kb = TWAI_TIMING_CONFIG_500KBITS();
        if (twai_driver_install(&g_config, &t_config_500kb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else if (baudrate == 800000)
    {
        twai_timing_config_t t_config_800kb = TWAI_TIMING_CONFIG_800KBITS();
        if (twai_driver_install(&g_config, &t_config_800kb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else if (baudrate == 1000000)
    {
        twai_timing_config_t t_config_1mb = TWAI_TIMING_CONFIG_1MBITS();
        if (twai_driver_install(&g_config, &t_config_1mb, &f_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "Driver installed");
        }
        else
        {
            ESP_LOGI(TAG, "Failed to install driver");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Write correct baud rate");
    }

    // Start TWAI driver
    if (twai_start() == ESP_OK)
    {
        ESP_LOGI(TAG, "Driver started");
    }
    else
    {
        ESP_LOGI(TAG, "Failed to start driver");
    }
}

void parseCommand(const uint8_t *command, size_t length)
{
    // Check if the command length matches expectations
    if (length == COMMAND_LENGTH)
    {
        // Check if the first byte of the command is 0x02
        if (command[0] == 0x02)
        {
            // Extract filter ID, filter mask, and bitrate from the command
            uint32_t filter_id, filter_mask;
            int bitrate, Extorstd;
            Extorstd = command[2];
            memcpy(&filter_id, &command[3], sizeof(filter_id));
            memcpy(&filter_mask, &command[7], sizeof(filter_mask));
            memcpy(&bitrate, &command[11], sizeof(bitrate));

            // Convert from network byte order (big-endian) to host byte order
            // filter_id = ntohl(filter_id);
            // filter_mask = ntohl(filter_mask);
            bitrate = ntohl(bitrate);

            // Configure TWAI with the received parameters
            ESP_LOGI(TAG, "Configuring filter %lx mask %lx bitrate %d Filter Format %d", filter_id, filter_mask, bitrate, Extorstd);
            // xSemaphoreTake(xSemaphore,portMAX_DELAY);
            TWAI_DEACTIVATE();
            canbus_init(filter_id, filter_mask, bitrate, Extorstd);
            // xSemaphoreGive(xSemaphore);
        }
    }
    if (length <= 16 && length >= 9)
    {
        if (command[0] == 0x03)
        {

            // Check the format byte (data[1])
            twai_message_t message;
            message.self = 0;
            message.extd = (command[2] == 0x01) ? 1 : 0;
            message.identifier = ntohl((command[3] << 24) | (command[4] << 16) | (command[5] << 8) | command[6]);
            int payload_len = command[7];
            message.data_length_code = payload_len;

            for (int i = 0; i < payload_len; i++)
            {
                message.data[i] = command[i + 8];
            }

            // Send the CAN message
            // xSemaphoreTake(xSemaphore, portMAX_DELAY);
            send_can_message(&message);
            // ESP_LOGI(TAG, "Sending CAN cmd ,  CAN Format %x CAN ID %lx CAN Payload %x",message.extd,message.identifier,message.data[0]);
            // xSemaphoreGive(xSemaphore);
        }
    }
    if (length <= 22 && length >= 15)
    {
        if (command[0] == 0x04)
        {

            int commad_len = command[1];
            // Check the format byte (data[1])
            twai_message_t message;
            message.self = 0;
            message.extd = (command[2] == 0x01) ? 1 : 0;
            message.identifier = ntohl((command[3] << 24) | (command[4] << 16) | (command[5] << 8) | command[6]);
            int payload_len = command[7];
            message.data_length_code = payload_len;
            int ID_address_in_uartcmd = 8 + payload_len;
            int ID = command[ID_address_in_uartcmd];
            uint32_t interverls = (command[ID_address_in_uartcmd + 1] << 24) | (command[ID_address_in_uartcmd + 2] << 16) | (command[ID_address_in_uartcmd + 3] << 8) | command[ID_address_in_uartcmd + 4];
            int start_stop = command[ID_address_in_uartcmd + 5];

            for (int i = 0; i < payload_len; i++)
            {
                message.data[i] = command[i + 8];
            }

            ESP_LOGI(TAG, "04Configuring filter %lx 04payloadlength %d 04payload first byte %d 04intervals %ld cmd_start_stop_bit %d", message.identifier, message.data_length_code, message.data[0], interverls, start_stop);

            if (start_stop == 1)
            {
                // Create a structure with the desired parameters
                no_of_active_sending_cmd = no_of_active_sending_cmd + 1;
                arr_of_sending_cmds_struct[ID].ID = ID;
                arr_of_sending_cmds_struct[ID].can_id = message.identifier;
                arr_of_sending_cmds_struct[ID].extORstd = message.extd;
                arr_of_sending_cmds_struct[ID].payload_length = message.data_length_code;
                memcpy(arr_of_sending_cmds_struct[ID].payload, message.data, arr_of_sending_cmds_struct[ID].payload_length);
                arr_of_sending_cmds_struct[ID].interval = interverls;
            }
            else if (start_stop == 0)
            {
                no_of_active_sending_cmd = no_of_active_sending_cmd - 1;
                arr_of_sending_cmds_struct[ID].ID = -15;
                arr_of_sending_cmds_struct[ID].can_id = 0;
                arr_of_sending_cmds_struct[ID].extORstd = 0;
                arr_of_sending_cmds_struct[ID].payload_length = 0;
                memcpy(arr_of_sending_cmds_struct[ID].payload, message.data, arr_of_sending_cmds_struct[ID].payload_length);
                arr_of_sending_cmds_struct[ID].interval = pdMS_TO_TICKS(1000);

                // no_of_active_sending_cmd = no_of_active_sending_cmd-1;
                // char sending_cmd_ID[5];
                // snprintf(sending_cmd_ID, sizeof(sending_cmd_ID), "ID", ID);

                // char taskName[16];
                // snprintf(taskName, sizeof(taskName), "TWAI_Task_%d", ID);
                // TaskHandle_t taskHandle = xTaskGetHandle(taskName);
                // ESP_LOGI("Inside TASK Delete function","Task with name %s deleted\n", taskName);
                // if (taskHandle != NULL) {
                //     vTaskDelete(taskHandle);
                //     ESP_LOGI("Inside TASK Delete function","Task with name %s deleted\n", taskName);
                // } else {
                //     ESP_LOGI("Inside TASK Delete function","Task with name %s not found\n", taskName);

                // }
            }
        }
    }
}

void printCANData(const twai_message_t *message)
{
    uint8_t buffer[32]; // Adjust the buffer size as needed

    uint64_t timestamp = esp_timer_get_time();
    uint8_t frameType = (message->extd) ? 1 : 0;
    uint32_t canId = message->identifier;
    uint8_t dlc = message->data_length_code; // Directly use dlc as 1 byte

    int offset = 0;

    // Append '01' to the buffer
    buffer[offset++] = 0x01;
    int frameLength = sizeof(timestamp) + sizeof(frameType) + sizeof(canId) + sizeof(dlc) + dlc;

    buffer[offset++] = frameLength;
    // Append timestamp to the buffer
    memcpy(buffer + offset, &timestamp, sizeof(timestamp));
    offset += sizeof(timestamp);
    ESP_LOGI(TAG, "inside printDATA function called by TWAI_RX task ");

    // Append frameType to the buffer
    buffer[offset++] = frameType;

    // Append CAN ID to the buffer
    memcpy(buffer + offset, &canId, sizeof(canId));
    offset += sizeof(canId);

    // Append DLC to the buffer as a single byte
    buffer[offset++] = dlc; // Use dlc as a single byte

    // Append data bytes to the buffer
    memcpy(buffer + offset, message->data, dlc);
    offset += dlc;

    // Append '\r' to the buffer
    // buffer[offset++] = '\r';
    // buffer[offset++] = '';

    // Send the entire buffer over UART
    uart_write_bytes(UART_NUM_1, (const char *)buffer, offset);
}

// For UART

void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 460800,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char *data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    // ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

// void tx_task(void *arg)
// {
//     static const char *TX_TASK_TAG = "TX_TASK";
//     esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
//     while (1) {
//         // sendData( "Hello world");
//         vTaskDelay(2000 / portTICK_PERIOD_MS);
//     }
// }
void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);
    while (1)
    {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (rxBytes > 0)
        {
            data[rxBytes] = 0;
            ESP_LOGI(TAG, "Received command bytes: ");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, rxBytes, ESP_LOG_INFO);
            // Parse the received command
            parseCommand(data, rxBytes);
        } // ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
          // ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
    }
}

// UART ENDs

static void twaitask(void *arg)
{

    twai_message_t message = {0};

    while (1)
    {
        // xSemaphoreTake(xSemaphore, portMAX_DELAY);
        if (twai_receive(&message, pdMS_TO_TICKS(10000)) == ESP_OK)
        {
            // ESP_LOGI(TAG, "Message received");

            // if (message.extd) {
            //     ESP_LOGI(TAG, "Message is in Extended Format");
            // } else {
            //     ESP_LOGI(TAG, "Message is in Standard Format");
            // }
            // ESP_LOGI(TAG, "ID is %lx", message.identifier);
            if (!(message.rtr))
            {
                // for (int i = 0; i < message.data_length_code; i++) {
                //     ESP_LOGI(TAG, "Data byte %d = %d", i, message.data[i]);
                // }
                ESP_LOGI(TAG, "CAN RX_TASK Message Format %d  ID is %lx   Data byte  =  %d %d %d %d ", message.extd, message.identifier, message.data[0], message.data[1], message.data[2], message.data[3]);
                ESP_LOGI(TAG, "RTR: %d", message.rtr);
                printCANData(&message);
            }
        }
        else
        {
            // ESP_LOGI(TAG, "Failed to receive message");
        }
        // xSemaphoreGive(xSemaphore);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// void twaitask_send_in_loop(void *arg)
// {

//     int test_cmd_cnt = 0;

//     uint64_t lastSendTime[10] = {0}; // Array to store the last send time for each command

//     while (1)
//     {
//         for (int i = 0; i < 10; i++)
//         {
//             if (arr_of_sending_cmds_struct[i].ID > 0)
//             {
//                 uint64_t currentTime = esp_timer_get_time();
//                 uint64_t intervalMicros = arr_of_sending_cmds_struct[i].interval * 1000; // Convert interval to microseconds

//                 if (currentTime - lastSendTime[i] >= intervalMicros)
//                 {
//                     // Send TWAI message here using arr_of_sending_cmds_struct[i]
//                     printf("Sending TWAI message for ID %d\n", arr_of_sending_cmds_struct[i].ID);
//                     sendTwaiMessage(&arr_of_sending_cmds_struct[i]);

//                     ESP_LOGI("Inside TWAI Task Loop", "Sending TWAI message ID %d  with intervals %ld   ", arr_of_sending_cmds_struct[i].ID, arr_of_sending_cmds_struct[i].interval);
//                     test_cmd_cnt = test_cmd_cnt + 1;
//                     printf("sent cmd_cnt%d\n", test_cmd_cnt);

//                     // Update the last send time
//                     lastSendTime[i] = currentTime;
//                 }
//             }
//         }

//         // Wait for the next iteration
//         vTaskDelay(1);
//     }
// }

void app_main()
{
    for (int k = 0; k < 10; k++)
    {
        arr_of_sending_cmds_struct[k].ID = -15;
    }

    xSemaphore = xSemaphoreCreateMutex();
    uart_init();
    canbus_init(0x00000000, 0x00000000, 500000, 1);
    // Create the TWAI message queue
    twai_queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(int));

    // Create the task to transmit TWAI messages
   


    const esp_timer_create_args_t my_timer_args = {
        .callback = &timer_callback,
        .name = "My Timer"};
    esp_timer_handle_t timer_handler;
    ESP_ERROR_CHECK(esp_timer_create(&my_timer_args, &timer_handler));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handler, 1000));


    //     twai_message_t tx_message = {
    //     .identifier = 0x123,  // CAN message identifier
    //     .data_length_code = 8, // Number of bytes in the message payload
    //     .data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08} // Message payload
    // };
    // while (1)
    // {
    //     /* code */
    //     send_can_message(&tx_message);
    //     vTaskDelay(1000);

    // }
    
    xTaskCreate(rx_task, "uart_rx_task", 4 * 1024, NULL, 1, NULL);
    // xTaskCreate(tx_task, "uart_tx_task", 1024, NULL, 1, NULL);
    xTaskCreate(twaitask, "twaiTASK", 4096, NULL, 1, NULL);
    xTaskCreate(twai_transmit_task, "twai_transmit_task", configMINIMAL_STACK_SIZE * 2, NULL, 5, NULL);
    // xTaskCreate(twaitask_send_in_loop, "twaiTASK_in_loop", 4096, NULL, 1, NULL);
}

void send_can_message(const twai_message_t *message)
{
    if (message == NULL)
    {
        ESP_LOGE("TWAI", "Invalid message pointer");
        return;
    }

    if (message->data_length_code > 8)
    {
        ESP_LOGE("TWAI", "Data length code exceeds maximum");
        return;
    }

    twai_message_t tx_message;
    tx_message.extd = message->extd;
    tx_message.identifier = message->identifier;
    tx_message.data_length_code = message->data_length_code;
    tx_message.self = 0;

    for (int i = 0; i < 8; i++)
    {
        tx_message.data[i] = (i < message->data_length_code) ? message->data[i] : 0;
    }

    if (twai_transmit(&tx_message, pdMS_TO_TICKS(10000)) == ESP_OK)
    {
        ESP_LOGI("Send from UART to CAN BUS", "Message transmitted");
        ESP_LOGI("Send from UART to CAN BUS", "Transmitting CAN Message:");
        ESP_LOGI("Send from UART to CAN BUS", "  Format: %s", tx_message.extd ? "Extended" : "Standard");
        ESP_LOGI("Send from UART to CAN BUS", "  ID: 0x%lX", tx_message.identifier);
        ESP_LOGI("Send from UART to CAN BUS", "  DLC: %d", tx_message.data_length_code);
        ESP_LOGI("Send from UART to CAN BUS", "  Data:");

        for (int i = 0; i < tx_message.data_length_code; i++)
        {
            ESP_LOGI("TWAI", "    Byte %d: %02X", i, tx_message.data[i]);
        }
    }
    else
    {
        ESP_LOGE("TWAI", "Failed to transmit message");
    }
}

void TWAI_DEACTIVATE(void)
{
    // Stop the TWAI driver
    esp_err_t stopRet = twai_stop();
    if (stopRet == ESP_OK)
    {
        ESP_LOGI(TAG, "Driver stopped");
    }
    else
    {
        ESP_LOGI(TAG, "Failed to stop driver: %d", stopRet);
        return;
    }

    // Uninstall the TWAI driver
    esp_err_t ret = twai_driver_uninstall();
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Driver uninstalled");
    }
    else
    {
        ESP_LOGI(TAG, "Failed to uninstall driver: %d", ret);
        return;
    }
}
