#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <regex>
#include <vector>

#include "StormLib.h"

inline const bool check(bool self, const char* message) noexcept {
    if (!self) [[unlikely]] {
        std::cerr << message << std::endl;
        std::cerr << "Error code: " << GetLastError() << std::endl;
        std::exit(1);
    }
    return self;
}

bool is_sc2_archive(const std::filesystem::path& input_folder) {
    return std::regex_search(input_folder.filename().generic_string(),
                             std::regex("\\.sc2(?:mod|map)$", std::regex_constants::icase));
}

int main(const int argc, const char* const* const args) {
    std::filesystem::path input_folder("");
    std::filesystem::path output_folder("");
    bool replace = false;
    for (size_t i = 0; i < argc - 1; i++) {
        std::string str(args[i]);
        if (str == "-i") {
            input_folder = std::string(args[++i]);
        }
        if (str == "-o") {
            output_folder = std::string(args[++i]);
        }
    }
    if (input_folder.empty()) {
        std::cerr << "Input directory is not specified" << std::endl;
        return 1;
    }
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> dirents {};
    if (is_sc2_archive(input_folder)) {
        if (output_folder.empty()) {
            output_folder = input_folder.parent_path();
        }
        dirents.push_back({ input_folder, output_folder / input_folder.filename() });
    } else {
        if (output_folder.empty()) {
            output_folder = input_folder;
        }
        for (const auto& dirent : std::filesystem::directory_iterator(input_folder)) {
            if (dirent.is_directory() && is_sc2_archive(dirent.path())) {
                dirents.push_back({ dirent.path(), output_folder / dirent.path().filename() });
            }
        }
    }
    for (const auto& [dirent, dirent_out] : dirents) {
        std::vector<std::filesystem::path> files {};
        for (const auto& entity : std::filesystem::recursive_directory_iterator(dirent)) {
            if (entity.is_regular_file()) {
                files.push_back(entity.path());
            }
        }
        HANDLE archive_handle;
        std::cout << "Now archiving" << std::endl;
        std::cout << dirent.generic_string() << std::endl;
        std::cout << dirent_out.generic_string() << std::endl;
        std::filesystem::remove(dirent_out);
        check(SFileCreateArchive(dirent_out.c_str(),
                                 MPQ_CREATE_ARCHIVE_V3,
                                 (int32_t)files.size(),
                                 &archive_handle),
              "create archive failed");

        for (const auto& file : files) {
            HANDLE file_handle;
            const auto file_size = std::filesystem::file_size(file);
            const auto file_archive_name = std::filesystem::relative(file, dirent);
            const auto fan = std::regex_replace(file_archive_name.generic_string(),
                                                std::regex("/"), "\\");
            std::cout << file_archive_name << std::endl;
            if (false) {
                check(SFileCreateFile(archive_handle,
                                      fan.c_str(),
                                      std::filesystem::last_write_time(file)
                                          .time_since_epoch()
                                          .count(),
                                      file_size,
                                      LANG_NEUTRAL,
                                      MPQ_FILE_COMPRESS,
                                      &file_handle),
                      "create file failed");
                void* const file_data = std::malloc(file_size);
                {
                    std::ifstream file_stream(file, std::ios::binary | std::ios::in);
                    file_stream.read((char*)file_data, file_size);
                }
                check(SFileWriteFile(file_handle,
                                     file_data,
                                     file_size,
                                     MPQ_COMPRESSION_LZMA),
                      "write file failed");
                check(SFileFinishFile(file_handle), "finish file failed");
                std::free(file_data);
            } else {
                check(SFileAddFileEx(archive_handle, file.c_str(), file_archive_name.c_str(), 0, MPQ_COMPRESSION_LZMA, MPQ_COMPRESSION_NEXT_SAME), "add file ex failed");
            }
        }

        check(SFileCompactArchive(archive_handle, nullptr, false),
              "compact archive failed");

        check(SFileCloseArchive(archive_handle),
              "close archive failed");
    }
}
