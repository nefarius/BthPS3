using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

using Nuke.Common;
using Nuke.Common.IO;
using Nuke.Common.ProjectModel;
using Nuke.Common.Tools.MSBuild;
using Nuke.Common.Tooling;

using Serilog;

using static Nuke.Common.Tools.MSBuild.MSBuildTasks;

class Build : NukeBuild
{
    [Parameter("Configuration to build - Default is 'Debug' (local) or 'Release' (server)")]
    readonly Configuration Configuration = IsLocalBuild ? Configuration.Debug : Configuration.Release;

    [Solution]
    readonly Solution Solution;

    [Parameter("Target platform for CI (x64 or ARM64). Not needed for local builds.")]
    readonly string TargetPlatform = "";

    [Parameter("Output path for release staging. Default: ./artifacts")]
    readonly string ArtifactsPath = "./artifacts";

    [Parameter("Path to the Microsoft-attested driver package (zip, cab, or extracted directory)")]
    readonly string MicrosoftPackagePath = "";

    AbsolutePath DmfSolution => RootDirectory / "DMF" / "Dmf.sln";

    AbsolutePath DomitoSolution => RootDirectory / "Domito" / "Domito.sln";

    AbsolutePath ResolvedArtifactsPath => (AbsolutePath)Path.GetFullPath(Path.Combine(RootDirectory, ArtifactsPath));

    /// <summary>
    /// Version stamp propagated from CI (BUILD_VERSION env var). Empty for local builds.
    /// </summary>
    static string BuildVersionStamp => Environment.GetEnvironmentVariable("BUILD_VERSION");

    /// <summary>
    /// MSBuild.exe located through vswhere. NUKE's resolver only probes VS2017-VS2022 folders.
    /// </summary>
    static string MSBuildPath => s_msBuildPath.Value;

    static readonly Lazy<string> s_msBuildPath = new(() =>
    {
        AbsolutePath vsWhere = (AbsolutePath)EnvironmentInfo.SpecialFolder(SpecialFolders.ProgramFilesX86)
            / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";

        if (vsWhere.FileExists())
        {
            string path = ProcessTasks.StartProcess(vsWhere,
                    "-latest -prerelease -products * -requires Microsoft.Component.MSBuild " +
                    @"-find MSBuild\**\Bin\amd64\MSBuild.exe", logOutput: false)
                .AssertZeroExitCode().Output
                .Select(x => x.Text.Trim())
                .FirstOrDefault(File.Exists);

            if (path != null)
            {
                Log.Information("Resolved MSBuild: {Path}", path);
                return path;
            }
        }

        return MSBuildToolPathResolver.Resolve();
    });

    Target Clean => _ => _
        .Before(Restore)
        .Executes(() =>
        {
        });

    Target Restore => _ => _
        .Executes(() =>
        {
            MSBuild(s => s
                .SetProcessToolPath(MSBuildPath)
                .SetTargetPath(Solution)
                .SetTargets("Restore"));
        });

    Target BuildDmf => _ => _
        .Executes(() =>
        {
            Log.Information("DMF solution path: {DmfSolution}", DmfSolution);

            foreach ((Configuration config, MSBuildTargetPlatform platform) in CiOrLocalCombinations())
            {
                Log.Information("Building DMF DmfK {Configuration} | {Platform}", config, platform);
                MSBuild(s => s
                    .SetProcessToolPath(MSBuildPath)
                    .SetTargetPath(DmfSolution)
                    .SetTargets("DmfK")
                    .SetConfiguration(config)
                    .SetTargetPlatform(platform)
                    .SetMaxCpuCount(Environment.ProcessorCount)
                    .SetNodeReuse(IsLocalBuild)
                    .SetVerbosity(MSBuildVerbosity.Minimal)
                );
            }
        });

    Target BuildDomito => _ => _
        .Executes(() =>
        {
            Log.Information("Domito solution path: {DomitoSolution}", DomitoSolution);

            foreach ((Configuration config, MSBuildTargetPlatform platform) in CiOrLocalCombinations())
            {
                Log.Information("Building Domito {Configuration} | {Platform}", config, platform);
                MSBuild(s => s
                    .SetProcessToolPath(MSBuildPath)
                    .SetTargetPath(DomitoSolution)
                    .SetTargets("Build")
                    .SetConfiguration(config)
                    .SetTargetPlatform(platform)
                    .SetMaxCpuCount(Environment.ProcessorCount)
                    .SetNodeReuse(IsLocalBuild)
                    .SetVerbosity(MSBuildVerbosity.Minimal)
                );
            }
        });

    Target Compile => _ => _
        .DependsOn(Restore)
        .DependsOn(BuildDmf)
        .DependsOn(BuildDomito)
        .Executes(() =>
        {
            if (IsLocalBuild)
            {
                foreach ((Configuration config, MSBuildTargetPlatform platform) in LocalCombinations())
                {
                    Log.Information("Compiling main solution {Configuration} | {Platform}", config, platform);
                    MSBuild(s => ApplyVersionStamp(s
                        .SetProcessToolPath(MSBuildPath)
                        .SetTargetPath(Solution)
                        .SetTargets("Rebuild")
                        .SetConfiguration(config)
                        .SetTargetPlatform(platform)
                        .SetMaxCpuCount(Environment.ProcessorCount)
                        .SetNodeReuse(IsLocalBuild)
                        .SetVerbosity(MSBuildVerbosity.Minimal)
                    ));
                }

                return;
            }

            MSBuildTargetPlatform ciPlatform = RequireCiPlatform();
            Log.Information("Compiling main solution {Configuration} | {Platform}", Configuration, ciPlatform);
            MSBuild(s => ApplyVersionStamp(s
                .SetProcessToolPath(MSBuildPath)
                .SetTargetPath(Solution)
                .SetTargets("Rebuild")
                .SetConfiguration(Configuration)
                .SetTargetPlatform(ciPlatform)
                .SetProperty("SignMode", "Off")
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild)
                .SetVerbosity(MSBuildVerbosity.Minimal)
            ));
        });

    /// <summary>
    /// Copies the unique BthPS3 and BthPS3PSM packages from a Microsoft-signed archive into artifacts/drivers.
    /// </summary>
    public Target IngestMicrosoftPackage => _ => _
        .Executes(() =>
        {
            if (string.IsNullOrWhiteSpace(MicrosoftPackagePath))
            {
                throw new InvalidOperationException(
                    "IngestMicrosoftPackage requires MicrosoftPackagePath (zip, cab, or extracted directory).");
            }

            string artifactsDir = ResolvedArtifactsPath;
            Directory.CreateDirectory(artifactsDir);
            ReleaseStaging.IngestMicrosoftPackage(MicrosoftPackagePath, ReleaseStaging.DriversDirectory(artifactsDir));
            ReleaseStaging.RequireDriverLayout(ReleaseStaging.DriversDirectory(artifactsDir));
            Log.Information("Ingested Microsoft-attested package into {Drivers}",
                ReleaseStaging.DriversDirectory(artifactsDir));
        });

    /// <summary>
    /// Runs offline version, INF, setup-release, and Partner Center dry-run checks.
    /// </summary>
    public Target TestReleasePipeline => _ => _
        .Executes(() =>
        {
            string shell = ToolPathResolver.TryGetEnvironmentExecutable("pwsh.exe")
                           ?? ToolPathResolver.TryGetEnvironmentExecutable("pwsh")
                           ?? TryGetPathExecutable("pwsh")
                           ?? "powershell";
            foreach (string testFile in new[]
                     {
                         "ReleaseVersion.Tests.ps1",
                         "SetupRelease.Tests.ps1",
                         "New-PartnerSubmissionInf.Tests.ps1",
                         "PartnerSigning.Tests.ps1",
                         "PartnerSigning.DryRun.ps1"
                     })
            {
                AbsolutePath tests = RootDirectory / "build" / testFile;
                ProcessTasks.StartProcess(shell, $"-NoProfile -File \"{tests}\"")
                    .AssertZeroExitCode();
            }

            ReleasePipelineTests.Run();
            Log.Information("Release pipeline tests passed");
        });

    IEnumerable<(Configuration config, MSBuildTargetPlatform platform)> LocalCombinations()
    {
        Configuration[] configs = [Configuration.Debug, Configuration.Release];
        MSBuildTargetPlatform[] platforms = [MSBuildTargetPlatform.x64, (MSBuildTargetPlatform)"ARM64"];
        return configs.SelectMany(config => platforms.Select(platform => (config, platform)));
    }

    IEnumerable<(Configuration config, MSBuildTargetPlatform platform)> CiOrLocalCombinations()
    {
        if (IsLocalBuild)
        {
            return LocalCombinations();
        }

        return [(Configuration, RequireCiPlatform())];
    }

    MSBuildTargetPlatform RequireCiPlatform()
    {
        if (string.IsNullOrWhiteSpace(TargetPlatform))
        {
            throw new InvalidOperationException("TargetPlatform must be set on CI, e.g. --target-platform x64.");
        }

        if (string.Equals(TargetPlatform, "ARM64", StringComparison.OrdinalIgnoreCase))
        {
            return (MSBuildTargetPlatform)"ARM64";
        }

        if (string.Equals(TargetPlatform, "x64", StringComparison.OrdinalIgnoreCase))
        {
            return MSBuildTargetPlatform.x64;
        }

        throw new InvalidOperationException($"Unsupported TargetPlatform '{TargetPlatform}'. Use x64 or ARM64.");
    }

    static MSBuildSettings ApplyVersionStamp(MSBuildSettings settings)
    {
        if (string.IsNullOrWhiteSpace(BuildVersionStamp))
        {
            return settings;
        }

        return settings
            .SetProperty("Version", BuildVersionStamp)
            .SetProperty("AssemblyVersion", BuildVersionStamp)
            .SetProperty("FileVersion", BuildVersionStamp)
            .SetProperty("InformationalVersion", BuildVersionStamp);
    }

    static string TryGetPathExecutable(string name)
    {
        try
        {
            return ToolPathResolver.GetPathExecutable(name);
        }
        catch (Exception)
        {
            return null;
        }
    }

    public static int Main() => Execute<Build>(x => x.Compile);
}
