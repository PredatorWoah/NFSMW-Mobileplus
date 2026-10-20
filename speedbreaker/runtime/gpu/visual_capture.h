// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <set>
#include <cstdlib>

namespace gpu::visual
{
// Command processor thread only. Keep metadata and binary files within one budget.
struct Capture
{
    static constexpr uint64_t limit = 140ull << 20;
    std::filesystem::path directory;
    uint64_t bytes = 0, frame = 0, finalFrame = 0;
    unsigned taken = 0, steady = 0, spacing = 0, images = 0, draws = 0;
    unsigned resolveIndex = 0, selectedResolve = 0;
    bool active = false, failed = false, budgetLimited = false;
    std::set<uint32_t> depthDestinations, sampled;
    bool enabled() const
    {
#ifdef __ANDROID__
        const char* v = std::getenv("NFSMW_VISUAL_CAPTURE");
        return !v || v[0] != '0';
#else
        const char* v = std::getenv("NFSMW_VISUAL_CAPTURE");
        return v && v[0] == '1';
#endif
    }
    bool write(const std::string& name, const void* data, size_t size, bool append = false)
    {
        const uint64_t cap = (name == "manifest.json" || name == "summary.txt") ? limit + (1ull << 20) : limit;
        if (directory.empty()) return false;
        if (bytes > cap || size > cap - bytes) { budgetLimited = true; return false; }
        std::ofstream out(directory / name, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
        if (!out) { failed = true; return false; }
        out.write(static_cast<const char*>(data), size);
        if (!out) { failed = true; return false; }
        bytes += size;
        return true;
    }
    bool text(const std::string& name, const std::string& s, bool append = false)
    { return write(name, s.data(), s.size(), append); }
    bool arm(uint64_t nextFrame, uint64_t frameDraws, uint64_t passes, uint64_t depthResolves,
             const std::filesystem::path& base)
    {
        if (!enabled() || taken >= 3 || failed) return false;
        // Render-state heuristic: sustained complex 3D plus depth copied for later sampling.
        // No verified guest race-state address exists; a busy 3D menu is a possible false positive.
        bool world = frameDraws >= 1500 && passes >= 10 && depthResolves >= 1;
        steady = world ? steady + 1 : 0;
        if (spacing) { --spacing; return false; }
        if (steady < 30) return false;
        if (directory.empty())
        {
            directory = base / ("visual-" + std::to_string(std::time(nullptr)));
            std::error_code ec;
            std::filesystem::create_directories(directory / "depth-targets", ec);
            if (!ec) std::filesystem::create_directories(directory / "resolved-textures", ec);
            if (ec) { failed = true; return false; }
            text("render-passes.csv", "frame,pass,gpu_us,gap_us,timestamp_status,draws,resolves,uploads,submits,synced_kb,description\n");
            text("summary.txt", "Automatic render-state capture; not a verified Quick Race guest-state detector.\nReadback stalls perturb timestamps. Raw float previews use value*255 without reversal or auto-normalization.\n");
        }
        frame = nextFrame; active = true; images = draws = resolveIndex = 0;
        selectedResolve = taken % unsigned(depthResolves > 4 ? 4 : depthResolves);
        sampled.clear();
        depthDestinations.clear();
        text("manifest.json", "{\"status\":\"recording\",\"schema\":1}\n");
        return true;
    }
    void finish()
    {
        active = false; ++taken; spacing = 60; steady = 0;
        text("manifest.json", "{\"schema\":1,\"status\":\"" + std::string(taken == 3 ? "complete" : "waiting") +
             "\",\"captures\":" + std::to_string(taken) + ",\"last_frame\":" + std::to_string(frame) +
             ",\"last_final_frame\":" + std::to_string(finalFrame) + ",\"bytes_written\":" + std::to_string(bytes) + ",\"budget_limited\":" + (budgetLimited ? "true" : "false") + ",\"io_error\":" + (failed ? "true" : "false") + "}\n");
        if (taken == 3) std::fprintf(stderr, "[visual] complete %s\n", directory.c_str());
    }
};
}
