# Production-ready setup creation documentation

## Remarks

- Driver and setup version numbers are decoupled as setup might require updates independently of driver logic changes.

## Signing cheat sheet for public release

- Get coffee
- Tag driver version as `vX.X.X.X`
- Update version in WiX project and tag that version as `setup-vX.X.X`
- Push with tags
- Let CI build the fun
- Adjust and run `.\stage0.ps1 -Token "APPVEYOR-API-TOKEN-HERE" -BuildVersion "1.3.79.0" -SetupVersion "1.3.79.0"`
- Upload `*.cab` files to Microsoft Portal and wait for signed results (stick to consistent names for submissions)
  - Example (x86): `BthPS3 x86 v1.3.65.0 09.01.2021`
  - Example (x64): `BthPS3 x64 v1.3.65.0 09.01.2021`
- Get another coffee
- Download and extract the signed files into the directory `.\Setup\drivers`
- Run `.\stage1.ps1`
- Build the MSI for all platforms
- Run `.\stage2.ps1`
- Craft new release for the previously created `setup-vX.X.X` tag
  - Fill in release notes
  - Attach MSI files
- Publish
