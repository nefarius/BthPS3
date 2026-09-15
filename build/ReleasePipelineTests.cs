using System;
using System.IO;
using System.IO.Compression;

static class ReleasePipelineTests
{
    public static void Run()
    {
        TestMetadataRoundTrip();
        TestIngestFromDirectoryAndZip();
        TestDriverLayoutRequiresBothPackages();
    }

    static void TestMetadataRoundTrip()
    {
        using TempScope scope = new();
        ReleaseMetadata metadata = SampleMetadata();
        string path = Path.Combine(scope.Root, ReleaseStaging.MetadataFileName);
        ReleaseStaging.WriteMetadata(path, metadata);
        ReleaseMetadata read = ReleaseStaging.ReadMetadata(path);
        AssertEqual(read.Tag, metadata.Tag, "tag");
        AssertEqual(read.SetupVersion, metadata.SetupVersion, "setup");
        AssertEqual(read.DriverVersion, metadata.DriverVersion, "driver");
        AssertEqual(read.Files.PartnerCab.Name, metadata.Files.PartnerCab.Name, "cab name");

        metadata.Files.PartnerCab.Name = null;
        string bad = Path.Combine(scope.Root, "bad.json");
        ReleaseStaging.WriteMetadata(bad, metadata);
        AssertThrows(() => ReleaseStaging.ReadMetadata(bad), "missing partnerCab name");
    }

    static void TestIngestFromDirectoryAndZip()
    {
        using TempScope scope = new();
        string package = WriteDriverPackage(Path.Combine(scope.Root, "extracted"));
        string drivers = Path.Combine(scope.Root, "drivers");
        ReleaseStaging.IngestMicrosoftPackage(package, drivers);
        ReleaseStaging.RequireDriverLayout(drivers);

        string zip = Path.Combine(scope.Root, "signed.zip");
        ZipFile.CreateFromDirectory(package, zip);
        string driversFromZip = Path.Combine(scope.Root, "drivers-zip");
        ReleaseStaging.IngestMicrosoftPackage(zip, driversFromZip);
        ReleaseStaging.RequireDriverLayout(driversFromZip);
    }

    static void TestDriverLayoutRequiresBothPackages()
    {
        using TempScope scope = new();
        string package = WriteDriverPackage(Path.Combine(scope.Root, "partial"));
        File.Delete(Path.Combine(package, "BthPS3PSM", "BthPS3PSM.inf"));
        AssertThrows(() => ReleaseStaging.RequireDriverLayout(package), "missing filter package");
    }

    static string WriteDriverPackage(string root)
    {
        Directory.CreateDirectory(Path.Combine(root, "BthPS3", "x64"));
        Directory.CreateDirectory(Path.Combine(root, "BthPS3", "ARM64"));
        Directory.CreateDirectory(Path.Combine(root, "BthPS3PSM", "x64"));
        Directory.CreateDirectory(Path.Combine(root, "BthPS3PSM", "ARM64"));
        File.WriteAllText(Path.Combine(root, "BthPS3", "BthPS3.inf"), "inf");
        File.WriteAllText(Path.Combine(root, "BthPS3", "BthPS3_PDO_NULL_Device.inf"), "null");
        File.WriteAllText(Path.Combine(root, "BthPS3", "x64", "BthPS3.sys"), "sys");
        File.WriteAllText(Path.Combine(root, "BthPS3", "ARM64", "BthPS3.sys"), "sys");
        File.WriteAllText(Path.Combine(root, "BthPS3PSM", "BthPS3PSM.inf"), "inf");
        File.WriteAllText(Path.Combine(root, "BthPS3PSM", "x64", "BthPS3PSM.sys"), "sys");
        File.WriteAllText(Path.Combine(root, "BthPS3PSM", "ARM64", "BthPS3PSM.sys"), "sys");
        return root;
    }

    static ReleaseMetadata SampleMetadata()
    {
        return new ReleaseMetadata
        {
            SchemaVersion = 1,
            Tag = "v2.12.0",
            SetupVersion = "2.12.0",
            DriverVersion = "2.12.0.2001",
            Commit = "abc",
            RunId = 42,
            Repository = "nefarius/BthPS3",
            PublisherSubject = ReleaseStaging.PublisherSubject,
            Artifacts = new ReleaseArtifactNames
            {
                PartnerSubmission = "bthps3-partner-submission",
                Tools = "bthps3-tools",
                Platforms = ["bthps3-x64", "bthps3-ARM64"],
                Metadata = "release-metadata"
            },
            Files = new ReleaseFiles
            {
                PartnerCab = new ReleaseFile
                {
                    Name = "BthPS3_2.12.0.2001.cab",
                    Sha256 = new string('a', 64)
                }
            }
        };
    }

    static void AssertEqual(string actual, string expected, string name)
    {
        if (!string.Equals(actual, expected, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"FAIL {name}: expected '{expected}', got '{actual}'.");
        }
    }

    static void AssertThrows(Action action, string name)
    {
        try
        {
            action();
        }
        catch
        {
            return;
        }

        throw new InvalidOperationException($"FAIL {name}: expected an exception");
    }

    sealed class TempScope : IDisposable
    {
        public string Root { get; } = Directory.CreateTempSubdirectory("bthps3-release-").FullName;

        public void Dispose()
        {
            if (Directory.Exists(Root))
            {
                Directory.Delete(Root, recursive: true);
            }
        }
    }
}
