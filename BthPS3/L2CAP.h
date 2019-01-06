#pragma once

VOID
FORCEINLINE
L2CAP_REUSE_OPEN_CHANNEL_RESPONSE(
    PBTH_PROFILE_DRIVER_INTERFACE pInterface,
    BRB_TYPE Type,
    struct _BRB_L2CA_OPEN_CHANNEL *pBrb
)
{
    BTH_ADDR addr;
    USHORT psm;
    L2CAP_CHANNEL_HANDLE handle;

    if (!pInterface || !pBrb) {
        return;
    }

    addr = pBrb->BtAddress;
    psm = pBrb->Psm;
    handle = pBrb->ChannelHandle;

    pInterface->BthReuseBrb((PBRB)pBrb, Type);

    pBrb->BtAddress = addr;
    pBrb->Psm = psm;
    pBrb->ChannelHandle = handle;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
L2CAP_PS3_SendConnectResponse(
    _In_ PBTHPS3_SERVER_CONTEXT DevCtx,
    _In_ PINDICATION_PARAMETERS ConnectParams
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE L2CAP_PS3_ConnectResponsePendingCompleted;
