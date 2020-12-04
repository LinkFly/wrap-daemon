#include <iostream>

using namespace std;

void SetPidFile(char* Filename) {
    FILE* f;
    f = fopen(Filename, "w+");
    if (f) {
        fprintf(f, "%u", getpid());
        fclose(f);
    }
}

int WorkProc() {
    struct sigaction sigact;
    sigset_t sigset;
    int signo;
    int status;

    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = signal_error;

    sigemptyset(&sigact.sa_mask);
    sigaction(SIGFPE, &sigact, 0);
    sigaction(SIGILL, &sigact, 0);
    sigaction(SIGSEGV, &sigact, 0);
    sigaction(SIGBUS, &sigact, 0);

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    SetFdLimit(FD_LIMIT);
    WriteLog("[DAEMON] Started\n");
    status = InitWorkThread();
    if (!status) {
        for(;;) {
            sigwait(&sigset, &signo);
            if (signo == SIGUSR1) {
                status = ReloadConfig();
                if (status == 0) {
                    WriteLog("[DAEMON] Reload config failed\n");
                } else {
                    WriteLog("[DAEMON] Reload config OK\n");
                }
            } else {
                break;
            }
        }
        DestroyWorkThread();
    } else {
        WriteLog("[DAEMON] Create work thread failed\n");
    }
    WriteLog("[DAEMON] Stopped\n");
    return CHILD_NEED_TERMINATE;
}

int MonitorProc() {
    int pid;
    int status;
    int need_start = 1;
    sigset_t sigset;
    siginfo_t siginfo;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    SetPidFile(PID_FILE);
    for(;;) {
        if (need_start) {
            pid = fork();
        }
        need_start = 1;
        if (pid == -1) {
            WriteLog("[MONITOR] Fork failed (%s)\n", strerror(errno));
        } else if (!pid) { // if child
            status = WorkProc();
            exit(status);
        } else {
            sigwaitinfo(&sigset, &siginfo);
            if (siginfo.si_signo == SIGCHLD) {
                wait(&status);
                status = WEXITSTATUS(status);
                if (status == CHILD_NEED_TERMINATE) {
                    WriteLog("[MONITOR] Child stopped\n");
                    break;
                } else if (status == CHILD_NEED_WORK) {
                    WriteLog("[MONITOR] Child restart\n");
                }
            } else if (siginfo.si_signo == SIGUSR1) {
                kill(pid, SIGUSR1);
                need_start = 0;
            } else {
                WriteLog("[MONITOR] Signal %s\n", strsignal(siginfo.si_signo));
                kill(pid, SIGTERM);
                status = 0;
                break;
            }
        }
    }

    WriteLog("[MONITOR] Stop\n");
    unlink(PID_FILE);
    return status;
}
int main(int argc, char** argv)
{
    int status;
    int pid;

    if (argc != 2) {
        cout << "Usage: ./wrap-daemon <program>";
        return -1;
    }

    pid = fork();
    if (pid == -1) {
        cerr << "Error: Start Daemon failed (%s" << strerror(errno) << ")\n" << endl;
        return -1;
    } else if (!pid) {
        umask(0);
        setsid();
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        status = MonitorProc();
        return status;
    } else {
        // this is parent
        return 0;
    }

    return 0;
}
