using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

using CliWrap;
using CliWrap.Buffered;
using CliWrap.Builders;

using Nefarius.BthPS3.Shared;
using Nefarius.Utilities.Bluetooth;
using Nefarius.Utilities.DeviceManagement.Drivers;
using Nefarius.Utilities.DeviceManagement.Exceptions;
using Nefarius.Utilities.DeviceManagement.PnP;

using WixSharp;

using WixToolset.Dtf.WindowsInstaller;

namespace Nefarius.BthPS3.Setup;

public static class CustomActions
{
    static CustomActions()
    {
        CustomActionAssemblyResolver.Register();
    }

    /// <summary>
    ///     Reads the <see cref="CustomProperties.UseModern" /> session property, defaulting to
    ///     <c>true</c> (the modern install path) when the property is missing or not a valid
    ///     boolean, instead of letting <see cref="bool.Parse(string)" /> throw and fail the
    ///     whole install with an opaque 1603.
    /// </summary>
    private static bool GetUseModern(Session session)
    {
        return !bool.TryParse(session.Property(CustomProperties.UseModern), out bool useModern) || useModern;
    }

    #region Web

    [CustomAction]
    public static ActionResult OpenArticle(Session session)
    {
        // MSI UILevel is reported as silent for Embedded/ManagedUI even during a full
        // wizard run. WIXSHARP_MANAGED_UI_HANDLE is only set when the ManagedUI window
        // is actually shown; it stays empty for reduced/basic/suppressed execution.
        string managedUiHandle = session.Property("WIXSHARP_MANAGED_UI_HANDLE");
        bool managedUiDisplayed = !string.IsNullOrWhiteSpace(managedUiHandle);
        bool articleFeatureEnabled = session.IsFeatureEnabled("PostInstArticle");

        session.Log(
            $"{nameof(OpenArticle)} - WIXSHARP_MANAGED_UI_HANDLE='{managedUiHandle}', " +
            $"managedUiDisplayed={managedUiDisplayed}, PostInstArticle={articleFeatureEnabled}");

        // Full ManagedUI: honor the optional feature. Reduced/basic/suppressed UI
        // never shows the Features dialog, so always open the article.
        if (managedUiDisplayed && !articleFeatureEnabled)
        {
            session.Log($"{nameof(OpenArticle)} - skipping launch; feature deselected in full UI");
            return ActionResult.Success;
        }

        try
        {
            session.Log($"{nameof(OpenArticle)} - launching post-installation article");
            Process.Start("https://docs.nefarius.at/projects/BthPS3/Welcome/Installation-Successful/");
        }
        catch (Exception ex)
        {
            session.Log($"Spawning article process failed with {ex}");
        }

        return ActionResult.Success;
    }

    #endregion

    #region Driver management

    /// <summary>
    ///     Put legacy/fallback install logic here.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    [CustomAction]
    public static ActionResult InstallDriversLegacy(Session session)
    {
        session.Log($"{nameof(InstallDriversLegacy)} - USE_MODERN = {session.Property(CustomProperties.UseModern)}");

        if (GetUseModern(session))
        {
            session.Log("USE_MODERN set to true, skipping action");
            return ActionResult.NotExecuted;
        }

        // clean out whatever has been on the machine before
        UninstallDriversLegacy(session);

        session.Log($"--- BEGIN {nameof(InstallDriversLegacy)} ---");

        try
        {
            #region Paths

            DirectoryInfo installDir = new(session.Property("INSTALLDIR"));
            session.Log($"installDir = {installDir}");
            string driversDir = Path.Combine(installDir.FullName, "drivers");
            session.Log($"driversDir = {driversDir}");
            string nefconDir = Path.Combine(installDir.FullName, "nefcon");
            session.Log($"nefconDir = {nefconDir}");
            string archShortName = ArchitectureInfo.PlatformShortName;
            session.Log($"archShortName = {archShortName}");

            string nefconcPath = Path.Combine(nefconDir, archShortName, "nefconc.exe");
            session.Log($"nefconcPath = {nefconcPath}");

            string bthPs3DriverDir = Path.Combine(driversDir, "BthPS3");
            session.Log($"bthPs3DriverDir = {bthPs3DriverDir}");
            string bthPs3PsmDriverDir = Path.Combine(driversDir, "BthPS3PSM");
            session.Log($"bthPs3PsmDriverDir = {bthPs3PsmDriverDir}");

            string bthPs3InfPath = Path.Combine(bthPs3DriverDir, "BthPS3.inf");
            session.Log($"bthPs3InfPath = {bthPs3InfPath}");
            string bthPs3PsmInfPath = Path.Combine(bthPs3PsmDriverDir, "BthPS3PSM.inf");
            session.Log($"bthPs3PsmInfPath = {bthPs3PsmInfPath}");
            string bthPs3NullInfPath = Path.Combine(bthPs3DriverDir, "BthPS3_PDO_NULL_Device.inf");
            session.Log($"bthPs3NullInfPath = {bthPs3NullInfPath}");

            #endregion

            session.Log("Installing BthPS3PSM filter driver.");

            BufferedCommandResult? filterInstRet = RunCommand(nefconcPath, builder => builder
                .Add("--inf-default-install")
                .Add("--inf-path").Add(bthPs3PsmInfPath)
            );

            if (filterInstRet?.ExitCode != 0 && filterInstRet?.ExitCode != 3010)
            {
                session.Log(
                    $"Filter installer failed with exit code: {filterInstRet?.ExitCode}, message: {Win32Exception.GetMessageFor(filterInstRet?.ExitCode)}");
                goto exitFailure;
            }

            session.Log("BthPS3 filter driver installed.");

            session.Log("Adding BthPS3PSM as Bluetooth class filters.");

            BufferedCommandResult? cassFilterRet = RunCommand(nefconcPath, builder => builder
                .Add("--add-class-filter")
                .Add("--position").Add("lower")
                .Add("--service-name").Add(FilterDriver.FilterServiceName)
                .Add("--class-guid").Add(DeviceClassIds.Bluetooth)
            );

            if (cassFilterRet?.ExitCode != 0 && cassFilterRet?.ExitCode != 3010)
            {
                session.Log(
                    $"Class filter modification failed with exit code: {cassFilterRet?.ExitCode}, message: {Win32Exception.GetMessageFor(cassFilterRet?.ExitCode)}");
                goto exitFailure;
            }

            session.Log("BthPS3PSM added to Bluetooth class filters.");

            session.Log("Installing BthPS3 driver in driver store.");

            BufferedCommandResult? profileInstRet = RunCommand(nefconcPath, builder => builder
                .Add("--install-driver")
                .Add("--inf-path").Add(bthPs3InfPath)
            );

            if (profileInstRet?.ExitCode != 0 && profileInstRet?.ExitCode != 3010)
            {
                session.Log(
                    $"Profile driver installation failed with exit code: {profileInstRet?.ExitCode}, message: {Win32Exception.GetMessageFor(profileInstRet?.ExitCode)}");
                goto exitFailure;
            }

            session.Log("BthPS3 driver installed in driver store.");

            session.Log("Installing PDO NULL driver.");

            BufferedCommandResult? nullDrvInstRet = RunCommand(nefconcPath, builder => builder
                .Add("--install-driver")
                .Add("--inf-path").Add(bthPs3NullInfPath)
            );

            if (nullDrvInstRet?.ExitCode != 0 && nullDrvInstRet?.ExitCode != 3010)
            {
                session.Log(
                    $"NULL driver installation failed with exit code: {nullDrvInstRet?.ExitCode}, message: {Win32Exception.GetMessageFor(nullDrvInstRet?.ExitCode)}");
                goto exitFailure;
            }

            session.Log("PDO NULL driver installed.");

            session.Log("Enabling profile service for BTHENUM.");

            BufferedCommandResult? serviceEnableRet = RunCommand(nefconcPath, builder => builder
                .Add("--enable-bluetooth-service")
                .Add("--service-name").Add(InstallScript.BthPs3ServiceName)
                .Add("--service-guid").Add(InstallScript.BthPs3ServiceGuid)
            );

            if (serviceEnableRet?.ExitCode != 0 && serviceEnableRet?.ExitCode != 3010)
            {
                session.Log(
                    $"Enabling service failed with exit code: {serviceEnableRet?.ExitCode}, message: {Win32Exception.GetMessageFor(serviceEnableRet?.ExitCode)}");
                goto exitFailure;
            }

            session.Log("Profile service for BTHENUM enabled.");

            session.Log($"--- END {nameof(InstallDriversLegacy)} SUCCESS ---");

            Record record = new(1);
            record[1] = "9003";

            session.Message(
                InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Information,
                record);

            return ActionResult.Success;
        }
        catch (Exception ex)
        {
            session.Log($"FTL: {ex}");
        }

        exitFailure:
        session.Log($"--- END {nameof(InstallDriversLegacy)} FAILURE ---");
        return ActionResult.Failure;
    }

    private static BufferedCommandResult? RunCommand(string executable, Action<ArgumentsBuilder> args)
    {
        return Cli.Wrap(executable)
            .WithArguments(args)
            .WithValidation(CommandResultValidation.None)
            .ExecuteBufferedAsync()
            .GetAwaiter()
            .GetResult();
    }

    /// <summary>
    ///     Put install logic here.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    [CustomAction]
    public static ActionResult InstallDrivers(Session session)
    {
        session.Log($"{nameof(InstallDrivers)} - USE_MODERN = {session.Property(CustomProperties.UseModern)}");

        if (!GetUseModern(session))
        {
            session.Log("USE_MODERN set to false, skipping action");
            return ActionResult.NotExecuted;
        }

        // validate the host radio transport before doing anything destructive: there's no
        // point tearing down an existing (possibly working) setup via UninstallDrivers below
        // if we already know we can't (re)install for this radio afterward
        if (!RadioTransport.TryGetHostRadioDevice(out PnPDevice preInstallRadioDevice) ||
            RadioTransport.GetTransportType(preInstallRadioDevice) == RadioTransportType.Unsupported)
        {
            session.Log(
                "WARN: Host radio not found or its transport is unsupported, aborting before uninstalling any existing setup");

            Record unsupportedTransportRecord = new(1);
            unsupportedTransportRecord[1] = "9004";

            session.Message(
                InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Error,
                unsupportedTransportRecord);

            return ActionResult.Failure;
        }

        // clean out whatever has been on the machine before
        UninstallDrivers(session);

        session.Log($"--- BEGIN {nameof(InstallDrivers)} ---");

        try
        {
            #region Paths

            DirectoryInfo installDir = new(session.Property("INSTALLDIR"));
            session.Log($"installDir = {installDir}");
            string driversDir = Path.Combine(installDir.FullName, "drivers");
            session.Log($"driversDir = {driversDir}");
            string nefconDir = Path.Combine(installDir.FullName, "nefcon");
            session.Log($"nefconDir = {nefconDir}");
            string archShortName = ArchitectureInfo.PlatformShortName;
            session.Log($"archShortName = {archShortName}");

            string nefconcPath = Path.Combine(nefconDir, archShortName, "nefconc.exe");
            session.Log($"nefconcPath = {nefconcPath}");

            string bthPs3DriverDir = Path.Combine(driversDir, "BthPS3");
            session.Log($"bthPs3DriverDir = {bthPs3DriverDir}");
            string bthPs3PsmDriverDir = Path.Combine(driversDir, "BthPS3PSM");
            session.Log($"bthPs3PsmDriverDir = {bthPs3PsmDriverDir}");

            string bthPs3InfPath = Path.Combine(bthPs3DriverDir, "BthPS3.inf");
            session.Log($"bthPs3InfPath = {bthPs3InfPath}");
            string bthPs3PsmInfPath = Path.Combine(bthPs3PsmDriverDir, "BthPS3PSM.inf");
            session.Log($"bthPs3PsmInfPath = {bthPs3PsmInfPath}");
            string bthPs3NullInfPath = Path.Combine(bthPs3DriverDir, "BthPS3_PDO_NULL_Device.inf");
            session.Log($"bthPs3NullInfPath = {bthPs3NullInfPath}");

            #endregion

            #region Filter install

            // BthPS3PSM filter install
            BufferedCommandResult result = Cli.Wrap(nefconcPath)
                .WithArguments(builder => builder
                    .Add("--inf-default-install")
                    .Add("--inf-path")
                    .Add(bthPs3PsmInfPath)
                )
                .WithValidation(CommandResultValidation.None)
                .ExecuteBufferedAsync()
                .GetAwaiter()
                .GetResult();

            session.Log($"BthPS3PSM command stdout: {result.StandardOutput}");
            session.Log($"BthPS3PSM command stderr: {result.StandardError}");

            bool rebootRequired = result.ExitCode == 3010;

            if (result.ExitCode != 0 && result.ExitCode != 3010)
            {
                session.Log(
                    $"Filter installer failed with exit code: {result.ExitCode}, message: {Win32Exception.GetMessageFor(result.ExitCode)}");

                goto exitFailure;
            }

            #endregion

            #region Register filter

            try
            {
                // re-validate right before writing LowerFilters: the ProjectOnLoad preflight
                // only guards the interactive UI session load, not this (possibly later,
                // possibly separately elevated) deferred custom action
                if (!RadioTransport.TryGetHostRadioDevice(out PnPDevice radioDevice) ||
                    RadioTransport.GetTransportType(radioDevice) == RadioTransportType.Unsupported)
                {
                    session.Log(
                        "WARN: Host radio not found or its transport is unsupported, aborting before filter registration");

                    Record unsupportedTransportRecord = new(1);
                    unsupportedTransportRecord[1] = "9004";

                    session.Message(
                        InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Error,
                        unsupportedTransportRecord);

                    goto exitFailure;
                }

                // register filter
                session.Log("Adding lower filter entry");
                DeviceClassFilters.AddLower(DeviceClassIds.Bluetooth, FilterDriver.FilterServiceName);
                session.Log("Added lower filter entry");
            }
            catch (Exception ex)
            {
                session.Log($"Adding lower filter failed with error {ex}");
                goto exitFailure;
            }

            #endregion

            #region Restart radio

            bool radioRestartSucceeded;

            try
            {
                Record restartTimeoutRecord = new(1);
                restartTimeoutRecord[1] = "9002";
                MessageResult dialogResult = MessageResult.Abort;
                bool restartSuccess = false;

                do
                {
                    restartSuccess = RestartRadioAndAwait(session);

                    dialogResult =
                        restartSuccess
                            ? MessageResult.OK
                            : session.Message(
                                InstallMessage.User | (InstallMessage)MessageButtons.AbortRetryIgnore |
                                (InstallMessage)MessageIcon.Warning,
                                restartTimeoutRecord);
                } while (!restartSuccess && dialogResult == MessageResult.Retry);

                if (dialogResult == MessageResult.Abort)
                {
                    session.Log("User aborted operation");
                    goto exitFailure;
                }

                radioRestartSucceeded = restartSuccess;

                if (!radioRestartSucceeded)
                {
                    // user chose to ignore the failed/timed-out restart confirmation; the new
                    // lower filter registration will only be picked up by the stack after a
                    // reboot, so defer the filter-enable IOCTL below and force the reboot prompt
                    session.Log(
                        "Radio restart was not confirmed, deferring filter activation until reboot");
                    rebootRequired = true;
                }
            }
            catch (Exception ex)
            {
                session.Log($"Radio restart failed with error {ex}");
                goto exitFailure;
            }

            #endregion

            #region Profile driver

            session.Log("Installing profile driver");

            if (!Devcon.Install(bthPs3InfPath, out bool profileRebootRequired))
            {
                int error = Marshal.GetLastWin32Error();
                session.Log(
                    $"Profile driver installation failed with win32 error: {error}, message: {Win32Exception.GetMessageFor(error)}");

                goto exitFailure;
            }

            session.Log("Installed profile driver");

            if (profileRebootRequired)
            {
                rebootRequired = true;
            }

            #endregion

            #region NULL driver

            session.Log("Installing NULL driver");

            if (!Devcon.Install(bthPs3NullInfPath, out bool nullRebootRequired))
            {
                int error = Marshal.GetLastWin32Error();
                session.Log(
                    $"NULL driver installation failed with win32 error: {error}, message: {Win32Exception.GetMessageFor(error)}");

                goto exitFailure;
            }

            session.Log("Installed NULL driver");

            if (nullRebootRequired)
            {
                rebootRequired = true;
            }

            #endregion

            #region Profile driver service

            try
            {
                HostRadio radio = new();

                try
                {
                    session.Log("Enabling BthPS3 service");
                    // enable service, spawns profile driver PDO
                    radio.EnableService(InstallScript.BthPs3ServiceGuid, InstallScript.BthPs3ServiceName);
                    session.Log("Enabled BthPS3 service");
                }
                catch (Exception ex)
                {
                    session.Log($"Enabling service failed with error {ex}");
                    goto exitFailure;
                }
                finally
                {
                    radio.Dispose();
                }
            }
            catch (Exception ex)
            {
                session.Log($"Enabling radio failed with error {ex}");
                goto exitFailure;
            }

            #endregion

            #region Filter settings

            if (radioRestartSucceeded)
            {
                try
                {
                    session.Log("Enabling PSM filter");
                    // make sure patching is enabled, might not be in the registry
                    FilterDriver.IsFilterEnabled = true;
                    session.Log("Enabled PSM filter");
                }
                catch (Exception ex)
                {
                    session.Log($"Enabling filter failed with {ex}");
                    goto exitFailure;
                }
            }
            else
            {
                // the control device (\\.\BthPS3PSMControl) is only created once the filter
                // attaches, which won't happen until the pending reboot; skip the IOCTL to avoid
                // a spurious failure and rely on the profile driver's AutoEnableFilter default
                session.Log("Skipping PSM filter enable IOCTL; filter isn't loaded until reboot");
            }

            #endregion

            if (rebootRequired)
            {
                Record record = new(1);
                record[1] = "9000";

                session.Message(
                    InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Information,
                    record);
            }

            session.Log($"--- END {nameof(InstallDrivers)} SUCCESS ---");

            return ActionResult.Success;
        }
        catch (Exception ex)
        {
            session.Log($"FTL: {ex}");
        }

        exitFailure:
        session.Log($"--- END {nameof(InstallDrivers)} FAILURE ---");
        return ActionResult.Failure;
    }

    /// <summary>
    ///     Attempts to restart the radio and waits until the Bluetooth interface comes back online.
    /// </summary>
    /// <param name="session">The <see cref="Session" />.</param>
    /// <returns>True on success, false on timeout.</returns>
    /// <remarks>
    ///     Reloading the device stack is the only way for a freshly registered Bluetooth class lower filter
    ///     (<c>BthPS3PSM</c>) to attach without a full reboot. USB radios support a hot port-cycle; BTHX/BthMini-bound
    ///     radios (e.g. Intel PCIe <c>iBtPciBus</c>) do not, so those are restarted by removing and re-enumerating the
    ///     device node instead. See #137.
    /// </remarks>
    private static bool RestartRadioAndAwait(Session session)
    {
        object syncRoot = new();
        bool disposed = false;
        AutoResetEvent waitEvent = new(false);
        DeviceNotificationListener listener = new();

        try
        {
            listener.RegisterDeviceArrived(RadioDeviceArrived, HostRadio.DeviceInterface);
            listener.StartListen(HostRadio.DeviceInterface);

            void RadioDeviceArrived(DeviceEventArgs obj)
            {
                // the arrival callback can fire on a notification thread concurrently with the
                // WaitOne timeout below returning on this thread and running the finally block;
                // guard against touching already (or concurrently being) disposed objects
                // ReSharper disable AccessToModifiedClosure
                lock (syncRoot)
                {
                    if (disposed)
                    {
                        return;
                    }

                    session.Log("Radio arrival event, path: {0}", obj.SymLink);
                    // ReSharper disable once AccessToDisposedClosure
                    listener.StopListen(HostRadio.DeviceInterface);
                    // ReSharper disable once AccessToDisposedClosure
                    waitEvent.Set();
                }
                // ReSharper restore AccessToModifiedClosure
            }

            if (!RadioTransport.TryGetHostRadioDevice(out PnPDevice radioDevice))
            {
                session.Log("WARN: Radio device not found, cannot restart");
                return false;
            }

            RadioTransportType transportType = RadioTransport.GetTransportType(radioDevice);
            session.Log($"Radio transport type detected as {transportType}");

            session.Log("Restarting radio device");

            switch (transportType)
            {
                case RadioTransportType.Usb:
                    // restart device, filter is loaded afterward
                    HostRadio.RestartRadioDevice();
                    break;
                case RadioTransportType.Bthx:
                    // USB port-cycling doesn't apply to BTHX/BthMini-bound radios; rebuild the
                    // device stack instead so the updated Bluetooth class LowerFilters
                    // registration is picked up without requiring a reboot
                    radioDevice.RemoveAndSetup();
                    break;
                default:
                    session.Log("WARN: Unsupported radio transport, skipping restart");
                    return false;
            }

            session.Log("Restarted radio device");
            session.Log("Waiting for radio device to come online...");

            // wait until either event fired OR the timeout has been reached
            if (!waitEvent.WaitOne(TimeSpan.FromSeconds(30)))
            {
                return false;
            }

            // require both a live radio interface and a resolvable device node before
            // reporting success, so a transient/incomplete re-enumeration (e.g. right after a
            // BTHX RemoveAndSetup) falls through to the deferred/reboot path instead of being
            // reported as a successful restart
            bool radioAvailable = HostRadio.IsAvailable && RadioTransport.TryGetHostRadioDevice(out _);

            session.Log(!radioAvailable
                ? "WARN: Radio not available after wait period"
                : "Radio available after restart");

            return radioAvailable;
        }
        catch (Exception ex)
        {
            session.Log($"Restarting radio device failed with error {ex}");
        }
        finally
        {
            lock (syncRoot)
            {
                disposed = true;
                listener.Dispose();
                waitEvent.Dispose();
            }
        }

        return false;
    }

    /// <summary>
    ///     Custom action wrapper for <see cref="UninstallDriversLegacy" />, scheduled before
    ///     <c>RemoveFiles</c> so packaged files (nefconc.exe) still exist when it runs.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    [CustomAction]
    public static ActionResult UninstallDriversLegacyAction(Session session)
    {
        UninstallDriversLegacy(session);
        return ActionResult.Success;
    }

    /// <summary>
    ///     Uninstalls and cleans all driver residue via old method.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    public static void UninstallDriversLegacy(Session session)
    {
        session.Log($"--- BEGIN {nameof(UninstallDriversLegacy)} ---");

        try
        {
            #region Paths

            DirectoryInfo installDir = new(session.Property("INSTALLDIR"));
            session.Log($"installDir = {installDir}");
            string driversDir = Path.Combine(installDir.FullName, "drivers");
            session.Log($"driversDir = {driversDir}");
            string nefconDir = Path.Combine(installDir.FullName, "nefcon");
            session.Log($"nefconDir = {nefconDir}");
            string archShortName = ArchitectureInfo.PlatformShortName;
            session.Log($"archShortName = {archShortName}");

            string nefconcPath = Path.Combine(nefconDir, archShortName, "nefconc.exe");
            session.Log($"nefconcPath = {nefconcPath}");

            #endregion

            RunCommand(nefconcPath, builder => builder
                .Add("--remove-class-filter")
                .Add("--position").Add("lower")
                .Add("--service-name").Add(FilterDriver.FilterServiceName)
                .Add("--class-guid").Add(DeviceClassIds.Bluetooth)
            );

            RunCommand(nefconcPath, builder => builder
                .Add("--remove-device-node")
                .Add("--hardware-id").Add("BTHENUM\\{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}")
                .Add("--class-guid").Add(DeviceClassIds.Bluetooth)
            );

            RunCommand(nefconcPath, builder => builder
                .Add("--disable-bluetooth-service")
                .Add("--service-name").Add(InstallScript.BthPs3ServiceName)
                .Add("--service-guid").Add(InstallScript.BthPs3ServiceGuid)
            );
        }
        catch (Exception ex)
        {
            session.Log($"FTL: {ex}");
        }

        session.Log($"--- END {nameof(UninstallDriversLegacy)} ---");
    }

    /// <summary>
    ///     Uninstalls and cleans all driver residue.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    public static void UninstallDrivers(Session session)
    {
        session.Log($"--- BEGIN {nameof(UninstallDrivers)} ---");

        try
        {
            // de-register filter
            session.Log("Removing lower filter entry");
            DeviceClassFilters.RemoveLower(DeviceClassIds.Bluetooth, FilterDriver.FilterServiceName);
            session.Log("Removed lower filter entry");
        }
        catch (Exception ex)
        {
            session.Log($"Removing lower filter failed with error {ex}");
        }

        try
        {
            HostRadio radio = new();

            try
            {
                session.Log("Disabling BthPS3 service");
                // disabling service unloads PDO
                radio.DisableService(InstallScript.BthPs3ServiceGuid, InstallScript.BthPs3ServiceName);
                session.Log("Disabled BthPS3 service");

                session.Log("Disabling radio");
                // unloads complete stack
                radio.DisableRadio();
                session.Log("Disabled radio");
            }
            catch (Exception ex)
            {
                session.Log($"Disabling service or radio failed with error {ex}");
            }
            finally
            {
                radio.Dispose();
            }
        }
        catch (Exception ex)
        {
            session.Log($"FTL: radio access for disabling service failed with {ex}, ignoring");
        }

        // ignore errors
        RestartRadioAndAwait(session);

        #region Driver store clean-up

        List<string> allDriverPackages = DriverStore.ExistingDrivers.ToList();

        // remove all old copies of BthPS3
        foreach (string driverPackage in allDriverPackages.Where(p =>
                     p.IndexOf("bthps3.inf", StringComparison.OrdinalIgnoreCase) >= 0))
        {
            try
            {
                session.Log($"Removing driver package {driverPackage}");
                DriverStore.RemoveDriver(driverPackage);
            }
            catch (Exception ex)
            {
                session.Log($"Removal of BthPS3 package {driverPackage} failed with error {ex}");
            }
        }

        // remove all old copies of BthPS3 NULL driver
        foreach (string driverPackage in allDriverPackages.Where(p =>
                     p.IndexOf("bthps3_pdo_null_device.inf", StringComparison.OrdinalIgnoreCase) >= 0))
        {
            try
            {
                session.Log($"Removing driver package {driverPackage}");
                DriverStore.RemoveDriver(driverPackage);
            }
            catch (Exception ex)
            {
                session.Log($"Removal of BthPS3 NULL driver package {driverPackage} failed with error {ex}");
            }
        }

        // remove all old copies of BthPS3PSM
        foreach (string driverPackage in allDriverPackages.Where(p =>
                     p.IndexOf("bthps3psm.inf", StringComparison.OrdinalIgnoreCase) >= 0))
        {
            try
            {
                session.Log($"Removing driver package {driverPackage}");
                DriverStore.RemoveDriver(driverPackage);
            }
            catch (Exception ex)
            {
                session.Log($"Removal of BthPS3PSM package {driverPackage} failed with error {ex}");
            }
        }

        #endregion

        try
        {
            HostRadio radio = new();

            try
            {
                session.Log("Enabling radio");
                // starts vanilla radio operation
                radio.EnableRadio();
                session.Log("Enabled radio");
            }
            catch (Exception ex)
            {
                session.Log($"Enabling radio failed with error {ex}");
            }
            finally
            {
                radio.Dispose();
            }
        }
        catch (Exception ex)
        {
            session.Log($"FTL: Radio access for enabling failed, ignoring: {ex}");
        }

        session.Log($"--- END {nameof(UninstallDrivers)} ---");
    }

    #endregion

    #region Updater

    /// <summary>
    ///     Register the auto-updater.
    /// </summary>
    [CustomAction]
    public static ActionResult RegisterUpdater(Session session)
    {
        try
        {
            DirectoryInfo installDir = new(session.Property("INSTALLDIR"));
            string updaterPath = Path.Combine(installDir.FullName, "nefarius_BthPS3_Updater.exe");

            CommandResult? result = Cli.Wrap(updaterPath)
                .WithArguments(builder => builder
                    .Add("--install")
                    .Add("--silent")
                    .Add("--override-success-code 0")
                )
                .WithValidation(CommandResultValidation.None)
                .ExecuteAsync()
                .GetAwaiter()
                .GetResult();

            session.Log(
                $"Updater registration {(result.IsSuccess ? "succeeded" : "failed")}, exit code: {result.ExitCode}");
        }
        catch (Exception ex)
        {
            // don't fail the whole install if the updater (a non-essential component) can't
            // be registered, e.g. because AV quarantined it
            session.Log($"Updater registration failed with error {ex}");
        }

        return ActionResult.Success;
    }

    /// <summary>
    ///     De-register the auto-updater.
    /// </summary>
    [CustomAction]
    public static ActionResult DeregisterUpdater(Session session)
    {
        try
        {
            DirectoryInfo installDir = new(session.Property("INSTALLDIR"));
            string updaterPath = Path.Combine(installDir.FullName, "nefarius_BthPS3_Updater.exe");

            CommandResult? result = Cli.Wrap(updaterPath)
                .WithArguments(builder => builder
                    .Add("--uninstall")
                    .Add("--silent")
                    .Add("--override-success-code 0")
                )
                .WithValidation(CommandResultValidation.None)
                .ExecuteAsync()
                .GetAwaiter()
                .GetResult();

            session.Log(
                $"Updater de-registration {(result.IsSuccess ? "succeeded" : "failed")}, exit code: {result.ExitCode}");

            return ActionResult.Success;
        }
        catch
        {
            //
            // Not failing a removal here
            // 
            return ActionResult.Success;
        }
    }

    #endregion

    #region ETW Manifests

    /// <summary>
    ///     Installs the ETW manifests
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    [CustomAction]
    public static ActionResult InstallManifest(Session session)
    {
        DirectoryInfo installDir = new(session.Property("INSTALLDIR"));
        string bthPs3Manifest = Path.Combine(installDir.FullName, InstallScript.ManifestsDir, "BthPS3.man");
        string bthPs3PsmManifest = Path.Combine(installDir.FullName, InstallScript.ManifestsDir, "BthPS3PSM.man");

        CommandResult? bthPs3ManifestResult = Cli.Wrap("wevtutil")
            .WithArguments(builder => builder
                .Add("im")
                .Add(bthPs3Manifest)
            )
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync()
            .GetAwaiter()
            .GetResult();

        session.Log(
            $"BthPS3 manifest import {(bthPs3ManifestResult.IsSuccess ? "succeeded" : "failed")}, " +
            $"exit code: {bthPs3ManifestResult.ExitCode}");

        CommandResult? bthPs3PsmManifestResult = Cli.Wrap("wevtutil")
            .WithArguments(builder => builder
                .Add("im")
                .Add(bthPs3PsmManifest)
            )
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync()
            .GetAwaiter()
            .GetResult();

        session.Log(
            $"BthPS3PSM manifest import {(bthPs3PsmManifestResult.IsSuccess ? "succeeded" : "failed")}, " +
            $"exit code: {bthPs3PsmManifestResult.ExitCode}");

        return ActionResult.Success;
    }

    /// <summary>
    ///     Uninstalls the ETW manifests.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    [CustomAction]
    public static ActionResult UninstallManifest(Session session)
    {
        DirectoryInfo installDir = new(session.Property("INSTALLDIR"));
        string bthPs3Manifest = Path.Combine(installDir.FullName, InstallScript.ManifestsDir, "BthPS3.man");
        string bthPs3PsmManifest = Path.Combine(installDir.FullName, InstallScript.ManifestsDir, "BthPS3PSM.man");

        CommandResult? bthPs3ManifestResult = Cli.Wrap("wevtutil")
            .WithArguments(builder => builder
                .Add("um")
                .Add(bthPs3Manifest)
            )
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync()
            .GetAwaiter()
            .GetResult();

        session.Log(
            $"BthPS3 manifest removal {(bthPs3ManifestResult.IsSuccess ? "succeeded" : "failed")}, " +
            $"exit code: {bthPs3ManifestResult.ExitCode}");

        CommandResult? bthPs3PsmManifestResult = Cli.Wrap("wevtutil")
            .WithArguments(builder => builder
                .Add("um")
                .Add(bthPs3PsmManifest)
            )
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync()
            .GetAwaiter()
            .GetResult();

        session.Log(
            $"BthPS3PSM manifest removal {(bthPs3PsmManifestResult.IsSuccess ? "succeeded" : "failed")}, " +
            $"exit code: {bthPs3PsmManifestResult.ExitCode}");

        return ActionResult.Success;
    }

    #endregion
}