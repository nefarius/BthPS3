# Production-ready setup creation documentation

## Remarks

- Driver and setup version numbers are decoupled as setup might require updates independently of driver logic changes.

## Signing cheat sheet for public release

- Get coffee
- Tag driver version as `vX.X.X.X`
- Tag setup version as `setup-vX.X.X`
- Push with tags enabled
- Let CI build the fun
- Adjust and run `.\stage0.ps1 -Token "$appVeyorToken" -BuildVersion "1.3.65.0"`
  - This downloads and signs the binaries and submissions files
- Upload `*.cab` files to Microsoft Portal and wait for signed results (stick to consistent names for submissions)
  - Example (x86): `BthPS3 x86 v1.3.65.0 09.01.2021`
  - Example (x64): `BthPS3 x64 v1.3.65.0 09.01.2021`
- Get another coffee
- Download and extract the signed files into the directory `.\Setup\drivers`
- Run `.\stage1.ps1`
  - This ensures all binaries are properly signed before getting packed by setup
- Run `.\stage2.ps1 -SetupVersion "1.3.65"`
  - This will build and sign the MSI files
- Craft new release for the previously created `setup-vX.X.X` tag
  - Fill in release notes
  - Attach MSI files
- Publish
