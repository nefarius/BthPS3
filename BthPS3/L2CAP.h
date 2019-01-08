#pragma once


_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendConnectResponse(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ConnectResponseCompleted;
