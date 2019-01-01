#include "Driver.h"

typedef enum _BTHPS3_CONNECTION_STATE {
    ConnectionStateUnitialized = 0,
    ConnectionStateInitialized,
    ConnectionStateConnecting,
    ConnectionStateConnected,
    ConnectionStateConnectFailed,
    ConnectionStateDisconnecting,
    ConnectionStateDisconnected

} BTHPS3_CONNECTION_STATE, *PBTHPS3_CONNECTION_STATE;