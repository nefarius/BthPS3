# Production setup

Driver and setup version numbers stay coupled for tagged GitHub Actions releases: a `vMAJOR.MINOR.PATCH` tag produces driver file version `MAJOR.MINOR.PATCH.(2000 + run_number)` and setup version `MAJOR.MINOR.PATCH`. Use a later `setup-vMAJOR.MINOR.PATCH` tag only for the GitHub Release that attaches the MSI.

## Repository configuration

Tagged runs need these GitHub repository settings:

- `SIGN_RELAY_SERVER` (variable)
- `SIGN_RELAY_CI_TOKEN` (secret)
- `WEBHOOK_URL` (secret; buildbot artifact mirror)
- `SDCM_PROFILES__DEFAULT__TENANTID` (secret)
- `SDCM_PROFILES__DEFAULT__CLIENTID` (secret)
- `SDCM_PROFILES__DEFAULT__KEY` (secret)

## Signing cheat sheet

1. Push tag `vMAJOR.MINOR.PATCH` (example: `v2.12.0`).
2. Wait for the Build workflow to compile x64 and ARM64, EV-sign both `BthPS3.sys` and both `BthPS3PSM.sys` binaries, pack **one** combined Partner Center CAB, and submit that CAB.
3. When Partner Center finishes, the same run (or a dispatched `Partner Center signing` retry) publishes `bthps3-microsoft-drivers`.
4. From the repository root, stage the run:

   ```powershell
   .\Setup\stage0.ps1 -RunId 123456789
   .\Setup\stage1.ps1
   .\Setup\stage2.ps1 -SetupVersion "2.12.0"
   ```

5. Create the GitHub Release on `setup-v2.12.0` and attach the signed MSI.

Do not re-sign Microsoft-attested `.sys` files. Attestation adds the Microsoft signature; appending another publisher signature is incorrect.

The combined CAB contains `BthPS3` (profile + NULL PDO INF) and `BthPS3PSM` (class filter) for both x64 and ARM64. Never submit the retired per-architecture CABs to Partner Center.
