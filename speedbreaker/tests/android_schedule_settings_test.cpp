// Real option-table/parser round trip; no game or graphics device.
#include <user/settings.h>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc == 3) {
        assert(settings::Load(argv[1]));
        assert(settings::GetBool(settings::Id::GpuWorkReduction) == (argv[2][0] == '1'));
        return 0;
    }
    assert(argc == 2);
    const std::filesystem::path file = argv[1];
    std::filesystem::create_directories(file.parent_path());
    { std::ofstream out(file); out << "[advanced]\ngpu_scheduling = 'compare'\n"; }
    assert(settings::Load(file));
    assert(settings::Info(settings::Id::GpuWorkReduction).apply == settings::Apply::Restart);
    const bool initial = settings::GetBool(settings::Id::GpuWorkReduction);
    assert(settings::Set(settings::Id::GpuWorkReduction, !initial) == settings::SetResult::Deferred);
    assert(settings::GetBool(settings::Id::GpuWorkReduction) == initial);
    assert(settings::Save(file)); assert(settings::Load(file));
    assert(bool(settings::GetStoredValue(settings::Id::GpuWorkReduction)) == !initial);
    assert(settings::RestartPending(settings::Id::GpuWorkReduction));
    const pid_t child = fork(); assert(child >= 0);
    if (!child) {
        execl(argv[0], argv[0], argv[1], initial ? "0" : "1", nullptr);
        _exit(127);
    }
    int status = 0; assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(settings::GetInt(settings::Id::GpuScheduling) == 4);
    assert(settings::Set(settings::Id::GpuScheduling, 1) == settings::SetResult::Invalid);
    assert(settings::Set(settings::Id::GpuScheduling, 2) == settings::SetResult::Applied);
    assert(settings::Save(file));
    assert(settings::Load(file));
    assert(settings::GetInt(settings::Id::GpuScheduling) == 2);
    setenv("SB_GPU_SCHEDULING", "conservative", 1);
    assert(settings::Load(file));
    assert(settings::GetInt(settings::Id::GpuScheduling) == 0);
    assert(settings::Set(settings::Id::GpuScheduling, 4) == settings::SetResult::Overridden);
    assert(settings::GetInt(settings::Id::GpuScheduling) == 0);
    unsetenv("SB_GPU_SCHEDULING");
    { std::ofstream out(file); out << "[advanced]\ngpu_scheduling = 'invalid'\n"; }
    assert(settings::Load(file));
    assert(settings::GetInt(settings::Id::GpuScheduling) == int(settings::DefaultValue(settings::Id::GpuScheduling)));
}
