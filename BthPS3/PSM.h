/*
* BthPS3 - Windows kernel-mode Bluetooth profile and bus driver
*
* MIT License
*
* Copyright (C) 2018-2019  Nefarius Software Solutions e.U. and Contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


#pragma once

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_DisablePatchSync(
	WDFIOTARGET IoTarget,
	ULONG DeviceIndex
);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3PSM_EnablePatchSync(
	WDFIOTARGET IoTarget,
	ULONG DeviceIndex
);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
BthPS3PSM_EnablePatchAsync(
    WDFIOTARGET IoTarget,
    ULONG DeviceIndex
);

EVT_WDF_REQUEST_COMPLETION_ROUTINE BthPS3PSM_FilterRequestCompletionRoutine;
