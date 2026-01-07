import Downloads
import ZipArchives
import Tar
import CodecZlib

const BASE_URL = "https://github.com/google/or-tools/releases/download"
const ORTOOLS_MINOR_VERSION = "9.14"
const ORTOOLS_PATCH_VERSION = "9.14.6206"

const PACKAGE_FILE_NAME_WITHOUT_EXTENSION = Dict{String, String}(
    "linux_arm64" => "or-tools_aarch64_AlmaLinux-8.10_cpp_v$ORTOOLS_PATCH_VERSION",
    "linux_x64" => "or-tools_amd64_almalinux-9_cpp_v$ORTOOLS_PATCH_VERSION",
    "macos_arm64" => "or-tools_arm64_macOS-15.5_cpp_v$ORTOOLS_PATCH_VERSION",
    "macos_x64" => "or-tools_x86_64_macOS-15.5_cpp_v$ORTOOLS_PATCH_VERSION",
    "windows_x64" => "or-tools_x64_VisualStudio2022_cpp_v$ORTOOLS_PATCH_VERSION",
)

const TARGET_PACKAGES = Dict{String, String}(
    platform => "$BASE_URL/v$ORTOOLS_MINOR_VERSION/$name.$(ifelse(startswith(platform, "windows"), "zip", "tar.gz"))"
    for (platform, name) in PACKAGE_FILE_NAME_WITHOUT_EXTENSION
)

const DEPS_DIR = @__DIR__

println("WARNING: if ORTools_jll provides binaries for your platform, prefer using them rather than this package.")
println()
println("Downloading and installing a precompiled version of OR-Tools...")
println("BASE_URL: $BASE_URL, ORTOOLS_MINOR_VERSION: $ORTOOLS_MINOR_VERSION, ORTOOLS_PATCH_VERSION: $ORTOOLS_PATCH_VERSION")

key = "unknown"
if Sys.islinux()
    if Sys.ARCH === :x86_64
        key = "linux_x64"
    elseif Sys.ARCH === :aarch64
        key = "linux_arm64"
    else
        key = "linux_unknown"
    end
elseif Sys.isapple()
    if Sys.ARCH === :x86_64
        key = "macos_x64"
    elseif Sys.ARCH === :aarch64
        key = "macos_arm64"
    else
        key = "macos_unknown"
    end
elseif Sys.iswindows()
    if Sys.ARCH === :x86_64
        key = "windows_x64"
    elseif Sys.ARCH === :aarch64
        key = "windows_arm64"
    else
        key = "windows_unknown"
    end
end

println("Sys.islinux: $(Sys.islinux()), Sys.isapple: $(Sys.isapple()), Sys.iswindows: $(Sys.iswindows()), Sys.ARCH: $(Sys.ARCH)")
println("Detected platform: $key")
if !(key in keys(TARGET_PACKAGES))
    error("No package found for $key. Known packages: $(keys(TARGET_PACKAGES)). Maybe ORTools_jll contains a package for your platform.")
end

println("Downloading the following binary package:")
println(TARGET_PACKAGES[key])
package = Downloads.download(TARGET_PACKAGES[key])
println("Package downloaded. Size: $(filesize(package)) bytes, i.e. roughly $(round(Int, filesize(package) / 1024 / 1024)) MiB")
println("Local path (temporary): $package")

dest_dir = joinpath(DEPS_DIR, "lib")
if isdir(dest_dir)
    rm(dest_dir, recursive=true, force=true)
end
mkpath(dest_dir)

count_files = 0
if endswith(TARGET_PACKAGES[key], ".zip")
    # Only for Windows. The ZIP archive contains a folder with the same name
    # as the archive itself. We only need the DLLs in the `bin` folder.
    zr = ZipArchives.ZipReader(read(package))
    for name in ZipArchives.zip_names(zr)
        if startswith(name, PACKAGE_FILE_NAME_WITHOUT_EXTENSION[key]) && endswith(name, ".dll")
            filename = basename(name)
            println("Extracting: $filename (path in the ZIP archive: $name)")
            
            ZipArchives.zip_openentry(zr, name) do io
                open(joinpath(dest_dir, filename), "w") do f
                    write(f, io)
                end
            end
            global count_files
            count_files += 1
        end
    end
elseif endswith(TARGET_PACKAGES[key], ".tar.gz")
    # For all other platforms. The TAR.GZ archives contains a folder with the
    # same name as the archive itself. We need all the dynamic libraries in
    # the `lib` or `lib64` folder (`lib` is for MacOS, `lib64` is for Linux).
    open(package, "r") do tar_gz
        tar_stream = CodecZlib.GzipDecompressorStream(tar_gz)

        Tar.extract(tar_stream, dest_dir) do header
            name = header.path
            should_extract = startswith(name, PACKAGE_FILE_NAME_WITHOUT_EXTENSION[key]) && 
                             (endswith(name, ".so") || endswith(name, ".dylib"))
            if should_extract
                println("Extracting: $(basename(name)) (path in TAR.GZ archive: $(name))")
            end
            return should_extract
        end
    end

    # Flatten the output, because Tar.extract recreates whatever path it
    # finds in the archive.
    for (root, dirs, files) in walkdir(dest_dir)
        for file in files
            mv(joinpath(root, file), joinpath(dest_dir, file), force=true)
        end
    end

    # Clean up after flattening.
    rm(joinpath(dest_dir, PACKAGE_FILE_NAME_WITHOUT_EXTENSION[key]), recursive=true)
else
    error("Assertion failed: archive type not supported. Please report the problem to the maintainers of OR-Tools.")
end
println("Installed $count_files files in $dest_dir.")
