# Production-ready setup creation documentation

## Build & submission preparations

1. Update/ensure correct `Driver Version Number` in `StampInf` of the projects.
2. Update/ensure correct version details in `*.rc` files.
3. Build drivers and tools in `Release` configuration for `x64` and `x86` architectures.
4. Production-sign drivers and tools to ensure authenticity of origin. 
5. Run `sign.cmd` which will create the two `*.cab` files in the `disk1` folder and sign them.
6. Upload `*.cab` files to Partner Portal.