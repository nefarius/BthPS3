using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

using CliWrap;
using CliWrap.Buffered;

using Microsoft.Win32;
using Microsoft.Win32.SafeHandles;

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

using File = WixSharp.File;
using RegistryHive = WixSharp.RegistryHive;
using Win32Exception = Nefarius.Utilities.DeviceManagement.Exceptions.Win32Exception;

namespace Nefarius.BthPS3.Setup;

internal class InstallScript
{
    public const string ProductName = "Nefarius BthPS3 Bluetooth Drivers";
    public const string ArtifactsDir = @"..\setup\artifacts";
    public const string DriversRoot = @"..\setup\drivers";
    public const string ManifestsDir = "manifests";

    public static string BthPs3ServiceName => "BthPS3Service";
    public static Guid BthPs3ServiceGuid => Guid.Parse("{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}");

    private static void Main()
    {
        Version version = Version.Parse(BuildVariables.SetupVersion);

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

        Feature postInstallArticleFeature = new("Open post-installation article", true, true)
        {
            Id = "PostInstArticle",
            Description = "When setup has finished successfully, open the post-installation web article."
        };

        driversFeature.Add(postInstallArticleFeature);
        driversFeature.Display = FeatureDisplay.expand;

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
                ),
                // config tool
                new File(driversFeature, Path.Combine(ArtifactsDir, @"bin\BthPS3CfgUI.exe"),
                    new FileShortcut("BthPS3 Driver Configuration Tool") { WorkingDirectory = "[INSTALLDIR]" }
                ),
                new Dir(@"%ProgramMenu%\Nefarius Software Solutions\BthPS3",
                    new ExeFileShortcut("Uninstall BthPS3", "[System64Folder]msiexec.exe", "/x [ProductCode]"),
                    new ExeFileShortcut("BthPS3 Driver Configuration Tool", "[INSTALLDIR]BthPS3CfgUI.exe", "")
                ),
                new File(driversFeature, "nefarius_BthPS3_Updater.exe")
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
            ),
            // register updater
            new ManagedAction(CustomActions.RegisterUpdater, Return.check,
                When.After,
                Step.InstallFinalize,
                Condition.NOT_Installed
            ),
            // remove updater cleanly
            new ManagedAction(CustomActions.DeregisterUpdater, Return.check,
                When.Before,
                Step.RemoveFiles,
                Condition.Installed
            ),
            new ManagedAction(CustomActions.OpenArticle, Return.check,
                When.After,
                Step.InstallFinalize,
                Condition.NOT_Installed
            ),
            // custom reboot prompt message
            new Error("9000",
                "Driver installation succeeded but a reboot is required to be fully operational. " +
                "After the setup is finished, please reboot the system before using the software."
            ),
            new Error("9001",
                "No Bluetooth host radio was found. " +
                "This machine has to have working Bluetooth set up for some of the installation/removal steps to succeed. " +
                "Setup will now exit."
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
            WildCardDedup = Project.UniqueFileNameDedup,
            DefaultFeature = driversFeature
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

        project.Load += ProjectOnLoad;

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

        #region Embed types of dependencies

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
        project.DefaultRefAssemblies.Add(typeof(SafeRegistryHandle).Assembly.Location);

        #endregion

        #region Control Panel

        project.ControlPanelInfo.ProductIcon = @"..\Setup\Icons\B3.ico";
        project.ControlPanelInfo.Manufacturer = "Nefarius Software Solutions e.U.";
        project.ControlPanelInfo.HelpLink = "https://docs.nefarius.at/Community-Support/";
        project.ControlPanelInfo.UrlInfoAbout = "https://github.com/nefarius/BthPS3";
        project.ControlPanelInfo.NoModify = true;

        #endregion

        project.ResolveWildCards();

        project.BuildMsi();
    }

    private static void ProjectOnLoad(SetupEventArgs e)
    {
        if (HostRadio.IsAvailable)
        {
            return;
        }

        Session? session = e.Session;

        if (session is null)
        {
            e.Result = ActionResult.Failure;
            return;
        }

        Record record = new(1);
        record[1] = "9001";

        session.Message(
            InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Error,
            record);

        e.Result = ActionResult.UserExit;
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
    #region Web

    [CustomAction]
    public static ActionResult OpenArticle(Session session)
    {
        if (!session.IsFeatureEnabled("PostInstArticle"))
        {
            return ActionResult.Success;
        }

        CommandResult? result = Cli.Wrap("explorer")
            .WithArguments("https://docs.nefarius.at/projects/BthPS3/Welcome/Installation-Successful/")
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync()
            .GetAwaiter()
            .GetResult();

        session.Log(
            $"Beta article launch {(result.IsSuccess ? "succeeded" : "failed")}, exit code: {result.ExitCode}");

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

            return ActionResult.Failure;
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
            return ActionResult.Failure;
        }

        #endregion

        #region Restart radio

        try
        {
            session.Log("Restarting radio device");
            // restart device, filter is loaded afterward
            HostRadio.RestartRadioDevice();
            session.Log("Restarted radio device");
            // safety margin
            Thread.Sleep(1000);
        }
        catch (Exception ex)
        {
            session.Log($"Restarting radio device failed with error {ex}");
        }

        #endregion

        #region Profile driver

        if (!Devcon.Install(bthPs3InfPath, out bool profileRebootRequired))
        {
            int error = Marshal.GetLastWin32Error();
            session.Log(
                $"Profile driver installation failed with win32 error: {error}, message: {Win32Exception.GetMessageFor(error)}");

            return ActionResult.Failure;
        }

        if (profileRebootRequired)
        {
            rebootRequired = true;
        }

        #endregion

        #region NULL driver

        if (!Devcon.Install(bthPs3NullInfPath, out bool nullRebootRequired))
        {
            int error = Marshal.GetLastWin32Error();
            session.Log(
                $"NULL driver installation failed with win32 error: {error}, message: {Win32Exception.GetMessageFor(error)}");

            return ActionResult.Failure;
        }

        if (nullRebootRequired)
        {
            rebootRequired = true;
        }

        #endregion

        #region Profile driver service

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
            session.Log($"Enabling service or radio failed with error {ex}");
            return ActionResult.Failure;
        }
        finally
        {
            radio.Dispose();
        }

        #endregion

        #region Filter settings

        // make sure patching is enabled, might not be in the registry
        FilterDriver.IsFilterEnabled = true;

        #endregion

        if (rebootRequired)
        {
            Record record = new(1);
            record[1] = "9000";

            session.Message(
                InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Information,
                record);
        }

        return ActionResult.Success;
    }

    /// <summary>
    ///     Uninstalls and cleans all driver residue.
    /// </summary>
    /// <remarks>Requires elevated permissions.</remarks>
    public static void UninstallDrivers(Session session)
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