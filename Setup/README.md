# Production-ready setup creation documentation

## Remarks

- Driver and setup version numbers are decoupled as setup might require updates independently of driver logic changes.

## release.ps1

```PowerShell
.\release.ps1 -Token "APPVEYOR-API-TOKEN-HERE" -Version "1.3.72.0"
```

## Signing cheat sheet for public release

- Get coffee
- Tag driver version as `vX.X.X.X`
- Update version in WiX project and tag that version as `setup-vX.X.X`
- Push with tags
- Let CI build the fun
- Download `*.cab` artifacts for all platforms
- Sign `*.cab` files for portal submission
  - `signtool sign /v /as /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\DigiCert_High_Assurance_EV_Root_CA.crt" /n "Nefarius Software Solutions e.U." /tr http://timestamp.digicert.com /fd sha256 /td sha256 .\BthPS3_x64.cab .\BthPS3_x86.cab`
- Upload to Microsoft Portal and wait for signed results (stick to consistent names for submissions)
  - Example (x86): `BthPS3 x86 v1.3.65.0 09.01.2021`
  - Example (x64): `BthPS3 x64 v1.3.65.0 09.01.2021`
- Get another coffee
- Download and extract the signed files into the directory `.\Setup\drivers`
- Download the `*.pdb` files from CI into the same directories
- Download the `*.exe` artifacts to `.\artifacts` directory
- Sign them
  - `signtool sign /v /as /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\DigiCert_High_Assurance_EV_Root_CA.crt" /n "Nefarius Software Solutions e.U." /tr http://timestamp.digicert.com /fd sha256 /td sha256 .\artifacts\*.exe .\artifacts\x64\*.exe .\artifacts\x86\*.exe`
- Append signature to signed files (`*.sys`)
  - `signtool sign /v /as /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\DigiCert_High_Assurance_EV_Root_CA.crt" /n "Nefarius Software Solutions e.U." /tr http://timestamp.digicert.com /fd sha256 /td sha256 .\Setup\drivers\BthPS3_x64\*.sys .\Setup\drivers\BthPS3_x86\*.sys .\Setup\drivers\BthPS3PSM_x64\*.sys .\Setup\drivers\BthPS3PSM_x86\*.sys`
- Make sure `BthPS3CA` is built for all platforms and signed
  - `signtool sign /v /as /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\DigiCert_High_Assurance_EV_Root_CA.crt" /n "Nefarius Software Solutions e.U." /tr http://timestamp.digicert.com /fd sha256 /td sha256 .\bin\x64\BthPS3CustomActions.dll .\bin\x86\BthPS3CustomActions.dll`
- Build and sign the MSI for all platforms
  - `signtool sign /v /ac "C:\Program Files (x86)\Windows Kits\10\CrossCertificates\DigiCert_High_Assurance_EV_Root_CA.crt" /n "Nefarius Software Solutions e.U." /tr http://timestamp.digicert.com /fd sha256 /td sha256 .\bin\x64\BthPS3Setup.msi .\bin\x86\BthPS3Setup.msi`
- Craft new release for the previously created `setup-vX.X.X` tag
  - Fill in release notes
  - Attach MSI files
- Publish
