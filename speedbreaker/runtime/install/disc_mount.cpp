// SpeedBreaker. GPL-3.0-or-later.
#include "disc_mount.h"
#include <algorithm>
#include <cctype>
#include <map>

namespace discmount
{
    namespace
    {
        struct Entry { std::filesystem::path path; const install::DiscFile* file = nullptr; };
        std::shared_ptr<install::DiscSource> s_source;
        std::map<std::string, Entry> s_entries;
        std::map<std::string, std::vector<std::filesystem::path>> s_children;
        std::string Key(const std::filesystem::path& path)
        {
            std::string key = path.lexically_normal().generic_string();
            while (key.size() > 1 && key.back() == '/') key.pop_back();
            for (char& c : key) c = char(std::tolower(static_cast<unsigned char>(c)));
            return key;
        }
    }
    const std::filesystem::path& Root()
    {
        static const std::filesystem::path root("/__speedbreaker_disc__");
        return root;
    }
    bool Active() { return bool(s_source); }
    bool Contains(const std::filesystem::path& path)
    {
        if (!Active()) return false;
        std::string key = Key(path), root = Key(Root());
        return key == root || key.starts_with(root + "/");
    }
    bool Mount(std::shared_ptr<install::DiscSource> source, std::string& error)
    {
        if (Active()) { error = "A disc image is already mounted."; return false; }
        if (!source || source->Kind() != install::SourceKind::Image)
        { error = "The runtime mount requires a disc image."; return false; }
        std::map<std::string, Entry> entries;
        entries.emplace(Key(Root()), Entry{Root()});
        for (const auto& file : source->Files())
        {
            std::filesystem::path relative(file.path);
            if (relative.empty() || relative.is_absolute())
            { error = "Invalid disc file path."; return false; }
            for (const auto& part : relative)
                if (part == ".." || part == ".")
                { error = "Invalid disc path component."; return false; }
            auto path = Root() / relative;
            if (!entries.emplace(Key(path), Entry{path, &file}).second)
            { error = "Conflicting disc file paths."; return false; }
            for (auto parent = path.parent_path(); parent != Root(); parent = parent.parent_path())
            {
                auto [it, added] = entries.emplace(Key(parent), Entry{parent});
                if (!added && it->second.file)
                { error = "A disc file conflicts with a directory."; return false; }
            }
        }
        std::map<std::string, std::vector<std::filesystem::path>> children;
        for (const auto& [key, entry] : entries)
            if (key != Key(Root())) children[Key(entry.path.parent_path())].push_back(entry.path);
        s_entries = std::move(entries);
        s_children = std::move(children);
        s_source = std::move(source);
        return true;
    }
    std::optional<std::filesystem::path> Resolve(const std::filesystem::path& path)
    {
        auto it = s_entries.find(Key(path));
        return it == s_entries.end() ? std::nullopt : std::optional(it->second.path);
    }
    std::optional<Attributes> Stat(const std::filesystem::path& path)
    {
        auto it = s_entries.find(Key(path));
        if (it == s_entries.end()) return std::nullopt;
        auto file = it->second.file;
        return Attributes{file ? file->size : 0, !file};
    }
    std::vector<std::filesystem::path> Children(const std::filesystem::path& path)
    {
        auto it = s_children.find(Key(path));
        return it == s_children.end() ? std::vector<std::filesystem::path>{} : it->second;
    }
    std::unique_ptr<install::FileReader> Open(const std::filesystem::path& path, std::string& error)
    {
        auto it = s_entries.find(Key(path));
        if (it == s_entries.end() || !it->second.file)
        { error = "Disc file not found."; return nullptr; }
        return s_source->Open(*it->second.file, error);
    }
}
