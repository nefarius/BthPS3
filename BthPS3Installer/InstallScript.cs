using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

using CliWrap;

using Microsoft.Win32;

using Nefarius.BthPS3.Setup.Util;
using Nefarius.BthPS3.Shared;
using Nefarius.Utilities.Bluetooth;
using Nefarius.Utilities.DeviceManagement.Drivers;
using Nefarius.Utilities.DeviceManagement.PnP;

using PInvoke;

using WixSharp;
using WixSharp.CommonTasks;
using WixSharp.Forms;

using WixToolset.Dtf.WindowsInstaller;

using static System.Collections.Specialized.BitVector32;

using File = WixSharp.File;
using RegistryHive = WixSharp.RegistryHive;

namespace Nefarius.BthPS3.Setup;

internal class InstallScript
{
    public const string ProductName = "Nefarius BthPS3 Bluetooth Drivers";
    public const string SetupRoot = @"..\setup";
    public const string DriversRoot = @"..\setup\drivers";
    public const string ManifestsDir = "manifests";

    public static string BthPS3ServiceName => "BthPS3Service";
    public static Guid BthPS3ServiceGuid => Guid.Parse("{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}");

    private static void Main()
    {
        Version version = Version.Parse(BuildVariables.SetupVersion);
        string archShortName = ArchitectureInfo.PlatformShortName;

        string driverPath = Path.Combine(DriversRoot, @"BthPS3_x64\BthPS3.sys");
        Version driverVersion = Version.Parse(FileVersionInfo.GetVersionInfo(driverPath).FileVersion);

        string filterPath = Path.Combine(DriversRoot, @"BthPS3PSM_x64\BthPS3PSM.sys");
        Version filterVersion = Version.Parse(FileVersionInfo.GetVersionInfo(filterPath).FileVersion);

        const string nefconDir = @".\nefcon";

        Console.WriteLine($"Setup version: {version}");
        Console.WriteLine($"Driver version: {driverVersion}");
        Console.WriteLine($"Filter version: {filterVersion}");

        Feature driversFeature = new("BthPS3 Bluetooth Drivers", true, false)
        {
            Description = "Installs the Nefarius BthPS3 drivers for PS3 peripherals. " +
                          "This is a mandatory core component and can't be de-selected."
        };

        ManagedProject project = new(ProductName,
            new Dir(driversFeature, @"%ProgramFiles%\Nefarius Software Solutions\BthPS3",
                // nefcon
                new Dir(driversFeature, "nefcon")
                {
                    Files = new DirFiles(driversFeature, "*.*").GetFiles(nefconDir),
                    Dirs = GetSubDirectories(driversFeature, nefconDir).ToArray()
                },
                // driver binaries
                new Dir(driversFeature, "drivers") { Dirs = GetSubDirectories(driversFeature, DriversRoot).ToArray() },
                // manifest files
                new Dir(driversFeature, ManifestsDir,
                    new File(driversFeature, @"..\BthPS3\BthPS3.man"),
                    new File(driversFeature, @"..\BthPS3PSM\BthPS3PSM.man")
                )
            ),
            // registry values
            new RegKey(driversFeature, RegistryHive.LocalMachine,
                $@"Software\Nefarius Software Solutions e.U.\{ProductName}",
                new RegValue("Path", "[INSTALLDIR]") { Win64 = true },
                new RegValue("Version", version.ToString()) { Win64 = true },
                new RegValue("DriverVersion", driverVersion.ToString()) { Win64 = true },
                new RegValue("FilterVersion", filterVersion.ToString()) { Win64 = true }
            ) { Win64 = true },
            // install drivers
            new ElevatedManagedAction(CustomActions.InstallDrivers, Return.check,
                When.After,
                Step.InstallFiles,
                Condition.NOT_Installed
            ),
            // install manifests
            new ElevatedManagedAction(CustomActions.InstallManifest, Return.check,
                When.After,
                Step.InstallFiles,
                Condition.NOT_Installed
            ),
            // remove manifests
            new ElevatedManagedAction(CustomActions.UninstallManifest, Return.check,
                When.Before,
                Step.RemoveFiles,
                Condition.Installed
            )
        )
        {
            GUID = new Guid("CC32A6ED-BDFE-4D51-9FFF-2AB51D9ECE18"),
            CAConfigFile = "CustomActions.config",
            OutFileName = $"Nefarius_BthPS3_Drivers_x64_arm64_v{version}",
            //custom set of standard UI dialogs
            ManagedUI = new ManagedUI(),
            Version = version,
            Platform = Platform.x64,
            WildCardDedup = Project.UniqueFileNameDedup
        };

        #region Fixes for setups < v2.10.x

        // override radio check with success
        project.AddProperty(new Property("RADIOFOUND", "1"));
        // override old detection check with absent
        project.AddProperty(new Property("FILTERNOTFOUND", "1"));
        // suppresses reboot dialogs from removing older versions
        project.AddProperty(new Property("REBOOT", "ReallySuppress"));

        #endregion

        project.MajorUpgrade = new MajorUpgrade
        {
            Schedule = UpgradeSchedule.afterInstallInitialize,
            DowngradeErrorMessage = "A later version of [ProductName] is already installed. Setup will now exit."
        };

        /*
        project.Load +=
            e =>
            {
                MessageBox.Show(e.Session.GetMainWindow(), e.ToString(),
                    "Before (Install/Uninstall) - " + new Version(e.Session["FOUNDPREVIOUSVERSION"]));
            };
        */

        project.ManagedUI.InstallDialogs.Add(Dialogs.Welcome)
            .Add(Dialogs.Licence)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.ManagedUI.ModifyDialogs.Add(Dialogs.MaintenanceType)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.AfterInstall += ProjectOnAfterInstall;

        // embed types of dependencies
        project.DefaultRefAssemblies.Add(typeof(Devcon).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(HostRadio).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(Cli).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(RegistryKey).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(ValueTask).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(IAsyncDisposable).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(Unsafe).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(BuffersExtensions).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(ArrayPool<>).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(Kernel32.SafeObjectHandle).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(FilterDriver).Assembly.Location);
        project.DefaultRefAssemblies.Add(typeof(BluetoothHelper).Assembly.Location);

        project.ControlPanelInfo.ProductIcon = @"..\Setup\Icons\B3.ico";
        project.ControlPanelInfo.Manufacturer = "Nefarius Software Solutions e.U.";
        project.ControlPanelInfo.HelpLink = "https://docs.nefarius.at/Community-Support/";
        project.ControlPanelInfo.UrlInfoAbout = "https://github.com/nefarius/BthPS3";
        project.ControlPanelInfo.NoModify = true;

        project.ResolveWildCards();

        project.BuildMsi();
    }

    /// <summary>
    ///     Put uninstall logic that doesn't access packaged files in here.
    /// </summary>
    /// <remarks>Runs with elevated privileges.</remarks>
    private static void ProjectOnAfterInstall(SetupEventArgs e)
    {
        if (e.IsUninstalling)
        {
            CustomActions.UninstallDrivers(e.Session);
        }
    }

    /// <summary>
    ///     Recursively resolves all subdirectories and their containing files.
    /// </summary>
    private static List<Dir> GetSubDirectories(Feature feature, string directory)
    {
        List<Dir> subDirectoryInfosCollection = new();

        foreach (string subDirectory in Directory.GetDirectories(directory))
        {
            string subDirectoryName = subDirectory.Remove(0, subDirectory.LastIndexOf('\\') + 1);
            Dir newDir =
                new(feature, subDirectoryName, new Files(feature, subDirectory + @"\*.*")) { Name = subDirectoryName };
            subDirectoryInfosCollection.Add(newDir);

            // Recursively traverse nested directories
            GetSubDirectories(feature, subDirectory);
        }

        return subDirectoryInfosCollection;
    }
}

public static class CustomActions
{
    /// <summary>
    ///     Put install logic here.
    /// </summary>
    [CustomAction]
    public static ActionResult InstallDrivers(Session session)
    {
        return ActionResult.Success;

        // clean out whatever has been on the machine before
        bool rebootRequired = UninstallDrivers(session);

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

        return ActionResult.Success;
    }

    /// <summary>
    ///     Uninstalls and cleans all driver residue.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    public static bool UninstallDrivers(Session session)
    {
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

        HostRadio radio = new();

        try
        {
            session.Log("Disabling BthPS3 service");
            // disabling service unloads PDO
            radio.DisableService(InstallScript.BthPS3ServiceGuid, InstallScript.BthPS3ServiceName);
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

        try
        {
            session.Log("Restarting radio device");
            // restart device, filter is unloaded afterward
            HostRadio.RestartRadioDevice();
            session.Log("Restarted radio device");
            // safety margin
            Thread.Sleep(1000);
        }
        catch (Exception ex)
        {
            session.Log($"Restarting radio device failed with error {ex}");
        }

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

        radio = new HostRadio();

        try
        {
            session.Log("Enabling radio");
            // starts vanilla radio operation
            radio.EnableRadio();
            session.Log("Enabled radio");
        }
        catch (Exception ex)
        {
            session.Log($"Disabling service or radio failed with error {ex}");
        }
        finally
        {
            radio.Dispose();
        }

        return false;
    }

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