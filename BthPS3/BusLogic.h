#pragma once


typedef struct _PDO_IDENTIFICATION_DESCRIPTION
{
    //
    // Mandatory header
    // 
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header;

    //
    // MAC address identifying this device
    // 
    BTH_ADDR RemoteAddress;

} PDO_IDENTIFICATION_DESCRIPTION, *PPDO_IDENTIFICATION_DESCRIPTION;

EVT_WDF_CHILD_LIST_CREATE_DEVICE BthPS3_EvtWdfChildListCreateDevice;
