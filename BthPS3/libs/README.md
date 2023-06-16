# Custom `WppRecorder.sys` linking

The provided `WppRecorder.lib` in the platform directories was made with:

```cmd
rem x64
lib /def:WppRecorder.def /machine:x64 /out:wpprecorder.lib

rem arm64
lib /def:WppRecorder.def /machine:ARM64 /out:wpprecorder.lib
```

This is [a workaround](https://fosstodon.org/@Nefarius/110538974979824362) for DMF pulling in mandatory linking against `imp_WppRecorderReplay` used for the Inflight Trace Recorder. This import is omitted from the `WppRecorder.def` file so during runtime, our custom implementation will be called instead, which simply does nothing on platforms that lack support.
