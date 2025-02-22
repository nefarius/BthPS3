using System;
using System.Buffers;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows.Forms;

using CliWrap;

using Microsoft.Win32;
using Microsoft.Win32.SafeHandles;

using Nefarius.BthPS3.Setup.Dialogues;
using Nefarius.BthPS3.Shared;
using Nefarius.Utilities.Bluetooth;
using Nefarius.Utilities.DeviceManagement.PnP;
using Nefarius.Utilities.WixSharp.Util;

using PInvoke;

using WixSharp;
using WixSharp.CommonTasks;
using WixSharp.Forms;

using WixToolset.Dtf.WindowsInstaller;

using File = WixSharp.File;
using RegistryHive = WixSharp.RegistryHive;

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
                    Dirs = WixExt.GetSubDirectories(driversFeature, nefconDir).ToArray()
                },
                // driver binaries
                new Dir(driversFeature, "drivers")
                {
                    Dirs = WixExt.GetSubDirectories(driversFeature, DriversRoot).ToArray()
                },
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
            new Property(CustomProperties.UseModern, bool.FalseString),
            // install drivers
            new ElevatedManagedAction(CustomActions.InstallDrivers, Return.check,
                When.After,
                Step.InstallFiles,
                Condition.NOT_Installed
            )
            {
                UsesProperties = CustomProperties.UseModern
            },
            // install drivers via legacy method
            new ElevatedManagedAction(CustomActions.InstallDriversLegacy, Return.check,
                When.After,
                Step.InstallFiles,
                Condition.NOT_Installed
            )
            {
                UsesProperties = CustomProperties.UseModern
            },
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
            ),
            new Error("9002",
                "Radio online detection timed out. " +
                "This error can be misleading on some systems (using Intel Wireless). " +
                "You can ignore the detection error and setup might be able to finish successfully. " +
                "You can retry the same operation again, which might fix it. " +
                "If you choose to abort, setup will end with an error."
            ),
            new Error("9003",
                "Legacy installation method was chosen. " +
                "After the setup is finished, you MUST REBOOT THE SYSTEM before using the software."
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
            DefaultFeature = driversFeature,
            LicenceFile = @"..\Setup\BthPS3_EULA.rtf",
            BackgroundImage = "left-banner.png",
            BannerImage = "top-banner.png"
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
            .Add(typeof(DriverSetupMethodSelector))
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.ManagedUI.ModifyDialogs.Add(Dialogs.MaintenanceType)
            .Add(Dialogs.Features)
            .Add(Dialogs.Progress)
            .Add(Dialogs.Exit);

        project.AfterInstall += ProjectOnAfterInstall;

        project.DefaultDeferredProperties += $",{CustomProperties.UseModern}";

        #region Embed types of dependencies

        project.EmbedCliWrap();

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
        project.DefaultRefAssemblies.Add(typeof(WixExt).Assembly.Location);

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
            CustomActions.UninstallDriversLegacy(e.Session);
        }
    }
}