using System;

using Nuke.Common;
using Nuke.Common.CI.AppVeyor;
using Nuke.Common.Git;
using Nuke.Common.IO;
using Nuke.Common.ProjectModel;
using Nuke.Common.Tools.MSBuild;

using static Nuke.Common.Tools.MSBuild.MSBuildTasks;

class Build : NukeBuild
{
    [Parameter("Configuration to build - Default is 'Debug' (local) or 'Release' (server)")]
    readonly Configuration Configuration = IsLocalBuild ? Configuration.Debug : Configuration.Release;

    [GitRepository]
    readonly GitRepository GitRepository;

    [Solution]
    readonly Solution Solution;

    AbsolutePath DmfSolution => "C:/projects/DMF/Dmf.sln";
    AbsolutePath DomitoSolution => "C:/projects/Domito/Domito.sln";

    Target Clean => _ => _
        .Before(Restore)
        .Executes(() =>
        {
        });

    Target Restore => _ => _
        .Executes(() =>
        {
            MSBuild(s => s
                .SetTargetPath(Solution)
                .SetTargets("Restore"));
        });

    Target BuildDmf => _ => _
        .Executes(() =>
        {
            if (IsLocalBuild)
            {
                return;
            }

            Console.WriteLine($"DMF solution path: {DmfSolution}");

            MSBuildTargetPlatform platform = AppVeyor.Instance.Platform switch
            {
                "x86" => MSBuildTargetPlatform.Win32,
                "ARM64" => "ARM64",
                _ => MSBuildTargetPlatform.x64
            };

            MSBuild(s => s
                .SetTargetPath(DmfSolution)
                .SetTargets("Build")
                .SetConfiguration(Configuration)
                .SetTargetPlatform(platform)
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild));
        });

    Target BuildDomito => _ => _
        .Executes(() =>
        {
            if (IsLocalBuild)
            {
                return;
            }

            Console.WriteLine($"Domito solution path: {DomitoSolution}");

            MSBuildTargetPlatform platform = AppVeyor.Instance.Platform switch
            {
                "x86" => MSBuildTargetPlatform.Win32,
                "ARM64" => "ARM64",
                _ => MSBuildTargetPlatform.x64
            };

            MSBuild(s => s
                .SetTargetPath(DomitoSolution)
                .SetTargets("Build")
                .SetConfiguration(Configuration)
                .SetTargetPlatform(platform)
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild));
        });

    Target Compile => _ => _
        .DependsOn(Restore)
        .DependsOn(BuildDmf)
        .DependsOn(BuildDomito)
        .Executes(() =>
        {
            MSBuild(s => s
                .SetTargetPath(Solution)
                .SetTargets("Rebuild")
                .SetConfiguration(Configuration)
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild));
        });

    /// Support plugins are available for:
    /// - JetBrains ReSharper        https://nuke.build/resharper
    /// - JetBrains Rider            https://nuke.build/rider
    /// - Microsoft VisualStudio     https://nuke.build/visualstudio
    /// - Microsoft VSCode           https://nuke.build/vscode
    public static int Main() => Execute<Build>(x => x.Compile);
}