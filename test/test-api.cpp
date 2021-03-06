#include <libporto.hpp>

#include <signal.h>
#include <cassert>

#define Expect(a)   assert(a)
#define ExpectEq(a, b)   assert((a) == (b))
#define ExpectNeq(a, b)   assert((a) != (b))
#define ExpectSuccess(ret) assert((ret) == Porto::EError::Success)

int main(int, char **) {
    std::vector<Porto::TString> list;
    Porto::TString str, path;
    uint64_t val;

    signal(SIGPIPE, SIG_IGN);

    Porto::TPortoApi api;

    Expect(!api.Connected());
    Expect(api.GetFd() < 0);

    // Connect
    ExpectSuccess(api.Connect());

    Expect(api.Connected());
    Expect(api.GetFd() >= 0);

    // Disconnect
    api.Disconnect();

    Expect(!api.Connected());
    Expect(api.GetFd() < 0);

    // Auto connect
    ExpectSuccess(api.GetVersion(str, str));
    Expect(api.Connected());

    // Auto reconnect
    ExpectEq(system("./portod reload"), 0);
    ExpectSuccess(api.GetVersion(str, str));
    Expect(api.Connected());

    // No auto reconnect
    api.Disconnect();
    api.SetAutoReconnect(false);
    ExpectEq(api.GetVersion(str, str), Porto::EError::SocketError);
    api.SetAutoReconnect(true);

    val = api.GetTimeout();
    ExpectNeq(val, 0);
    ExpectSuccess(api.SetTimeout(5));

    ExpectSuccess(api.List(list));

    ExpectSuccess(api.ListProperties(list));

    ExpectSuccess(api.ListVolumes(list));

    ExpectSuccess(api.ListVolumeProperties(list));

    ExpectSuccess(api.ListLayers(list));

    ExpectSuccess(api.ListStorages(list));

    ExpectSuccess(api.Call("Version {}", str));

    ExpectSuccess(api.GetProperty("/", "state", str));
    ExpectEq(str, "meta");

    ExpectSuccess(api.GetInt("/", "state", val));
    ExpectEq(val, Porto::META);

    ExpectSuccess(api.GetProperty("/", "controllers", "memory", str));
    ExpectEq(str, "true");

    ExpectSuccess(api.GetInt("/", "controllers", "memory", val));
    ExpectEq(val, 1);

    ExpectSuccess(api.GetProperty("/", "memory_usage", str));
    ExpectNeq(str, "0");

    val = 0;
    ExpectSuccess(api.GetInt("/", "memory_usage", val));
    ExpectNeq(val, 0);

    auto ct = api.GetContainer("/");
    Expect(ct != nullptr);
    ExpectEq(ct->name(), "/");

    ExpectEq(api.GetInt("/", "__wrong__", val), Porto::EError::InvalidProperty);
    ExpectEq(api.Error(), Porto::EError::InvalidProperty);
    ExpectEq(api.GetLastError(str), Porto::EError::InvalidProperty);

    ct = api.GetContainer("a");
    Expect(ct == nullptr);
    ExpectEq(api.Error(), Porto::EError::ContainerDoesNotExist);

    ExpectSuccess(api.Create("a"));

    ExpectSuccess(api.SetProperty("a", "memory_limit", "2M"));
    ExpectSuccess(api.GetProperty("a", "memory_limit", str));
    ExpectEq(str, "2097152");

    ExpectSuccess(api.SetInt("a", "memory_limit", 1<<20));
    ExpectSuccess(api.GetInt("a", "memory_limit", val));
    ExpectEq(val, 1048576);

    ExpectSuccess(api.SetLabel("a", "TEST.a", "."));

    ct = api.GetContainer("a");
    Expect(ct != nullptr);
    ExpectEq(ct->st(), Porto::EContainerState::STOPPED);
    ExpectEq(ct->state(), "stopped");
    ExpectEq(ct->memory_limit(), 1 << 20);

    ExpectSuccess(api.WaitContainer("a", str));
    ExpectEq(str, "stopped");

    ExpectSuccess(api.CreateVolume(path, {
                {"containers", "a"},
                {"backend", "native"},
                {"space_limit", "1G"}}));
    ExpectNeq(path, "");

    auto vd = api.GetVolumeDesc(path);
    Expect(vd != nullptr);
    ExpectEq(vd->path(), path);

    auto vs = api.GetVolume(path);
    Expect(vs != nullptr);
    ExpectEq(vs->path(), path);

    ExpectSuccess(api.SetVolumeLabel(path, "TEST.a", "."));

    ExpectSuccess(api.SetProperty("a", "command", "sleep 1000"));
    ExpectSuccess(api.Start("a"));

    ExpectSuccess(api.GetProperty("a", "state", str));
    ExpectEq(str, "running");

    ExpectSuccess(api.Destroy("a"));

    api.Disconnect();

    return 0;
}
