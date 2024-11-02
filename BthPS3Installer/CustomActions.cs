﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

using CliWrap;
using CliWrap.Buffered;

using Nefarius.BthPS3.Shared;
using Nefarius.Utilities.Bluetooth;
using Nefarius.Utilities.DeviceManagement.Drivers;
using Nefarius.Utilities.DeviceManagement.Exceptions;
using Nefarius.Utilities.DeviceManagement.PnP;
using Nefarius.Utilities.WixSharp.Util;

using WixSharp;

using WixToolset.Dtf.WindowsInstaller;

namespace Nefarius.BthPS3.Setup;

public static class CustomActions
{
    #region Web

    [CustomAction]
    public static ActionResult OpenArticle(Session session)
    {
        if (!session.IsFeatureEnabled("PostInstArticle"))
        {
            return ActionResult.Success;
        }

        try
        {
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
    ///     Put install logic here.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    [CustomAction]
    public static ActionResult InstallDrivers(Session session)
    {
        // clean out whatever has been on the machine before
        UninstallDrivers(session);

        session.Log($"--- BEGIN {nameof(InstallDrivers)} ---");

        try
        {
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

            string bthPs3DriverDir = Path.Combine(driversDir, $"BthPS3_{archShortName}");
            session.Log($"bthPs3DriverDir = {bthPs3DriverDir}");
            string bthPs3PsmDriverDir = Path.Combine(driversDir, $"BthPS3PSM_{archShortName}");
            session.Log($"bthPs3PsmDriverDir = {bthPs3PsmDriverDir}");

            string bthPs3InfPath = Path.Combine(bthPs3DriverDir, "BthPS3.inf");
            session.Log($"bthPs3InfPath = {bthPs3InfPath}");
            string bthPs3PsmInfPath = Path.Combine(bthPs3PsmDriverDir, "BthPS3PSM.inf");
            session.Log($"bthPs3PsmInfPath = {bthPs3PsmInfPath}");
            string bthPs3NullInfPath = Path.Combine(bthPs3DriverDir, "BthPS3_PDO_NULL_Device.inf");
            session.Log($"bthPs3NullInfPath = {bthPs3NullInfPath}");

            #region Filter install

            // BthPS3PSM filter install
            BufferedCommandResult? result = Cli.Wrap(nefconcPath)
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

            bool rebootRequired = result?.ExitCode == 3010;

            if (result?.ExitCode != 0 && result?.ExitCode != 3010)
            {
                session.Log(
                    $"Filter installer failed with exit code: {result?.ExitCode}, message: {Win32Exception.GetMessageFor(result?.ExitCode)}");

                goto exitFailure;
            }

            #endregion

            #region Register filter

            try
            {
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
                    return ActionResult.Failure;
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
    /// <param name="session">The <see cref="Session"/>.</param>
    /// <returns>True on success, false on timeout.</returns>
    private static bool RestartRadioAndAwait(Session session)
    {
        AutoResetEvent waitEvent = new(false);
        DeviceNotificationListener listener = new();

        try
        {
            listener.RegisterDeviceArrived(RadioDeviceArrived, HostRadio.DeviceInterface);
            listener.StartListen(HostRadio.DeviceInterface);

            void RadioDeviceArrived(DeviceEventArgs obj)
            {
                session.Log("Radio arrival event, path: {0}", obj.SymLink);
                // ReSharper disable once AccessToDisposedClosure
                listener.StopListen(HostRadio.DeviceInterface);
                // ReSharper disable once AccessToDisposedClosure
                waitEvent.Set();
            }

            session.Log("Restarting radio device");
            // restart device, filter is loaded afterward
            HostRadio.RestartRadioDevice();
            session.Log("Restarted radio device");
            session.Log("Waiting for radio device to come online...");

            // wait until either event fired OR the timeout has been reached
            if (!waitEvent.WaitOne(TimeSpan.FromSeconds(30)))
            {
                return false;
            }

            session.Log(!HostRadio.IsAvailable
                ? "WARN: Radio not available after wait period"
                : "Radio available after restart");

            return true;
        }
        catch (Exception ex)
        {
            session.Log($"Restarting radio device failed with error {ex}");
        }
        finally
        {
            listener.Dispose();
            waitEvent.Dispose();
        }

        return false;
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
                     p.Contains("bthps3.inf", StringComparison.OrdinalIgnoreCase)))
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
                     p.Contains("bthps3_pdo_null_device.inf", StringComparison.OrdinalIgnoreCase)))
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
                     p.Contains("bthps3psm.inf", StringComparison.OrdinalIgnoreCase)))
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
            session.Log("FTL: Radio access for enabling failed, ignoring", ex);
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