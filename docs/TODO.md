# Open Tasks

## Unimplemented sections in the state machine

Some sections in the (dis-)connection state machine are not implemented, those cases are hard to trigger and test and could lead to leaking memory. They are marked with `TODO:` tags.

## SCP-compatibility not a 100%

For not yet discovered reasons some controllers working under [SCP](https://github.com/nefarius/ScpToolkit) behave differently with this solution. For example, they connect and stay connected, but don't send input reports or the reports they send wireless is truncated or otherwise malformed. It's *assumed* that L2CAP MTU has an influence here but so far couldn't successfully be validated. BthPS3 announces an MTU of `0xFFFF` (similar to code in the [Arduino USB Host Shield 2.0](https://github.com/felis/USB_Host_Shield_2.0/blob/06d5ed134a37e22575c0ce18c061d9ef115151e0/BTD.cpp#L1288-L1289) implementation), while [SCP uses](https://github.com/nefarius/ScpToolkit/blob/c082de827fb6ec3efdff0a7a632977fbdff898e1/ScpControl/Bluetooth/BthDongle.L2cap.cs#L124-L125) `0x0096`. Comparing USB/L2CAP packet captures between SCP and BthPS3 operation could lead to the missing insights.
