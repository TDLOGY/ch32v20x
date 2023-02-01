/********************************** (C) COPYRIGHT *******************************
 * File Name          : ble_uart_service_16bit_char.c
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2022/01/19
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "CONFIG.h"
#include "gattprofile.h"
#include "stdint.h"
#include "ble_uart_service.h"


/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#define SERVAPP_NUM_ATTR_SUPPORTED    7

#define RAWPASS_TX_VALUE_HANDLE       2
#define RAWPASS_RX_VALUE_HANDLE       5
/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

// ble_uart GATT Profile Service UUID
const uint8_t ble_uart_ServiceUUID[ATT_BT_UUID_SIZE] =
    {0xf0, 0xff};

// Characteristic rx uuid
const uint8_t ble_uart_RxCharUUID[ATT_BT_UUID_SIZE] =
    {0xf2, 0xff};

// Characteristic tx uuid
const uint8_t ble_uart_TxCharUUID[ATT_BT_UUID_SIZE] =
    {0xf1, 0xff};

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static ble_uart_ProfileChangeCB_t ble_uart_AppCBs = NULL;

/*********************************************************************
 * Profile Attributes - variables
 */

// Profile Service attribute
static const gattAttrType_t ble_uart_Service = {ATT_BT_UUID_SIZE, ble_uart_ServiceUUID};

// Profile Characteristic 1 Properties
//static uint8_t ble_uart_RxCharProps = GATT_PROP_WRITE_NO_RSP| GATT_PROP_WRITE;
static uint8_t ble_uart_RxCharProps = GATT_PROP_WRITE_NO_RSP | GATT_PROP_WRITE;

// Characteristic 1 Value
static uint8_t ble_uart_RxCharValue[BLE_UART_RX_BUFF_SIZE];
//static uint8_t ble_uart_RxCharValue[1];

// Profile Characteristic 2 Properties
//static uint8_t ble_uart_TxCharProps = GATT_PROP_NOTIFY| GATT_PROP_INDICATE;
static uint8_t ble_uart_TxCharProps = GATT_PROP_NOTIFY;

// Characteristic 2 Value
static uint8_t ble_uart_TxCharValue = 0;

// Simple Profile Characteristic 2 User Description
static gattCharCfg_t ble_uart_TxCCCD[4];

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t ble_uart_ProfileAttrTbl[] = {
    // Simple Profile Service
    {
        {ATT_BT_UUID_SIZE, primaryServiceUUID}, /* type */
        GATT_PERMIT_READ,                       /* permissions */
        0,                                      /* handle */
        (uint8_t *)&ble_uart_Service              /* pValue */
    },

    // Characteristic 2 Declaration
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &ble_uart_TxCharProps},

    // Characteristic Value 2
    {
        {ATT_BT_UUID_SIZE, ble_uart_TxCharUUID},
        0,
        0,
        (uint8_t *)&ble_uart_TxCharValue},

    // Characteristic 2 User Description
    {
        {ATT_BT_UUID_SIZE, clientCharCfgUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t *)ble_uart_TxCCCD},

    // Characteristic 1 Declaration
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &ble_uart_RxCharProps},

    // Characteristic Value 1
    {
        {ATT_BT_UUID_SIZE, ble_uart_RxCharUUID},
        GATT_PERMIT_WRITE,
        0,
        &ble_uart_RxCharValue[0]},

};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t ble_uart_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                     uint8_t *pValue, uint16_t *pLen, uint16_t offset, uint16_t maxLen, uint8_t method);
static bStatus_t ble_uart_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                      uint8_t *pValue, uint16_t len, uint16_t offset, uint8_t method);

static void ble_uart_HandleConnStatusCB(uint16_t connHandle, uint8_t changeType);

/*********************************************************************
 * PROFILE CALLBACKS
 */
// Simple Profile Service Callbacks
gattServiceCBs_t ble_uart_ProfileCBs = {
    ble_uart_ReadAttrCB,  // Read callback function pointer
    ble_uart_WriteAttrCB, // Write callback function pointer
    NULL                  // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      ble_uart_AddService
 *
 * @brief   Initializes the Simple Profile service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 *
 * @return  Success or Failure
 */
bStatus_t ble_uart_add_service(ble_uart_ProfileChangeCB_t cb)
{
    uint8_t status = SUCCESS;

    GATTServApp_InitCharCfg(INVALID_CONNHANDLE, ble_uart_TxCCCD);
    // Register with Link DB to receive link status change callback
    linkDB_Register(ble_uart_HandleConnStatusCB);

    //    ble_uart_TxCCCD.connHandle = INVALID_CONNHANDLE;
    //    ble_uart_TxCCCD.value = 0;
    // Register GATT attribute list and CBs with GATT Server App
    status = GATTServApp_RegisterService(ble_uart_ProfileAttrTbl,
                                         GATT_NUM_ATTRS(ble_uart_ProfileAttrTbl),
                                         GATT_MAX_ENCRYPT_KEY_SIZE,
                                         &ble_uart_ProfileCBs);
    if(status != SUCCESS)
        PRINT("Add ble uart service failed!\n");
    ble_uart_AppCBs = cb;

    return (status);
}

/*********************************************************************
 * @fn          ble_uart_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 *
 * @return      Success or Failure
 */
static bStatus_t ble_uart_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                     uint8_t *pValue, uint16_t *pLen, uint16_t offset, uint16_t maxLen, uint8_t method)
{
    bStatus_t status = SUCCESS;
    PRINT("ReadAttrCB\n");
    // Make sure it's not a blob operation (no attributes in the profile are long)
    if(pAttr->type.len == ATT_BT_UUID_SIZE)
    {
        // 16-bit UUID
        uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
        if(uuid == GATT_CLIENT_CHAR_CFG_UUID)
        {
            *pLen = 2;
            tmos_memcpy(pValue, pAttr->pValue, 2);
        }
    }
    //    else
    //    {
    //        if(!tmos_memcmp(pAttr->type.uuid, ble_uart_TxCharUUID, 16))
    //        {
    //            *pLen = 1;
    //            pValue[0] = '1';
    //        }
    //        else if(!tmos_memcmp(pAttr->type.uuid, ble_uart_RxCharUUID, 16))
    //        {
    //            PRINT("read tx char\n");
    //        }
    //    }

    return (status);
}

/*********************************************************************
 * @fn      simpleProfile_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 *
 * @return  Success or Failure
 */

static bStatus_t ble_uart_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                      uint8_t *pValue, uint16_t len, uint16_t offset, uint8_t method)
{
    bStatus_t status = SUCCESS;
    //uint8_t notifyApp = 0xFF;
    // If attribute permissions require authorization to write, return error
    if(gattPermitAuthorWrite(pAttr->permissions))
    {
        // Insufficient authorization
        return (ATT_ERR_INSUFFICIENT_AUTHOR);
    }

    if(pAttr->type.len == ATT_BT_UUID_SIZE)
    {
        // 16-bit UUID
        uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
        if(uuid == GATT_CLIENT_CHAR_CFG_UUID)
        {
            status = GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len,
                                                    offset, GATT_CLIENT_CFG_NOTIFY);
            if(status == SUCCESS && ble_uart_AppCBs)
            {
                uint16_t         charCfg = BUILD_UINT16(pValue[0], pValue[1]);
                ble_uart_evt_t evt;

                //PRINT("CCCD set: [%d]\n", charCfg);
                evt.type = (charCfg == GATT_CFG_NO_OPERATION) ? BLE_UART_EVT_TX_NOTI_DISABLED : BLE_UART_EVT_TX_NOTI_ENABLED;
                ble_uart_AppCBs(connHandle, &evt);
            }
        }

        //  UUID
        if(pAttr->handle == ble_uart_ProfileAttrTbl[RAWPASS_RX_VALUE_HANDLE].handle)
        {
            if(ble_uart_AppCBs)
            {
                ble_uart_evt_t evt;
                evt.type = BLE_UART_EVT_BLE_DATA_RECIEVED;
                evt.data.length = (uint16_t)len;
                evt.data.p_data = pValue;
                ble_uart_AppCBs(connHandle, &evt);
            }
        }
    }
    //    else
    //    {
    //        // 128-bit UUID
    //        if(pAttr->handle == ble_uart_ProfileAttrTbl[RAWPASS_RX_VALUE_HANDLE].handle)
    //        {
    //            if(ble_uart_AppCBs) {
    //                ble_uart_evt_t evt;
    //                evt.type = BLE_UART_EVT_BLE_DATA_RECIEVED;
    //                evt.data.length = (uint16_t)len;
    //                evt.data.p_data = pValue;
    //                ble_uart_AppCBs(connHandle,&evt);
    //            }
    //        }
    //    }
    return (status);
}

/*********************************************************************
 * @fn          simpleProfile_HandleConnStatusCB
 *
 * @brief       Simple Profile link status change handler function.
 *
 * @param       connHandle - connection handle
 * @param       changeType - type of change
 *
 * @return      none
 */
static void ble_uart_HandleConnStatusCB(uint16_t connHandle, uint8_t changeType)
{
    // Make sure this is not loopback connection
    if(connHandle != LOOPBACK_CONNHANDLE)
    {
        // Reset Client Char Config if connection has dropped
        if((changeType == LINKDB_STATUS_UPDATE_REMOVED) ||
           ((changeType == LINKDB_STATUS_UPDATE_STATEFLAGS) &&
            (!linkDB_Up(connHandle))))
        {
            //ble_uart_TxCCCD[0].value = 0;
            GATTServApp_InitCharCfg(connHandle, ble_uart_TxCCCD);
            //PRINT("clear client configuration\n");
        }
    }
}

uint8_t ble_uart_notify_is_ready(uint16_t connHandle)
{
    return (GATT_CLIENT_CFG_NOTIFY == GATTServApp_ReadCharCfg(connHandle, ble_uart_TxCCCD));
}
/*********************************************************************
 * @fn          BloodPressure_IMeasNotify
 *
 * @brief       Send a notification containing a bloodPressure
 *              measurement.
 *
 * @param       connHandle - connection handle
 * @param       pNoti - pointer to notification structure
 *
 * @return      Success or Failure
 */
bStatus_t ble_uart_notify(uint16_t connHandle, attHandleValueNoti_t *pNoti, uint8_t taskId)
{
    //uint16_t value = ble_uart_TxCCCD[0].value;
    uint16_t value = GATTServApp_ReadCharCfg(connHandle, ble_uart_TxCCCD);
    // If notifications enabled
    if(value & GATT_CLIENT_CFG_NOTIFY)
    {
        // Set the handle
        pNoti->handle = ble_uart_ProfileAttrTbl[RAWPASS_TX_VALUE_HANDLE].handle;

        // Send the Indication
        return GATT_Notification(connHandle, pNoti, FALSE);
    }
    return bleIncorrectMode;
}

/*********************************************************************
*********************************************************************/
