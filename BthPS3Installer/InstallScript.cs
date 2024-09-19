using System;
using System.Buffers;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows.Forms;

using CliWrap;

using Microsoft.Win32;

using Nefarius.Utilities.Bluetooth;
using Nefarius.Utilities.DeviceManagement.PnP;

using WixSharp;
using WixSharp.Forms;

using WixToolset.Dtf.WindowsInstaller;

namespace Nefarius.BthPS3.Setup;

internal class InstallScript
{
    public const string ProductName = "BthPS3 Bluetooth Drivers";

    private static void Main()
    {
        Version version = Version.Parse(BuildVariables.SetupVersion);

        ManagedProject project = new(ProductName,
            new Dir(@"%ProgramFiles%\Nefarius Software Solutions\BthPS3",
                new File("InstallScript.cs")))
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