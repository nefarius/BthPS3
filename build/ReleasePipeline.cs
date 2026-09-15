using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

static class ReleaseStaging
{
    public const string PublisherSubject = "Nefarius Software Solutions e.U.";
    public const string MicrosoftSignerHint = "Microsoft";
    public const string MetadataFileName = "release-metadata.json";

    public static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public static string DriversDirectory(string artifactsRoot) => Path.Combine(artifactsRoot, "drivers");

    public static string MetadataPath(string artifactsRoot) => Path.Combine(artifactsRoot, MetadataFileName);

    public static ReleaseMetadata ReadMetadata(string path)
    {
        if (!File.Exists(path))
        {
            throw new InvalidOperationException($"Release metadata not found: {path}");
        }

        string json = File.ReadAllText(path);
        ReleaseMetadata metadata = JsonSerializer.Deserialize<ReleaseMetadata>(json, JsonOptions)
                                   ?? throw new InvalidOperationException($"Release metadata is empty or invalid: {path}");
        metadata.ValidateIdentity();
        return metadata;
    }

    public static void WriteMetadata(string path, ReleaseMetadata metadata)
    {
        string directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        File.WriteAllText(path, JsonSerializer.Serialize(metadata, JsonOptions), new UTF8Encoding(false));
    }

    public static string Sha256File(string path)
    {
        using FileStream stream = File.OpenRead(path);
        byte[] hash = SHA256.HashData(stream);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    public static void ExtractArchive(string archivePath, string destination)
    {
        Directory.CreateDirectory(destination);
        string extension = Path.GetExtension(archivePath);
        if (extension.Equals(".zip", StringComparison.OrdinalIgnoreCase))
        {
            ZipFile.ExtractToDirectory(archivePath, destination, overwriteFiles: true);
            return;
        }

        if (extension.Equals(".cab", StringComparison.OrdinalIgnoreCase))
        {
            ProcessStartInfo info = new()
            {
                FileName = "expand.exe",
                Arguments = $"-F:* \"{archivePath}\" \"{destination}\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            using Process process = Process.Start(info)
                                    ?? throw new InvalidOperationException("Failed to start expand.exe.");
            Task<string> standardOutput = process.StandardOutput.ReadToEndAsync();
            Task<string> standardError = process.StandardError.ReadToEndAsync();
            process.WaitForExit();
            _ = standardOutput.GetAwaiter().GetResult();
            string error = standardError.GetAwaiter().GetResult();
            if (process.ExitCode != 0)
            {
                throw new InvalidOperationException(
                    $"expand.exe failed ({process.ExitCode}) for {archivePath}: {error}");
            }

            return;
        }

        throw new InvalidOperationException($"Unsupported Microsoft package archive: {archivePath}");
    }

    public static void IngestMicrosoftPackage(string source, string driversDir)
    {
        if (string.IsNullOrWhiteSpace(source))
        {
            throw new InvalidOperationException("MicrosoftPackagePath is required.");
        }

        string resolved = Path.GetFullPath(source);
        string packageRoot;
        string temp = null;

        try
        {
            if (File.Exists(resolved))
            {
                temp = Directory.CreateTempSubdirectory("bthps3-msft-").FullName;
                ExtractArchive(resolved, temp);
                packageRoot = FindAttestedRoot(temp);
            }
            else if (Directory.Exists(resolved))
            {
                packageRoot = FindAttestedRoot(resolved);
            }
            else
            {
                throw new InvalidOperationException($"Microsoft package not found: {resolved}");
            }

            RequireDriverLayout(packageRoot);

            if (Directory.Exists(driversDir))
            {
                Directory.Delete(driversDir, recursive: true);
            }

            CopyDirectory(packageRoot, driversDir);
            RequireDriverLayout(driversDir);
        }
        finally
        {
            if (temp != null && Directory.Exists(temp))
            {
                Directory.Delete(temp, recursive: true);
            }
        }
    }

    public static void RequireDriverLayout(string driversDir)
    {
        string[] required =
        [
            Path.Combine(driversDir, "BthPS3", "BthPS3.inf"),
            Path.Combine(driversDir, "BthPS3", "BthPS3_PDO_NULL_Device.inf"),
            Path.Combine(driversDir, "BthPS3", "x64", "BthPS3.sys"),
            Path.Combine(driversDir, "BthPS3", "ARM64", "BthPS3.sys"),
            Path.Combine(driversDir, "BthPS3PSM", "BthPS3PSM.inf"),
            Path.Combine(driversDir, "BthPS3PSM", "x64", "BthPS3PSM.sys"),
            Path.Combine(driversDir, "BthPS3PSM", "ARM64", "BthPS3PSM.sys")
        ];

        NormalizeArm64Folder(Path.Combine(driversDir, "BthPS3"));
        NormalizeArm64Folder(Path.Combine(driversDir, "BthPS3PSM"));

        List<string> missing = required.Where(path => !File.Exists(path)).ToList();
        if (missing.Count > 0)
        {
            throw new InvalidOperationException(
                "Driver package is missing required files:" + Environment.NewLine +
                string.Join(Environment.NewLine, missing));
        }
    }

    static void NormalizeArm64Folder(string packageDir)
    {
        string arm64 = Path.Combine(packageDir, "ARM64");
        string alias = Path.Combine(packageDir, "arm64");
        if (Directory.Exists(arm64) || !Directory.Exists(alias))
        {
            return;
        }

        Directory.CreateDirectory(arm64);
        foreach (string file in Directory.GetFiles(alias))
        {
            File.Copy(file, Path.Combine(arm64, Path.GetFileName(file)), overwrite: true);
        }
    }

    static string FindAttestedRoot(string root)
    {
        if (File.Exists(Path.Combine(root, "BthPS3", "BthPS3.inf")) &&
            File.Exists(Path.Combine(root, "BthPS3PSM", "BthPS3PSM.inf")))
        {
            return Path.GetFullPath(root);
        }

        IReadOnlyList<string> profile = Directory.GetFiles(root, "BthPS3.inf", SearchOption.AllDirectories)
            .Select(Path.GetDirectoryName)
            .Where(directory => !string.IsNullOrWhiteSpace(directory))
            .Select(Path.GetFullPath)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();

        if (profile.Count == 1)
        {
            string parent = Path.GetDirectoryName(profile[0]);
            if (!string.IsNullOrWhiteSpace(parent) &&
                File.Exists(Path.Combine(parent, "BthPS3PSM", "BthPS3PSM.inf")))
            {
                return parent;
            }
        }

        throw new InvalidOperationException(
            $"Could not find both BthPS3 and BthPS3PSM packages under {root}.");
    }

    static void CopyDirectory(string source, string destination)
    {
        Directory.CreateDirectory(destination);
        foreach (string directory in Directory.GetDirectories(source, "*", SearchOption.AllDirectories))
        {
            Directory.CreateDirectory(directory.Replace(source, destination, StringComparison.OrdinalIgnoreCase));
        }

        foreach (string file in Directory.GetFiles(source, "*", SearchOption.AllDirectories))
        {
            string dest = file.Replace(source, destination, StringComparison.OrdinalIgnoreCase);
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
            File.Copy(file, dest, overwrite: true);
        }
    }
}

sealed class ReleaseMetadata
{
    public int SchemaVersion { get; set; }

    public string Tag { get; set; }

    public string SetupVersion { get; set; }

    public string DriverVersion { get; set; }

    public string Commit { get; set; }

    public long RunId { get; set; }

    public string Repository { get; set; }

    public string PublisherSubject { get; set; }

    public ReleaseArtifactNames Artifacts { get; set; }

    public ReleaseFiles Files { get; set; }

    public void ValidateIdentity()
    {
        if (string.IsNullOrWhiteSpace(Tag) || !Regex.IsMatch(Tag, @"^v\d+\.\d+\.\d+$"))
        {
            throw new InvalidOperationException($"Release metadata tag is missing or not vMAJOR.MINOR.PATCH: '{Tag}'.");
        }

        if (string.IsNullOrWhiteSpace(SetupVersion) || SetupVersion != Tag[1..])
        {
            throw new InvalidOperationException(
                $"Release metadata setupVersion '{SetupVersion}' must match tag '{Tag}' without the v prefix.");
        }

        if (string.IsNullOrWhiteSpace(DriverVersion) ||
            DriverVersion.Split('.').Length != 4 ||
            !Version.TryParse(DriverVersion, out _))
        {
            throw new InvalidOperationException($"Release metadata driverVersion is invalid: '{DriverVersion}'.");
        }

        if (!DriverVersion.StartsWith(SetupVersion + ".", StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                $"Release metadata driverVersion '{DriverVersion}' is not derived from setupVersion '{SetupVersion}'.");
        }

        if (RunId <= 0)
        {
            throw new InvalidOperationException("Release metadata runId is missing.");
        }

        if (Files?.PartnerCab is not { } partnerCab)
        {
            throw new InvalidOperationException("Release metadata is missing files.partnerCab.");
        }

        if (string.IsNullOrWhiteSpace(partnerCab.Name) || string.IsNullOrWhiteSpace(partnerCab.Sha256))
        {
            throw new InvalidOperationException("Release metadata files.partnerCab must include name and sha256.");
        }
    }
}

sealed class ReleaseArtifactNames
{
    public string PartnerSubmission { get; set; }

    public string Tools { get; set; }

    public List<string> Platforms { get; set; }

    public string Metadata { get; set; }
}

sealed class ReleaseFiles
{
    public ReleaseFile PartnerCab { get; set; }
}

sealed class ReleaseFile
{
    public string Name { get; set; }

    public string Sha256 { get; set; }
}
