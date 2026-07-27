// do@Redlive

#include "../platform_tool.h"

#include "runtime/resource/file/file_system.h"

#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <fstream>

namespace dodoe {

#ifdef DO_PLATFORM_WINDOWS

    namespace fs = std::filesystem;

    namespace {

        String EscapeXml(String value) {
            auto replace_all = [](String& text, const String& from, const String& to) {
                size_t pos = 0;
                while ((pos = text.find(from, pos)) != String::npos) {
                    text.replace(pos, from.size(), to);
                    pos += to.size();
                }
            };

            replace_all(value, "&", "&amp;");
            replace_all(value, "\"", "&quot;");
            replace_all(value, "'", "&apos;");
            replace_all(value, "<", "&lt;");
            replace_all(value, ">", "&gt;");
            return value;
        }

        String MakeProjectRelativePath(const FsPath& base_dir, const FsPath& target_path) {
            std::error_code ec;
            const FsPath relative = fs::relative(target_path, base_dir, ec);
            return String((ec ? target_path.lexically_normal() : relative.lexically_normal()).generic_string().c_str());
        }

        FsPath FindCoreAssemblyPath() {
            const std::array candidates = {
                FileSystem::GetEngineRootPath() / "GreenCake.dll",
                FileSystem::GetEngineRootPath() / "engine" / "src" / "scriptcore" / "bin" / "Debug" / "net10.0" / "GreenCake.dll",
            };

            for (const auto& candidate : candidates) {
                if (fs::exists(candidate)) {
                    return candidate;
                }
            }

            return {};
        }

        bool CollectCSharpFiles(const FsPath& asset_directory, std::vector<FsPath>& out_files) {
            out_files.clear();
            if (!fs::exists(asset_directory) || !fs::is_directory(asset_directory)) {
                return false;
            }

            for (const auto& entry : fs::recursive_directory_iterator(asset_directory)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                if (entry.path().extension() == ".cs") {
                    out_files.push_back(entry.path().lexically_normal());
                }
            }

            std::sort(out_files.begin(), out_files.end());
            return true;
        }

        bool WriteGeneratedProjectFile(const FsPath& project_file,
            const std::vector<FsPath>& source_files,
            const FsPath& core_assembly_path,
            const String& assembly_name) {
            std::ofstream fout(project_file);
            if (!fout.is_open()) {
                return false;
            }

            const FsPath base_dir = project_file.parent_path();

            fout << "<Project Sdk=\"Microsoft.NET.Sdk\">\n\n";
            fout << "  <PropertyGroup>\n";
            fout << "    <TargetFramework>net10.0</TargetFramework>\n";
            fout << "    <ImplicitUsings>disable</ImplicitUsings>\n";
            fout << "    <Nullable>disable</Nullable>\n";
            fout << "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n";
            fout << "    <LangVersion>12.0</LangVersion>\n";
            fout << "    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n";
            fout << "    <OutputType>Library</OutputType>\n";
            fout << "    <AssemblyName>" << EscapeXml(assembly_name) << "</AssemblyName>\n";
            fout << "    <OutputPath>./</OutputPath>\n";
            fout << "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n";
            fout << "    <AppendRuntimeIdentifierToOutputPath>false</AppendRuntimeIdentifierToOutputPath>\n";
            fout << "    <BaseIntermediateOutputPath>./obj/</BaseIntermediateOutputPath>\n";
            fout << "    <IntermediateOutputPath>./obj/</IntermediateOutputPath>\n";
            fout << "  </PropertyGroup>\n\n";
            fout << "  <ItemGroup>\n";
            fout << "    <Reference Include=\"GreenCake\">\n";
            fout << "      <HintPath>" << EscapeXml(MakeProjectRelativePath(base_dir, core_assembly_path)) << "</HintPath>\n";
            fout << "      <Private>false</Private>\n";
            fout << "    </Reference>\n";
            fout << "  </ItemGroup>\n\n";
            fout << "  <ItemGroup>\n";
            for (const auto& source_file : source_files) {
                fout << "    <Compile Include=\"" << EscapeXml(MakeProjectRelativePath(base_dir, source_file)) << "\" />\n";
            }
            fout << "  </ItemGroup>\n\n";
            fout << "</Project>\n";
            return true;
        }

        bool RunProcessAndWait(const String& command_line, const FsPath& working_directory) {
            STARTUPINFOA startup_info{};
            startup_info.cb = sizeof(startup_info);
            PROCESS_INFORMATION process_info{};

            String mutable_command = command_line;
            const String working_dir_string(working_directory.string().c_str());
            const BOOL launched = CreateProcessA(
                nullptr,
                mutable_command.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                working_dir_string.empty() ? nullptr : working_dir_string.c_str(),
                &startup_info,
                &process_info);

            if (!launched) {
                const DWORD err = GetLastError();
                DO_ERROR("RunProcessAndWait: CreateProcessA failed for '{}', error code: {}", command_line, err);
                return false;
            }

            WaitForSingleObject(process_info.hProcess, INFINITE);

            DWORD exit_code = 1;
            GetExitCodeProcess(process_info.hProcess, &exit_code);
            CloseHandle(process_info.hThread);
            CloseHandle(process_info.hProcess);
            if (exit_code != 0) {
                DO_ERROR("RunProcessAndWait: '{}' exited with code {}", command_line, exit_code);
                return false;
            }
            return true;
        }

    } // namespace

    String PlatformTool::OpenProjectFileDialog() {
        char file_buffer[MAX_PATH]{};
        const String initial_dir = FileSystem::GetEngineRootPathString();
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = file_buffer;
        ofn.nMaxFile = static_cast<DWORD>(std::size(file_buffer));
        ofn.lpstrFilter = "Dodoe Project (*.doproj)\0*.doproj\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        ofn.lpstrInitialDir = initial_dir.c_str();
        return GetOpenFileNameA(&ofn) ? String(file_buffer) : String();
    }

    String PlatformTool::OpenDirectoryDialog(const String& initial_directory) {
        const HRESULT init_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool should_uninitialize = SUCCEEDED(init_result);

        IFileOpenDialog* dialog = nullptr;
        const HRESULT create_result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));
        if (FAILED(create_result) || !dialog) {
            if (should_uninitialize) {
                CoUninitialize();
            }
            return {};
        }

        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options))) {
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        }

        if (!initial_directory.empty()) {
            IShellItem* folder = nullptr;
            const std::wstring initial_directory_wide = FsPath(initial_directory).wstring();
            if (SUCCEEDED(SHCreateItemFromParsingName(initial_directory_wide.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        String selected_path;
        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR wide_path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wide_path)) && wide_path) {
                    selected_path = FsPath(wide_path).lexically_normal().generic_string();
                    CoTaskMemFree(wide_path);
                }
                item->Release();
            }
        }

        dialog->Release();
        if (should_uninitialize) {
            CoUninitialize();
        }
        return selected_path;
    }

    bool PlatformTool::BuildCSharpAssembly(const FsPath& asset_directory,
        const FsPath& output_directory,
        const String& assembly_name) {
        std::vector<FsPath> source_files;
        if (!CollectCSharpFiles(asset_directory, source_files)) {
            DO_ERROR("BuildCSharpAssembly: asset directory '{}' does not exist", asset_directory.string());
            return false;
        }

        std::error_code ec;
        fs::create_directories(output_directory, ec);

        if (source_files.empty()) {
            return true;
        }

        const FsPath core_assembly_path = FindCoreAssemblyPath();
        if (core_assembly_path.empty()) {
            DO_ERROR("BuildCSharpAssembly: GreenCake.dll not found (engine root: {})", FileSystem::GetEngineRootPathString());
            return false;
        }

        const FsPath generated_project_path = fs::absolute(output_directory / (assembly_name + ".generated.csproj"));
        if (!WriteGeneratedProjectFile(generated_project_path, source_files, core_assembly_path, assembly_name)) {
            DO_ERROR("BuildCSharpAssembly: failed to write project file '{}'", generated_project_path.string());
            return false;
        }

        const String command(std::string("dotnet build \"" + generated_project_path.string() + "\" -c Debug --nologo").c_str());
        return RunProcessAndWait(command, output_directory);
    }

#endif//DO_PLATFORM_WINDOWS

} // dodoe
