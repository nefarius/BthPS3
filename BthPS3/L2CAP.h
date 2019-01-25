#pragma once


_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_HandleRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_DenyRemoteConnect(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
);

_IRQL_requires_max_(DISPATCH_LEVEL)
void
L2CAP_PS3_ConnectionIndicationCallback(
    _In_ PVOID Context,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ConnectResponseCompleted;
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_DenyRemoteConnectCompleted;

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ControlConnectResponseCompleted;
EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_InterruptConnectResponseCompleted;

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_ConnectionStateConnected(
    WDFOBJECT ConnectionObject
);
