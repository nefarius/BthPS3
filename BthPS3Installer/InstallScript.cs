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

        string driverPath = Path.Combine(DriversRoot, @"BthPS3\x64\BthPS3.sys");
        string filterPath = Path.Combine(DriversRoot, @"BthPS3PSM\x64\BthPS3PSM.sys");
        string cfgUiPath = Path.Combine(ArtifactsDir, @"bin\BthPS3CfgUI.exe");

        RequireStagedPayload(driverPath, filterPath, cfgUiPath);

        Version driverVersion = Version.Parse(FileVersionInfo.GetVersionInfo(driverPath).FileVersion);
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
            new Property(CustomProperties.UseModern, bool.TrueString),
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
                new Condition("REMOVE=\"ALL\"")
            ),
            // remove driver residue via legacy method (must run before packaged files are
            // removed, since it shells out to INSTALLDIR\nefcon\<arch>\nefconc.exe)
            new ElevatedManagedAction(CustomActions.UninstallDriversLegacyAction, Return.check,
                When.Before,
                Step.RemoveFiles,
                new Condition("REMOVE=\"ALL\"")
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
                new Condition("REMOVE=\"ALL\"")
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
                "This error can be misleading on some systems (using Intel Wireless), and is expected on " +
                "BTHX/BthMini-based radios (e.g. Intel PCIe iBtPciBus), which cannot be power-cycled without a reboot. " +
                "Choosing Ignore lets setup finish; a reboot will then be required to fully load the driver. " +
                "You can retry the same operation again, which might fix it. " +
                "If you choose to abort, setup will end with an error."
            ),
            new Error("9003",
                "Legacy installation method was chosen. " +
                "After the setup is finished, you MUST REBOOT THE SYSTEM before using the software."
            ),
            new Error("9004",
                "The detected Bluetooth host radio is not attached via a supported transport (USB, or BTHX/BthMini " +
                "for select non-USB radios such as Intel PCIe iBtPciBus). " +
                "Installing the drivers would not work and could leave your Bluetooth stack in a broken state. " +
                "Setup will now exit without making changes."
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
        // suppresses reboot dialogs from removing older versions; note this applies to the
        // whole session (not just RemoveExistingProducts), so this setup never lets MSI itself
        // schedule a reboot - the 9000/9003 messages below are purely informational and rely on
        // the user rebooting manually
        project.AddProperty(new Property("REBOOT", "ReallySuppress"));

        #endregion

        project.MajorUpgrade = new MajorUpgrade
        {
            Schedule = UpgradeSchedule.afterInstallInitialize,
            DowngradeErrorMessage = "A later version of [ProductName] is already installed. Setup will now exit.",
            // ProductCode is regenerated on every build while the UpgradeCode (project.GUID)
            // stays fixed; without this, re-releasing the same SetupVersion (e.g. a re-spin
            // after a signing retry) installs side-by-side as a duplicate ARP entry instead of
            // upgrading the existing one.
            AllowSameVersionUpgrades = true
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
            .Add(typeof(InstallMethodDialog))
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
        // hides the "Change" button in Add/Remove Programs; ManagedUI.ModifyDialogs above is
        // still reachable via `msiexec /f` (repair) and is kept for that path
        project.ControlPanelInfo.NoModify = true;

        #endregion

        project.ResolveWildCards();

        project.BuildMsi();
    }

    /// <summary>
    ///     Fails fast with an actionable message when the expected staged driver/artifact
    ///     payload (Setup\drivers, Setup\artifacts\bin) is missing, instead of letting
    ///     <see cref="FileVersionInfo.GetVersionInfo(string)" /> throw an opaque
    ///     <see cref="FileNotFoundException" />.
    /// </summary>
    private static void RequireStagedPayload(params string[] requiredFiles)
    {
        string[] missing = Array.FindAll(requiredFiles, path => !System.IO.File.Exists(path));

        if (missing.Length == 0)
        {
            return;
        }

        throw new InvalidOperationException(
            "Setup payload is incomplete, missing:" + Environment.NewLine +
            string.Join(Environment.NewLine, missing) + Environment.NewLine +
            "Run Setup\\stage0.ps1, Setup\\stage1.ps1 (and stage2.ps1 to build) first, " +
            "or place the drivers/artifacts manually as described in Setup\\README.md.");
    }

    private static void ProjectOnLoad(SetupEventArgs e)
    {
        // this preflight only matters for a fresh install (it decides whether it is safe to
        // write the Bluetooth class LowerFilters registry value); running it during maintenance,
        // repair, or uninstall would incorrectly block those paths if the radio has since been
        // removed or disabled
        if (!e.IsInstalling)
        {
            return;
        }

        Session? session = e.Session;

        if (!HostRadio.IsAvailable)
        {
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
            return;
        }

        // reject transports the filter driver can't attach to (see #137) before setup writes
        // anything to the Bluetooth class LowerFilters registry value
        if (!RadioTransport.TryGetHostRadioDevice(out PnPDevice radioDevice))
        {
            // HostRadio.IsAvailable reported a radio present but the device node couldn't be
            // resolved; don't let install proceed to DeviceClassFilters.AddLower without knowing
            // which transport it targets
            if (session is null)
            {
                e.Result = ActionResult.Failure;
                return;
            }

            Record unresolvedDeviceRecord = new(1);
            unresolvedDeviceRecord[1] = "9004";

            session.Message(
                InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Error,
                unresolvedDeviceRecord);

            e.Result = ActionResult.UserExit;
            return;
        }

        if (RadioTransport.GetTransportType(radioDevice) != RadioTransportType.Unsupported)
        {
            return;
        }

        if (session is null)
        {
            e.Result = ActionResult.Failure;
            return;
        }

        Record unsupportedTransportRecord = new(1);
        unsupportedTransportRecord[1] = "9004";

        session.Message(
            InstallMessage.User | (InstallMessage)MessageButtons.OK | (InstallMessage)MessageIcon.Error,
            unsupportedTransportRecord);

        e.Result = ActionResult.UserExit;
    }

    /// <summary>
    ///     Put uninstall logic that doesn't access packaged files in here.
    /// </summary>
    /// <remarks>
    ///     Runs with elevated privileges, after <c>InstallFinalize</c> - i.e. after
    ///     <c>RemoveFiles</c> has already deleted the package payload. Legacy driver removal
    ///     needs the packaged <c>nefconc.exe</c> and is therefore scheduled separately as
    ///     <see cref="CustomActions.UninstallDriversLegacyAction" /> before <c>RemoveFiles</c>.
    /// </remarks>
    private static void ProjectOnAfterInstall(SetupEventArgs e)
    {
        if (e.IsUninstalling)
        {
            CustomActions.UninstallDrivers(e.Session);
        }
    }
}