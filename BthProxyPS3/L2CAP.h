/*
MIT License

Copyright (c) 2017 Benjamin "Nefarius" Höglinger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#pragma once

/**
* \typedef enum _L2CAP_SIGNALLING_COMMAND_CODE
*
* \brief   Defines an alias representing an L2CAP signaling command code.
*/
typedef enum _L2CAP_SIGNALLING_COMMAND_CODE
{
    L2CAP_Reserved = 0x00,
    L2CAP_Command_Reject = 0x01,

    /// <summary>
    ///     A Connection Request packet has been received.
    /// </summary>
    L2CAP_Connection_Request = 0x02,

    /// <summary>
    ///     A Connection Response packet has been received with a positive result indicating that the connection has been
    ///     established.
    /// </summary>
    L2CAP_Connection_Response = 0x03,

    /// <summary>
    ///     A Configuration Request packet has been received indicating the remote endpoint wishes to engage in negotiations
    ///     concerning channel parameters.
    /// </summary>
    L2CAP_Configuration_Request = 0x04,

    /// <summary>
    ///     A Configuration Response packet has been received indicating the remote endpoint agrees with all the parameters
    ///     being negotiated.
    /// </summary>
    L2CAP_Configuration_Response = 0x05,

    /// <summary>
    ///     A Disconnection Request packet has been received and the channel must initiate the disconnection process. Following
    ///     the completion of an L2CAP channel disconnection process, an L2CAP entity should return the corresponding local CID
    ///     to the pool of “unassigned” CIDs.
    /// </summary>
    L2CAP_Disconnection_Request = 0x06,

    /// <summary>
    ///     A Disconnection Response packet has been received. Following the receipt of this signal, the receiving L2CAP entity
    ///     may return the corresponding local CID to the pool of unassigned CIDs. There is no corresponding negative response
    ///     because the Disconnect Request must succeed.
    /// </summary>
    L2CAP_Disconnection_Response = 0x07,
    L2CAP_Echo_Request = 0x08,
    L2CAP_Echo_Response = 0x09,
    L2CAP_Information_Request = 0x0A,
    L2CAP_Information_Response = 0x0B

} L2CAP_SIGNALLING_COMMAND_CODE;

/**
* \typedef enum _L2CAP_CONFIGURATION_RESPONSE_RESULT
*
* \brief   Defines an alias representing an L2CAP configuration response result.
*/
typedef enum _L2CAP_CONFIGURATION_RESPONSE_RESULT
{
    /// <summary>
    ///     Success
    /// </summary>
    L2CAP_ConfigurationResponseResult_Success = 0x0000,

    /// <summary>
    ///     Failure – unacceptable parameters
    /// </summary>
    L2CAP_ConfigurationResponseResult_FailureUnacceptableParameters = 0x0001,

    /// <summary>
    ///     Failure – rejected (no reason provided)
    /// </summary>
    L2CAP_ConfigurationResponseResult_FailureRejected = 0x0002,

    /// <summary>
    ///     Failure – unknown options
    /// </summary>
    L2CAP_ConfigurationResponseResult_FailureUnknownOptions = 0x0003

} L2CAP_CONFIGURATION_RESPONSE_RESULT;

/**
* \typedef enum _L2CAP_CONNECTION_RESPONSE_RESULT
*
* \brief   Defines an alias representing an L2CAP connection response result.
*/
typedef enum _L2CAP_CONNECTION_RESPONSE_RESULT
{
    /// <summary>
    ///     Connection successful.
    /// </summary>
    L2CAP_ConnectionResponseResult_ConnectionSuccessful = 0x0000,

    /// <summary>
    ///     Connection pending.
    /// </summary>
    L2CAP_ConnectionResponseResult_ConnectionPending = 0x0001,

    /// <summary>
    ///     Connection refused – PSM not supported.
    /// </summary>
    L2CAP_ConnectionResponseResult_ConnectionRefusedPsmNotNupported = 0x0002,

    /// <summary>
    ///     Connection refused – security block.
    /// </summary>
    L2CAP_ConnectionResponseResult_ConnectionRefusedSecurityBlock = 0x0003,

    /// <summary>
    ///     Connection refused – no resources available.
    /// </summary>
    L2CAP_ConnectionResponseResult_ConnectionRefusedNoResourcesAvailable = 0x0004

} L2CAP_CONNECTION_RESPONSE_RESULT;

/**
* \typedef enum _L2CAP_CONNECTION_RESPONSE_STATUS
*
* \brief   Defines an alias representing an L2CAP connection response status.
*/
typedef enum _L2CAP_CONNECTION_RESPONSE_STATUS
{
    /// <summary>
    ///     No further information available.
    /// </summary>
    L2CAP_ConnectionResponseStatus_NoFurtherInformationAvailable = 0x0000,
    /// <summary>
    ///     Authentication pending.
    /// </summary>
    L2CAP_ConnectionResponseStatus_AuthenticationPending = 0x0001,
    /// <summary>
    ///     Authorisation pending.
    /// </summary>
    L2CAP_ConnectionResponseStatus_AuthorisationPending = 0x0002

} L2CAP_CONNECTION_RESPONSE_STATUS;

/**
* \typedef enum _L2CAP_PSM
*
* \brief   Defines an alias representing an L2CAP Protocol/Service Multiplexer.
*/
typedef enum _L2CAP_PSM
{
    L2CAP_PSM_HID_Command = 0x11,
    L2CAP_PSM_HID_Interrupt = 0x13

} L2CAP_PSM;

/**
* \typedef struct _L2CAP_CID
*
* \brief   Defines an alias representing an L2CAP Channel Identifier.
*/
typedef struct _L2CAP_CID
{
    BYTE Lsb;
    BYTE Msb;

} L2CAP_CID, *PL2CAP_CID;

/**
* \typedef struct _L2CAP_SIGNALLING_COMMAND_REJECT
*
* \brief   Defines an alias representing data attached to COMMAND REJECT.
*/
typedef struct _L2CAP_SIGNALLING_COMMAND_REJECT
{
    BYTE Code;
    BYTE Identifier;
    USHORT Length;
    USHORT Reason;

} L2CAP_SIGNALLING_COMMAND_REJECT, *PL2CAP_SIGNALLING_COMMAND_REJECT;

/**
* \typedef struct _L2CAP_SIGNALLING_CONNECTION_REQUEST
*
* \brief   Defines an alias representing data attached to CONNECTION REQUEST.
*/
typedef struct _L2CAP_SIGNALLING_CONNECTION_REQUEST
{
    BYTE Code;
    BYTE Identifier;
    USHORT Length;
    USHORT PSM;
    L2CAP_CID SCID;

} L2CAP_SIGNALLING_CONNECTION_REQUEST, *PL2CAP_SIGNALLING_CONNECTION_REQUEST;

/**
* \typedef struct _L2CAP_SIGNALLING_CONNECTION_RESPONSE
*
* \brief   Defines an alias representing data attached to CONNECTION RESPONSE.
*/
typedef struct _L2CAP_SIGNALLING_CONNECTION_RESPONSE
{
    BYTE Code;
    BYTE Identifier;
    USHORT Length;
    L2CAP_CID DCID;
    L2CAP_CID SCID;
    USHORT Result;
    USHORT Status;

} L2CAP_SIGNALLING_CONNECTION_RESPONSE, *PL2CAP_SIGNALLING_CONNECTION_RESPONSE;

/**
* \typedef struct _L2CAP_SIGNALLING_CONFIGURATION_REQUEST
*
* \brief   Defines an alias representing data attached to CONFIGURATION REQUEST.
*/
typedef struct _L2CAP_SIGNALLING_CONFIGURATION_REQUEST
{
    BYTE Code;
    BYTE Identifier;
    USHORT Length;
    L2CAP_CID DCID;
    USHORT Flags;
    ULONG Options;

} L2CAP_SIGNALLING_CONFIGURATION_REQUEST, *PL2CAP_SIGNALLING_CONFIGURATION_REQUEST;

/**
* \typedef struct _L2CAP_SIGNALLING_CONFIGURATION_RESPONSE
*
* \brief   Defines an alias representing data attached to CONFIGURATION RESPONSE.
*/
typedef struct _L2CAP_SIGNALLING_CONFIGURATION_RESPONSE
{
    BYTE Code;
    BYTE Identifier;
    USHORT Length;
    L2CAP_CID SCID;
    USHORT Flags;
    USHORT Result;
    USHORT Options;

} L2CAP_SIGNALLING_CONFIGURATION_RESPONSE, *PL2CAP_SIGNALLING_CONFIGURATION_RESPONSE;

/**
* \typedef struct _L2CAP_SIGNALLING_DISCONNECTION_REQUEST
*
* \brief   Defines an alias representing data attached to DISCONNECTION REQUEST.
*/
typedef struct _L2CAP_SIGNALLING_DISCONNECTION_REQUEST
{
    BYTE Code;
    BYTE Identifier;
    USHORT Length;
    L2CAP_CID DCID;
    L2CAP_CID SCID;

} L2CAP_SIGNALLING_DISCONNECTION_REQUEST, *PL2CAP_SIGNALLING_DISCONNECTION_REQUEST;

/**
* \typedef struct _L2CAP_SIGNALLING_DISCONNECTION_RESPONSE
*
* \brief   Defines an alias representing data attached to DISCONNECTION RESPONSE.
*/
typedef struct _L2CAP_SIGNALLING_DISCONNECTION_RESPONSE
{
    BYTE Code;
    BYTE Identifier;
    USHORT Length;
    L2CAP_CID DCID;
    L2CAP_CID SCID;

} L2CAP_SIGNALLING_DISCONNECTION_RESPONSE, *PL2CAP_SIGNALLING_DISCONNECTION_RESPONSE;

/**
* \def L2CAP_IS_CONTROL_CHANNEL(_buf_) ((BOOLEAN)_buf_[6] == 0x01 && _buf_[7] == 0x00)
*
* \brief   A macro that identifies the control channel.
*
* \author  Benjamin "Nefarius" Höglinger
* \date    18.09.2017
*
* \param   _buf_   The buffer.
*/
#define L2CAP_IS_CONTROL_CHANNEL(_buf_)                     ((BOOLEAN)_buf_[6] == 0x01 && _buf_[7] == 0x00)

/**
* \def L2CAP_IS_HID_INPUT_REPORT(_buf_) ((BOOLEAN)_buf_[8] == 0xA1 && _buf_[9] == 0x01)
*
* \brief   A macro that identifies a HID input report.
*
* \author  Benjamin "Nefarius" Höglinger
* \date    18.09.2017
*
* \param   _buf_   The buffer.
*/
#define L2CAP_IS_HID_INPUT_REPORT(_buf_)                    ((BOOLEAN)_buf_[8] == 0xA1 && _buf_[9] == 0x01)

/**
* \def L2CAP_GET_SIGNALLING_COMMAND_CODE(_buf_) ((L2CAP_SIGNALLING_COMMAND_CODE)_buf_[8])
*
* \brief   A macro that validates the signaling command code.
*
* \author  Benjamin "Nefarius" Höglinger
* \date    18.09.2017
*
* \param   _buf_   The buffer.
*/
#define L2CAP_GET_SIGNALLING_COMMAND_CODE(_buf_)            ((L2CAP_SIGNALLING_COMMAND_CODE)_buf_[8])

/**
* \fn  BOOLEAN FORCEINLINE L2CAP_IS_SIGNALLING_COMMAND_CODE( PUCHAR Buffer )
*
* \brief   Checks if the supplied buffer represents a valid L2CAP signaling command code.
*
* \author  Benjamin "Nefarius" Höglinger
* \date    18.09.2017
*
* \param   Buffer  The buffer.
*
* \return  TRUE if valid, FALSE otherwise.
*/
BOOLEAN FORCEINLINE L2CAP_IS_SIGNALLING_COMMAND_CODE(
    PUCHAR Buffer
)
{
    for (UCHAR i = L2CAP_Command_Reject; i <= L2CAP_Information_Response; i++)
    {
        if (i == Buffer[8]) return TRUE;
    }

    return FALSE;
}
