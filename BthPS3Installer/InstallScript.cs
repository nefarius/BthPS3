using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing.Drawing2D;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows.Forms;

using CliWrap;

using Microsoft.Win32;

using Nefarius.BthPS3.Setup.Util;
using Nefarius.Utilities.Bluetooth;
using Nefarius.Utilities.DeviceManagement.PnP;

using WixSharp;
using WixSharp.Forms;

using WixToolset.Dtf.WindowsInstaller;

using File = WixSharp.File;
using RegistryHive = WixSharp.RegistryHive;

namespace Nefarius.BthPS3.Setup;

internal class InstallScript
{
    public const string ProductName = "BthPS3 Bluetooth Drivers";
    public const string SetupRoot = @"..\setup";

    private static void Main()
    {
        Version version = Version.Parse(BuildVariables.SetupVersion);
        string archShortName = ArchitectureInfo.PlatformShortName;

        string driverPath = Path.Combine(SetupRoot, @"drivers\BthPS3_x64\BthPS3.sys");
        Version driverVersion = Version.Parse(FileVersionInfo.GetVersionInfo(driverPath).FileVersion);

        string filterPath = Path.Combine(SetupRoot, @"drivers\BthPS3PSM_x64\BthPS3PSM.sys");
        Version filterVersion = Version.Parse(FileVersionInfo.GetVersionInfo(filterPath).FileVersion);

        Console.WriteLine($"Setup version: {version}");
        Console.WriteLine($"Driver version: {driverVersion}");
        Console.WriteLine($"Filter version: {filterVersion}");

        Feature driversFeature = new(ProductName, true, false)
        {
            Description = "Installs the Nefarius BthPS3 drivers for PS3 peripherals. " +
                          "This is a mandatory core component and can't be de-selected."
        };

        ManagedProject project = new(ProductName,
            new Dir(driversFeature, @"%ProgramFiles%\Nefarius Software Solutions\BthPS3",
                new File("InstallScript.cs")),
            // registry values
            new RegKey(driversFeature, RegistryHive.LocalMachine,
                $@"Software\Nefarius Software Solutions e.U.\{ProductName}",
                new RegValue("Path", "[INSTALLDIR]") { Win64 = true },
                new RegValue("Version", version.ToString()) { Win64 = true },
                new RegValue("DriverVersion", driverVersion.ToString()) { Win64 = true },
                new RegValue("FilterVersion", filterVersion.ToString()) { Win64 = true }
            ) { Win64 = true })
        {
            GUID = new Guid("CC32A6ED-BDFE-4D51-9FFF-2AB51D9ECE18"),
            CAConfigFile = "CustomActions.config",
            OutFileName = $"Nefarius_BthPS3_Drivers_x64_arm64_v{version}",
            //custom set of standard UI dialogs
            ManagedUI = new ManagedUI(),
            Version = version,
            Platform = Platform.x64,
            WildCardDedup = Project.UniqueFileNameDedup,
            MajorUpgradeStrategy = MajorUpgradeStrategy.Default
        };

        project.ManagedUI.InstallDialogs.Add(Dialogs.Welcome)
            .Add(Dialogs.Licence)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.ManagedUI.ModifyDialogs.Add(Dialogs.MaintenanceType)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.Load += Msi_Load;
        project.BeforeInstall += Msi_BeforeInstall;
        project.AfterInstall += Msi_AfterInstall;

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

        project.ControlPanelInfo.ProductIcon = @"..\Setup\Icons\B3.ico";
        project.ControlPanelInfo.Manufacturer = "Nefarius Software Solutions e.U.";
        project.ControlPanelInfo.HelpLink = "https://docs.nefarius.at/Community-Support/";
        project.ControlPanelInfo.UrlInfoAbout = "https://github.com/nefarius/BthPS3";
        project.ControlPanelInfo.NoModify = true;

        project.MajorUpgradeStrategy.PreventDowngradingVersions.OnlyDetect = false;

        project.ResolveWildCards();

        project.BuildMsi();
    }

    private static void Msi_Load(SetupEventArgs e)
    {
        if (!e.IsUISupressed && !e.IsUninstalling)
        {
            MessageBox.Show(e.ToString(), "Load");
        }
    }

    private static void Msi_BeforeInstall(SetupEventArgs e)
    {
        if (!e.IsUISupressed && !e.IsUninstalling)
        {
            MessageBox.Show(e.ToString(), "BeforeInstall");
        }
    }

    private static void Msi_AfterInstall(SetupEventArgs e)
    {
        if (!e.IsUISupressed && !e.IsUninstalling)
        {
            MessageBox.Show(e.ToString(), "AfterExecute");
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
    [CustomAction]
    public static ActionResult InstallManifest(Session session)
    {
        CommandResult? result = Cli.Wrap("wevtutil")
            .WithArguments(builder => builder
                .Add("im")
            )
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync()
            .GetAwaiter()
            .GetResult();

        session.Log($"Manifest import {(result.IsSuccess ? "succeeded" : "failed")}, exit code: {result.ExitCode}");

        return ActionResult.Success;
    }

    [CustomAction]
    public static ActionResult UninstallManifest(Session session)
    {
        CommandResult? result = Cli.Wrap("wevtutil")
            .WithArguments(builder => builder
                .Add("um")
            )
            .WithValidation(CommandResultValidation.None)
            .ExecuteAsync()
            .GetAwaiter()
            .GetResult();

        session.Log($"Manifest removal {(result.IsSuccess ? "succeeded" : "failed")}, exit code: {result.ExitCode}");

        return ActionResult.Success;
    }
}