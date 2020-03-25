# Production-ready setup creation documentation

## Remarks

- Driver and setup version numbers are decoupled as setup might require updates independently of driver logic changes. 

## Build & submission preparations

1. Update/ensure correct `Driver Version Number` in `StampInf` of the projects.
2. Update/ensure correct version details in `*.rc` files.
3. Build drivers and tools in `Release` configuration for `x64` and `x86` architectures.
4. Production-sign drivers and tools to ensure authenticity of origin. 
5. Run `sign.cmd` which will create the two `*.cab` files in the `disk1` folder and sign them.
6. Upload `*.cab` files to Partner Portal.
7. Request signatures, wait and pray.
8. Download signed files and extract `drivers` directory directly into this directory.
9. **Optional:** Re-create `*.cat` files to support Windows 7 in addition to 10 (causes confirmation prompt to appear!):
    - x64: `Inf2Cat.exe /driver:"." /uselocaltime /os:7_X64,10_X64,Server10_X64,10_AU_X64,Server2016_X64,10_RS2_X64,ServerRS2_X64,10_RS3_X64,ServerRS3_X64,10_RS4_X64,ServerRS4_X64`
    - x86: `Inf2Cat.exe /driver:"." /uselocaltime /os:7_X86,10_X86,10_AU_X86,10_RS2_X86,10_RS3_X86,10_RS4_X86`
10. Build and sign the Advanced Installer setup project.
11. Adjust updater control file and upload setup to distribution server.
