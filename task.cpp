#include <climits>
#include <sstream>
#include <iterator>
#include <csignal>

#include "porto.hpp"
#include "task.hpp"
#include "cgroup.hpp"
#include "log.hpp"
#include "util/string.hpp"
#include "util/unix.hpp"

extern "C" {
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include <wordexp.h>
}

using namespace std;

// TTaskEnv
TError TTaskEnv::Prepare() {
    if (Command.empty())
        return TError::Success();

    string workdir;
    if (Cwd.length())
        workdir = Cwd;
    else
        workdir = "/home/" + User;

    env.push_back("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:" + workdir);

    if (SplitString(Envir, ';', env)) {
        TError error(EError::InvalidValue, errno, "split(" + Envir + ")");
        return error;
    }

    env.push_back("HOME=" + workdir);
    env.push_back("USER=" + User);

    struct passwd *p = getpwnam(User.c_str());
    if (!p) {
        TError error(EError::InvalidValue, EINVAL, "getpwnam(" + User + ")");
        return error;
    } else {
        Uid = p->pw_uid;
    }

    struct group *g = getgrnam(Group.c_str());
    if (!g) {
        TError error(EError::InvalidValue, EINVAL, "getgrnam(" + Group + ")");
        return error;
    } else {
        Gid = g->gr_gid;
    }

    return TError::Success();
}

const char** TTaskEnv::GetEnvp() {
    auto envp = new const char* [env.size() + 1];
    for (size_t i = 0; i < env.size(); i++)
        envp[i] = env[i].c_str();
    envp[env.size()] = NULL;

    return envp;
}

// TTask
int TTask::CloseAllFds(int except) {
    for (int i = 0; i < getdtablesize(); i++)
        if (i != except)
            close(i);

    return except;
}

void TTask::ReportResultAndExit(int fd, int result)
{
    if (write(fd, &result, sizeof(result))) {}
    exit(EXIT_FAILURE);
}

void TTask::Syslog(const string &s)
{
    openlog("portod", LOG_NDELAY, LOG_DAEMON);
    syslog(LOG_ERR, "%s", s.c_str());
    closelog();
}

TTask::~TTask() {
    if (stdoutFile.length()) {
        TFile f(stdoutFile);
        TError e = f.Remove();
        TLogger::LogError(e, "Can't remove task stdout " + stdoutFile);
    }
    if (stderrFile.length()) {
        TFile f(stderrFile);
        TError e = f.Remove();
        TLogger::LogError(e, "Can't remove task stderr " + stdoutFile);
    }
}

static string GetTmpFile() {
    char p[] = "/tmp/XXXXXX";
    int fd = mkstemp(p);
    if (fd < 0)
        return "";

    string path(p);
    close(fd);
    return path;
}

static int child_fn(void *arg) {
    TTask *task = static_cast<TTask*>(arg);
    return task->ChildCallback();
}

int TTask::ChildCallback() {
    close(rfd);
    ResetAllSignalHandlers();

    /*
     * ReportResultAndExit(fd, -errno) means we failed while preparing
     * to execve, which should never happen (but it will :-)
     *
     * ReportResultAndExit(fd, +errno) means execve failed
     */

    if (prctl(PR_SET_KEEPCAPS, 0, 0, 0, 0) < 0) {
        Syslog(string("prctl(PR_SET_KEEPCAPS): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    if (setsid() < 0) {
        Syslog(string("setsid(): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    // remount proc so PID namespace works
    TMount proc("proc", "/proc", "proc", {});
    if (proc.Remount()) {
        Syslog(string("remount procfs: ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    // move to target cgroups
    for (auto cg : leaf_cgroups) {
        auto error = cg->Attach(getpid());
        if (error) {
            Syslog(string("cgroup attach: ") + error.GetMsg());
            ReportResultAndExit(wfd, -error.GetError());
        }
    }

    wfd = CloseAllFds(wfd);
    if (wfd < 0) {
        Syslog(string("close fds: ") + strerror(errno));
        /* there is no way of telling parent that we failed (because we
         * screwed up fds), so exit with some eye catching error code */
        exit(0xAA);
    }

    int ret = open("/dev/null", O_RDONLY);
    if (ret < 0) {
        Syslog(string("open(0): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    ret = open(stdoutFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0700);
    if (ret < 0) {
        Syslog(string("open(1): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    ret = fchown(ret, env.Uid, env.Gid);
    if (ret < 0) {
        Syslog(string("fchown(1): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    ret = open(stderrFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0700);
    if (ret < 0) {
        Syslog(string("open(2): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    ret = fchown(ret, env.Uid, env.Gid);
    if (ret < 0) {
        Syslog(string("fchown(2): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    TMount new_root(env.Root, env.Root + "/", "none", {});
    TMount new_proc("proc", env.Root + "/proc", "proc", {});
    TMount new_sys("/sys", env.Root + "/sys", "none", {});
    TMount new_dev("/dev", env.Root + "/dev", "none", {});
    TMount new_var("/var", env.Root + "/var", "none", {});
    TMount new_run("/run", env.Root + "/run", "none", {});
    TMount new_tmp("/tmp", env.Root + "/tmp", "none", {});

    if (env.Root.length()) {
        if (new_root.Bind()) {
            Syslog(string("remount /: ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (new_tmp.Bind()) {
            Syslog(string("remount /tmp: ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (new_sys.Bind()) {
            Syslog(string("remount /sys: ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (new_run.Bind()) {
            Syslog(string("remount /run: ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (new_dev.Bind()) {
            Syslog(string("remount /dev: ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (new_var.Bind()) {
            Syslog(string("remount /var: ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (new_proc.Mount()) {
            Syslog(string("remount /proc: ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (chdir(env.Root.c_str()) < 0) {
            Syslog(string("chdir(): ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (chroot(env.Root.c_str()) < 0) {
            Syslog(string("chroot(): ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

        if (chdir("/") < 0) {
            Syslog(string("chdir(): ") + strerror(errno));
            ReportResultAndExit(wfd, -errno);
        }

    }

    if (env.Cwd.length() && chdir(env.Cwd.c_str()) < 0) {
        Syslog(string("chdir(): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    // drop privileges
    if (setgid(env.Gid) < 0) {
        Syslog(string("setgid(): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    if (initgroups(env.User.c_str(), env.Gid) < 0) {
        Syslog(string("initgroups(): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    if (setuid(env.Uid) < 0) {
        Syslog(string("setuid(): ") + strerror(errno));
        ReportResultAndExit(wfd, -errno);
    }

    umask(0);
    clearenv();

	wordexp_t result;

	ret = wordexp(env.Command.c_str(), &result, WRDE_NOCMD | WRDE_UNDEF);
    switch (ret) {
    case WRDE_BADCHAR:
        Syslog(string("wordexp(): illegal occurrence of newline or one of |, &, ;, <, >, (, ), {, }"));
        ReportResultAndExit(wfd, -EINVAL);
    case WRDE_BADVAL:
        Syslog(string("wordexp(): undefined shell variable was referenced"));
        ReportResultAndExit(wfd, -EINVAL);
    case WRDE_CMDSUB:
        Syslog(string("wordexp(): command substitution is not supported"));
        ReportResultAndExit(wfd, -EINVAL);
    case WRDE_SYNTAX:
        Syslog(string("wordexp(): syntax error"));
        ReportResultAndExit(wfd, -EINVAL);
    default:
    case WRDE_NOSPACE:
        Syslog(string("wordexp(): error ") + strerror(ret));
        ReportResultAndExit(wfd, -EINVAL);
    case 0:
        break;
    }

#ifdef __DEBUG__
    Syslog(env.Command.c_str());
    for (unsigned i = 0; i < result.we_wordc; i++)
        Syslog(result.we_wordv[i]);
#endif

    auto envp = env.GetEnvp();
    execvpe(result.we_wordv[0], (char *const *)result.we_wordv, (char *const *)envp);

    Syslog(string("execvpe(): ") + strerror(errno));
    ReportResultAndExit(wfd, errno);

    return 0;
}

TError TTask::Start() {
    int ret;
    int pfd[2];

    exitStatus.Error = 0;
    exitStatus.Status = 0;

    if (env.Cwd.length()) {
        stdoutFile = env.Cwd + "/stdout";
        stderrFile = env.Cwd + "/stderr";
    } else {
        stdoutFile = GetTmpFile();
        stderrFile = GetTmpFile();
    }

    ret = pipe2(pfd, O_CLOEXEC);
    if (ret) {
        TError error(EError::Unknown, errno, "pipe2(pdf)");
        TLogger::LogError(error, "Can't create communication pipe for child");
        return error;
    }

    rfd = pfd[0];
    wfd = pfd[1];

    pid_t fork_pid = fork();
    if (fork_pid < 0) {
        TError error(EError::Unknown, errno, "fork()");
        TLogger::LogError(error, "Can't spawn child");
        return error;
    } else if (fork_pid == 0) {
        char stack[8192];

        (void)setsid();

        pid_t clone_pid = clone(child_fn, stack + sizeof(stack),
                                SIGCHLD | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS,
                                this);
        if (write(wfd, &clone_pid, sizeof(clone_pid))) {}
        if (clone_pid < 0) {
            TError error(EError::Unknown, errno, "clone()");
            TLogger::LogError(error, "Can't spawn child");
            return error;
        }
        exit(EXIT_SUCCESS);
    }
    (void)waitpid(fork_pid, NULL, 0);

    close(wfd);
    int n = read(rfd, &pid, sizeof(pid));
    if (n <= 0) {
        TError error(EError::Unknown, errno, "read(rfd)");
        TLogger::LogError(error, "Can't read pid from the child");
        return error;
    }

    n = read(rfd, &ret, sizeof(ret));
    close(rfd);
    if (n < 0) {
        pid = 0;
        TError error(EError::Unknown, errno, "read(rfd)");
        TLogger::LogError(error, "Can't read result from the child");
        return error;
    } else if (n == 0) {
        state = Started;
        return TError::Success();
    } else {
        pid = 0;

        exitStatus.Error = ret;
        exitStatus.Status = -1;

        TError error;
        if (ret < 0)
            error = TError(EError::Unknown, string("child prepare: ") + strerror(-ret));
        else
            error = TError(EError::Unknown, string("child exec: ") + strerror(ret));
        TLogger::LogError(error, "Child process couldn't exec");
        return error;
    }
}

int TTask::GetPid() {
    return pid;
}

bool TTask::IsRunning() {
    return state == Started;
}

TExitStatus TTask::GetExitStatus() {
    return exitStatus;
}

void TTask::DeliverExitStatus(int status) {
    exitStatus.Error = 0;
    exitStatus.Status = status;
    state = Stopped;
}

void TTask::Kill(int signal) {
    if (!pid)
        throw "Tried to kill invalid process!";

    TLogger::Log("kill " + to_string(pid));

    int ret = kill(pid, signal);
    if (ret != 0) {
        TError error(EError::Unknown, errno, "kill(" + to_string(pid) + ")");
        TLogger::LogError(error, "Can't kill child process");
    }
}

std::string TTask::GetStdout() {
    string s;
    TFile f(stdoutFile);
    TError e(f.LastStrings(STDOUT_READ_BYTES, s));
    TLogger::LogError(e, "Can't read container stdout");
    return s;
}

std::string TTask::GetStderr() {
    string s;
    TFile f(stderrFile);
    TError e(f.LastStrings(STDOUT_READ_BYTES, s));
    TLogger::LogError(e, "Can't read container stderr");
    return s;
}

TError TTask::Restore(int pid_) {
    exitStatus.Error = 0;
    exitStatus.Status = 0;

    // There are to possibilities here:
    // 1. We died and loop reaped container, so it will deliver
    // exit_status later;
    // 2. In previous session we died right after we reaped exit_status
    // but didn't change persistent store.
    //
    // Thus, we need to be in Started state so we can possibly receive
    // exit_status from (1); if it was really case (2) we will indicate
    // error when user tries to get task state in Reap() from waitpit().
    //
    // Moreover, if task didn't die, but we are restoring, it can go
    // away under us any time, so don't fail if we can't recover
    // something.

    TFile stdoutLink("/proc/" + to_string(pid_) + "/fd/1");
    TError error = stdoutLink.ReadLink(stdoutFile);
    if (error)
        stdoutFile = env.Cwd + "/stdout";
    TLogger::LogError(error, "Restore stdout");

    TFile stderrLink("/proc/" + to_string(pid_) + "/fd/2");
    error = stderrLink.ReadLink(stderrFile);
    if (error)
        stderrFile = env.Cwd + "/stderr";
    TLogger::LogError(error, "Restore stderr");

    pid = pid_;
    state = Started;

    error = ValidateCgroups();
    TLogger::LogError(error, "Can't validate cgroups");

    return TError::Success();
}

TError TTask::ValidateCgroups() {
    TFile f("/proc/" + to_string(pid) + "/cgroup");

    vector<string> lines;
    map<string, string> cgmap;
    TError error = f.AsLines(lines);
    if (error)
        return error;

    vector<string> tokens;
    for (auto l : lines) {
        tokens.clear();
        error = SplitString(l, ':', tokens);
        if (error)
            return error;

        const string &subsys = tokens[1];
        const string &path = tokens[2];

        bool valid = false;
        for (auto cg : leaf_cgroups) {
            if (cg->Relpath() == path) {
                valid = true;
                break;
            }
        }

        if (!valid)
            return TError(EError::Unknown, "Task belongs to invalid subsystem " + subsys + ":" + path);
    }

    return TError::Success();
}

TError TTask::RotateFile(const std::string path) {
    struct stat st;

    if (stat(path.c_str(), &st) < 0)
        return TError(EError::Unknown, errno, "stat(" + path + ")");

    if (st.st_size > CONTAINER_MAX_LOG_SIZE)
        if (truncate(path.c_str(), 0) < 0)
            return TError(EError::Unknown, errno, "truncate(" + path + ")");

    return TError::Success();
}

TError TTask::Rotate() {
    TError error;

    error = RotateFile(stdoutFile);
    if (error)
        return error;

    error = RotateFile(stderrFile);
    if (error)
        return error;

    return TError::Success();
}
