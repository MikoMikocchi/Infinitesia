#pragma once

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

// A copy of a data file in a fresh temporary directory, to be broken on purpose. Edits go through
// the parsed JSON, so they do not depend on how the file happens to be formatted.
namespace InfinitesiaTests
{
    using Json = nlohmann::json;

    class BrokenFile
    {
    public:
        BrokenFile(const std::filesystem::path& original, const char* name)
            : m_directory(std::filesystem::temp_directory_path() / name)
            , m_path(m_directory / original.filename())
        {
            std::filesystem::remove_all(m_directory);
            std::filesystem::create_directories(m_directory);
            std::filesystem::copy_file(original, m_path);
        }
        ~BrokenFile() { std::filesystem::remove_all(m_directory); }
        BrokenFile(const BrokenFile&) = delete;
        BrokenFile& operator=(const BrokenFile&) = delete;

        const std::filesystem::path& Path() const { return m_path; }

        void Edit(const std::function<void(Json&)>& change) const
        {
            std::ifstream in(m_path);
            Json root = Json::parse(in);
            in.close();
            change(root);
            std::ofstream(m_path, std::ios::trunc) << root.dump(2);
        }

        void WriteText(const std::string& text) const { std::ofstream(m_path, std::ios::trunc) << text; }
        void Remove() const { std::filesystem::remove(m_path); }

        // The record of `array` whose "id" is `key`.
        static Json& Record(Json& root, const char* array, const std::string& key)
        {
            for (Json& record : root.at(array))
            {
                if (record.at("id") == key)
                    return record;
            }
            FAIL("test record not found: " << key);
            return root;
        }

    private:
        std::filesystem::path m_directory;
        std::filesystem::path m_path;
    };
}
