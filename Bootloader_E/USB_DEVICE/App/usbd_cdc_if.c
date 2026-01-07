/**
 * USB CDC Interface for Bootloader
 * Handles incoming firmware packets
 */

#include "usbd_cdc_if.h"
#include "bootload.h"
#include "stm32h7xx_hal.h"
#include <string.h>

// USB handles
extern USBD_HandleTypeDef hUsbDeviceHS;

// USB CDC receive buffer
static uint8_t UserRxBufferFS[512];
static uint8_t UserTxBufferFS[64];

// Packet assembly buffer
// CRITICAL: Must be 4-byte aligned for consistent structure alignment
__attribute__((aligned(4)))
static uint8_t PacketAssemblyBuffer[sizeof(FirmwarePacket_t)];
static uint32_t rx_buffer_index = 0;

// Processing buffer (copy of packet for main loop processing)
// CRITICAL: Must be 4-byte aligned for HAL_HASH operations on packet->data
__attribute__((aligned(4)))
static uint8_t PacketProcessingBuffer[sizeof(FirmwarePacket_t)];

// Packet ready flag (processed in main loop, not in interrupt)
static volatile uint8_t packet_ready = 0;

/**
 * @brief Clear USB packet assembly state
 * Call this at start of ENC_START to ensure clean state after reconnect
 */
void CDC_ClearPacketState(void)
{
    rx_buffer_index = 0;
    packet_ready = 0;
}

// Function prototypes (already declared in usbd_cdc_if.h)
static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len);
uint8_t CDC_Transmit_HS(uint8_t* Buf, uint16_t Len);

// USB CDC interface callbacks
USBD_CDC_ItfTypeDef USBD_Interface_fops_HS =
{
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS
};

/**
 * @brief Initialize USB CDC interface
 */
static int8_t CDC_Init_FS(void)
{
    // Set application buffers
    USBD_CDC_SetTxBuffer(&hUsbDeviceHS, UserTxBufferFS, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceHS, UserRxBufferFS);

    // CRITICAL: Clear packet assembly state on USB reconnect
    // Without this, leftover bytes from a previous disconnected transfer
    // get mixed with new packets, causing corruption
    rx_buffer_index = 0;
    packet_ready = 0;

    return USBD_OK;
}

/**
 * @brief DeInitialize USB CDC interface
 * Called on USB disconnect - clear packet state to prevent corruption on reconnect
 */
static int8_t CDC_DeInit_FS(void)
{
    // Clear packet assembly state on disconnect
    // This ensures clean state when USB reconnects
    rx_buffer_index = 0;
    packet_ready = 0;
    return USBD_OK;
}

/**
 * @brief Control USB CDC interface
 */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
    switch(cmd)
    {
    case CDC_SEND_ENCAPSULATED_COMMAND:
        break;

    case CDC_GET_ENCAPSULATED_RESPONSE:
        break;

    case CDC_SET_COMM_FEATURE:
        break;

    case CDC_GET_COMM_FEATURE:
        break;

    case CDC_CLEAR_COMM_FEATURE:
        break;

    case CDC_SET_LINE_CODING:
        break;

    case CDC_GET_LINE_CODING:
        // Return line coding (typically 115200 8N1)
        pbuf[0] = 0x00;  // 115200 baud = 0x0001C200
        pbuf[1] = 0xC2;
        pbuf[2] = 0x01;
        pbuf[3] = 0x00;
        pbuf[4] = 0x00;  // 1 stop bit
        pbuf[5] = 0x00;  // No parity
        pbuf[6] = 0x08;  // 8 data bits
        break;

    case CDC_SET_CONTROL_LINE_STATE:
        break;

    case CDC_SEND_BREAK:
        break;

    default:
        break;
    }

    return USBD_OK;
}

/**
 * @brief Data received over USB CDC
 * MATCHING MAIN FIRMWARE PATTERN: Re-enable receive FIRST, then process
 */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    // CRITICAL: Re-enable USB receive FIRST (matching main firmware pattern)
    USBD_CDC_SetRxBuffer(&hUsbDeviceHS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceHS);

    // Now process the received data
    uint32_t bytes_received = *Len;

    // DEFENSIVE: If starting fresh packet and first byte looks like ENC_START (0x11),
    // ensure we have clean state (handles edge cases where DeInit wasn't called)
    if (rx_buffer_index == 0 && bytes_received > 0) {
        if (Buf[0] == 0x11) {  // PACKET_TYPE_ENC_START
            // Starting a new ENC_START - ensure packet_ready is cleared
            packet_ready = 0;
        }
    }

    // Accumulate bytes into assembly buffer
    for (uint32_t i = 0; i < bytes_received; i++) {
        if (rx_buffer_index < sizeof(PacketAssemblyBuffer)) {
            PacketAssemblyBuffer[rx_buffer_index++] = Buf[i];
        }
    }

    // Check if we have a complete packet (272 bytes)
    if (rx_buffer_index >= sizeof(FirmwarePacket_t)) {
        // Copy to processing buffer and set flag (only if not already pending)
        if (!packet_ready) {
            memcpy(PacketProcessingBuffer, PacketAssemblyBuffer, sizeof(FirmwarePacket_t));
            packet_ready = 1;
        }
        rx_buffer_index = 0;
    }

    return USBD_OK;
}

/**
 * @brief Check if packet is ready and process it (call from main loop)
 */
void CDC_ProcessPacket(void)
{
    if (packet_ready) {
        // Process firmware packet from the processing buffer
        FirmwarePacket_t *packet = (FirmwarePacket_t *)PacketProcessingBuffer;

        // v2: Check for STATUS packet - send extended response
        if (packet->packet_type == PACKET_TYPE_STATUS) {
            BootloaderStatus_t status;
            GetBootloaderStatus(&status);
            CDC_Transmit_HS((uint8_t *)&status, sizeof(BootloaderStatus_t));
            packet_ready = 0;
            return;
        }

        // Process other packet types
        uint8_t pkt_type = packet->packet_type;  // Save before processing
        int32_t result = ProcessFirmwarePacket(packet);

        // Build standard 8-byte response
        uint8_t response[8];
        response[0] = 0xAA;  // Response header
        response[1] = pkt_type;
        response[2] = (result == 0) ? 0x01 : 0x00;  // Success/Fail
        response[3] = GetFirmwareUpdateProgress();  // Progress %
        response[4] = (uint8_t)(GetFirmwareUpdateState());
        response[5] = 0x00;
        response[6] = 0x00;
        response[7] = 0x55;  // Response footer

        // Send response
        CDC_Transmit_HS(response, 8);

        // Clear flag (receive is always re-enabled in callback)
        packet_ready = 0;
    }
}

/**
 * @brief Transmit data over USB CDC
 */
uint8_t CDC_Transmit_HS(uint8_t* Buf, uint16_t Len)
{
    uint8_t result = USBD_OK;

    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceHS.pClassData;
    if (hcdc->TxState != 0) {
        return USBD_BUSY;
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceHS, Buf, Len);
    result = USBD_CDC_TransmitPacket(&hUsbDeviceHS);

    return result;
}
