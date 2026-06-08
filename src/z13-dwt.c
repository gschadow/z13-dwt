// z13-dwt.c
// Copyright (c) 2026, Gunther Schadow. All rights reserved.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <linux/input.h>
#include <limits.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <libgen.h>

static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int sig) {
    (void)sig;
    stop_requested = 1;
}

static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static int wait_child(pid_t pid) {
    int status;
    for (;;) {
        if (waitpid(pid, &status, 0) >= 0)
            return status;
        if (errno != EINTR)
            die("waitpid");
    }
}

static int debug_enabled(void) {
    const char *debug = getenv("Z13_DWT_DEBUG");
    return debug && *debug && strcmp(debug, "0") != 0;
}

static long long quiet_period_ms(void) {
    const char *value = getenv("Z13_DWT_QUIET_MS");
    char *end = NULL;
    long ms;

    if (!value || !*value)
        return 700;

    errno = 0;
    ms = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || ms < 0 || ms > 10000) {
        fprintf(stderr, "warn: invalid Z13_DWT_QUIET_MS=%s, using 700\n", value);
        return 700;
    }

    return ms;
}

struct kwin_target {
    char user[64];
    char bus[PATH_MAX];
    uid_t uid;
    gid_t gid;
};

static int read_cmd_line(const char *cmd, char *buf, size_t len) {
    FILE *fp = popen(cmd, "r");
    if (!fp)
        return -1;

    if (!fgets(buf, len, fp)) {
        pclose(fp);
        return -1;
    }

    buf[strcspn(buf, "\r\n")] = '\0';
    int status = pclose(fp);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int resolve_kwin_target(struct kwin_target *target) {
    const char *user = getenv("Z13_KWIN_USER");
    const char *bus = getenv("Z13_KWIN_BUS");
    if (user && *user && bus && *bus) {
        snprintf(target->user, sizeof target->user, "%s", user);
        snprintf(target->bus, sizeof target->bus, "%s", bus);
        struct passwd *pw = getpwnam(target->user);
        if (!pw)
            return -1;
        target->uid = pw->pw_uid;
        target->gid = pw->pw_gid;
        return 0;
    }

    char sid[64];
    if (read_cmd_line("loginctl show-seat seat0 -p ActiveSession --value",
                      sid, sizeof sid) < 0 || sid[0] == '\0')
        return -1;

    char cmd[256];
    char active[32], remote[32], state[32], seat[32], type[32], class[32], uid[32];
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p Active --value", sid);
    if (read_cmd_line(cmd, active, sizeof active) < 0 || strcmp(active, "yes") != 0)
        return -1;
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p Remote --value", sid);
    if (read_cmd_line(cmd, remote, sizeof remote) < 0 || strcmp(remote, "no") != 0)
        return -1;
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p State --value", sid);
    if (read_cmd_line(cmd, state, sizeof state) < 0 || strcmp(state, "active") != 0)
        return -1;
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p Seat --value", sid);
    if (read_cmd_line(cmd, seat, sizeof seat) < 0 || strcmp(seat, "seat0") != 0)
        return -1;
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p Type --value", sid);
    if (read_cmd_line(cmd, type, sizeof type) < 0 ||
        (strcmp(type, "wayland") != 0 && strcmp(type, "x11") != 0))
        return -1;
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p Class --value", sid);
    if (read_cmd_line(cmd, class, sizeof class) < 0 || strcmp(class, "user") != 0)
        return -1;
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p Name --value", sid);
    if (read_cmd_line(cmd, target->user, sizeof target->user) < 0)
        return -1;
    snprintf(cmd, sizeof cmd, "loginctl show-session %s -p User --value", sid);
    if (read_cmd_line(cmd, uid, sizeof uid) < 0)
        return -1;

    snprintf(target->bus, sizeof target->bus, "unix:path=/run/user/%s/bus", uid);
    struct passwd *pw = getpwnam(target->user);
    if (!pw)
        return -1;
    target->uid = pw->pw_uid;
    target->gid = pw->pw_gid;

    char sock[PATH_MAX];
    snprintf(sock, sizeof sock, "/run/user/%s/bus", uid);
    if (access(sock, F_OK) != 0)
        return -1;

    if (debug_enabled())
        printf("resolved KWin target session=%s user=%s bus=%s\n", sid, target->user, target->bus);
    return 0;
}

static int build_object_path(const char *touchpad_path, char *object_path, size_t len) {
    char resolved[PATH_MAX];
    const char *source = touchpad_path;

    if (realpath(touchpad_path, resolved) != NULL)
        source = resolved;

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s", source);
    char *leaf = basename(tmp);

    snprintf(object_path, len, "/org/kde/KWin/InputDevice/%s", leaf);
    return 0;
}

static void log_touchpad_selection(const char *touchpad_path, long long quiet_ms) {
    char resolved[PATH_MAX];
    char object_path[PATH_MAX];
    const char *resolved_path = realpath(touchpad_path, resolved) ? resolved : "(unresolved)";

    build_object_path(touchpad_path, object_path, sizeof object_path);
    printf("touchpad path=%s resolved=%s kwin_object=%s quiet_ms=%lld\n",
           touchpad_path, resolved_path, object_path, quiet_ms);
}

static int set_tap_to_click_once(const char *object_path, const struct kwin_target *target, int enabled) {
    const char *value = enabled ? "true" : "false";
    pid_t pid = fork();
    if (pid < 0)
        die("fork");
    if (pid == 0) {
        setenv("DBUS_SESSION_BUS_ADDRESS", target->bus, 1);
        if (initgroups(target->user, target->gid) < 0 ||
            setgid(target->gid) < 0 ||
            setuid(target->uid) < 0)
            _exit(126);
        execlp("busctl", "busctl", "--user", "set-property",
               "org.kde.KWin", object_path, "org.kde.KWin.InputDevice",
               "tapToClick", "b", value, (char *)NULL);
        _exit(127);
    }

    int status = wait_child(pid);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int set_tap_to_click(const char *touchpad_path, struct kwin_target *target, int enabled) {
    char object_path[PATH_MAX];
    build_object_path(touchpad_path, object_path, sizeof object_path);

    const char *value = enabled ? "true" : "false";
    if (target->user[0] && set_tap_to_click_once(object_path, target, enabled) == 0)
        return 0;

    if (target->user[0])
        fprintf(stderr, "warn: cached KWin target failed, refreshing active session\n");

    if (resolve_kwin_target(target) < 0) {
        fprintf(stderr, "failed to resolve active KWin target for tapToClick=%s\n", value);
        return -1;
    }

    if (set_tap_to_click_once(object_path, target, enabled) == 0)
        return 0;

    fprintf(stderr, "failed to set tapToClick=%s on %s for user %s\n",
            value, object_path, target->user);
    return -1;
}

static int open_fd(const char *path) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        fprintf(stderr, "warn: open %s: %s\n", path, strerror(errno));
    return fd;
}

static int touchpad_event_name(const char *touchpad_path, char *buf, size_t len) {
    char resolved[PATH_MAX];
    const char *source = touchpad_path;

    if (realpath(touchpad_path, resolved) != NULL)
        source = resolved;

    const char *leaf = strrchr(source, '/');
    leaf = leaf ? leaf + 1 : source;

    if (strncmp(leaf, "event", 5) != 0)
        return -1;
    if (strlen(leaf) >= len)
        return -1;

    snprintf(buf, len, "%s", leaf);
    return 0;
}

static int find_event_name(const char *line, char *buf, size_t len) {
    const char *event = strstr(line, "event");
    char *end = NULL;
    long number;

    if (!event)
        return -1;

    errno = 0;
    number = strtol(event + 5, &end, 10);
    if (errno != 0 || end == event + 5 || number < 0)
        return -1;

    snprintf(buf, len, "event%ld", number);
    return 0;
}

struct input_block {
    char name[256];
    char handlers[1024];
};

static void reset_input_block(struct input_block *block) {
    block->name[0] = '\0';
    block->handlers[0] = '\0';
}

static void parse_input_line(struct input_block *block, const char *line) {
    if (strncmp(line, "N: Name=", 8) == 0) {
        snprintf(block->name, sizeof block->name, "%s", line + 8);
        block->name[strcspn(block->name, "\r\n")] = '\0';
        if (block->name[0] == '"' && strlen(block->name) > 1) {
            memmove(block->name, block->name + 1, strlen(block->name));
            block->name[strcspn(block->name, "\"")] = '\0';
        }
    } else if (strncmp(line, "H: Handlers=", 12) == 0) {
        snprintf(block->handlers, sizeof block->handlers, "%s", line);
        block->handlers[strcspn(block->handlers, "\r\n")] = '\0';
    }
}

static int block_event_path(const struct input_block *block, char *path, size_t len) {
    char event[64];

    if (find_event_name(block->handlers, event, sizeof event) < 0)
        return -1;

    snprintf(path, len, "/dev/input/%s", event);
    return 0;
}

static int is_z13_touchpad_block(const struct input_block *block) {
    return strstr(block->name, "GZ302EA-Keyboard Touchpad") != NULL ||
           strstr(block->name, "Touchpad") != NULL;
}

static int is_z13_keyboard_block(const struct input_block *block) {
    if (!strstr(block->handlers, "kbd"))
        return 0;

    return strstr(block->name, "GZ302EA-Keyboard") != NULL ||
           strstr(block->name, "N-KEY Device") != NULL;
}

static int discover_touchpad(char *path, size_t len) {
    const char *env_path = getenv("Z13_DWT_TOUCHPAD");
    FILE *fp;
    char line[1024];
    struct input_block block;

    if (env_path && *env_path) {
        snprintf(path, len, "%s", env_path);
        printf("touchpad candidate path=%s source=env\n", path);
        return 0;
    }

    fp = fopen("/proc/bus/input/devices", "r");
    if (!fp)
        die("open /proc/bus/input/devices");

    reset_input_block(&block);
    while (fgets(line, sizeof line, fp)) {
        if (line[0] == '\n') {
            if (is_z13_touchpad_block(&block) && block_event_path(&block, path, len) == 0) {
                printf("touchpad candidate path=%s name=\"%s\" source=auto\n", path, block.name);
                fclose(fp);
                return 0;
            }
            reset_input_block(&block);
            continue;
        }
        parse_input_line(&block, line);
    }

    if (is_z13_touchpad_block(&block) && block_event_path(&block, path, len) == 0) {
        printf("touchpad candidate path=%s name=\"%s\" source=auto\n", path, block.name);
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return -1;
}

static int discover_keyboards(const char *touchpad_path, char paths[][PATH_MAX], int max_paths) {
    char skip[64] = {0};
    FILE *fp;
    char line[1024];
    int count = 0;
    struct input_block block;

    if (touchpad_event_name(touchpad_path, skip, sizeof skip) < 0) {
        fprintf(stderr, "warn: could not resolve touchpad event name for %s\n", touchpad_path);
        skip[0] = '\0';
    }

    fp = fopen("/proc/bus/input/devices", "r");
    if (!fp)
        die("open /proc/bus/input/devices");

    reset_input_block(&block);
    while (fgets(line, sizeof line, fp) && count < max_paths) {
        if (line[0] == '\n') {
            char event[64];

            if (is_z13_keyboard_block(&block) &&
                find_event_name(block.handlers, event, sizeof event) == 0 &&
                (!skip[0] || strcmp(event, skip) != 0)) {
                snprintf(paths[count], PATH_MAX, "/dev/input/%s", event);
                printf("keyboard candidate path=%s name=\"%s\" source=auto\n",
                       paths[count], block.name);
                count++;
            }
            reset_input_block(&block);
            continue;
        }
        parse_input_line(&block, line);
    }

    if (count < max_paths) {
        char event[64];

        if (is_z13_keyboard_block(&block) &&
            find_event_name(block.handlers, event, sizeof event) == 0 &&
            (!skip[0] || strcmp(event, skip) != 0)) {
            snprintf(paths[count], PATH_MAX, "/dev/input/%s", event);
            printf("keyboard candidate path=%s name=\"%s\" source=auto\n",
                   paths[count], block.name);
            count++;
        }
    }

    fclose(fp);
    return count;
}

static inline int is_modifier_key(__u16 code) {
    return (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT ||
            code == KEY_LEFTCTRL  || code == KEY_RIGHTCTRL  ||
            code == KEY_LEFTALT   || code == KEY_RIGHTALT);
}

int main(int argc, char **argv) {
    if (argc > 1 && argc < 3) {
        fprintf(stderr,
                "usage: %s [<touchpad-event> <keyboard-event> [keyboard-event ...]]\n",
                argv[0]);
        return 2;
    }

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    setvbuf(stdout, NULL, _IOLBF, 0);

    char auto_touchpad_path[PATH_MAX];
    const char *touchpad_path = NULL;
    const long long quiet_ms = quiet_period_ms();
    struct kwin_target kwin = {0};
    int kfds[16];
    int nfds = 0;
    char discovered[16][PATH_MAX];
    const char *keyboard_paths[16];
    int keyboard_count = 0;

    if (argc >= 3) {
        touchpad_path = argv[1];
        for (int i = 2; i < argc && keyboard_count < (int)(sizeof keyboard_paths / sizeof keyboard_paths[0]); i++) {
            keyboard_paths[keyboard_count++] = argv[i];
            printf("keyboard candidate path=%s source=argv\n", argv[i]);
        }
    } else {
        if (discover_touchpad(auto_touchpad_path, sizeof auto_touchpad_path) < 0) {
            fprintf(stderr, "failed to auto-detect touchpad; set Z13_DWT_TOUCHPAD or pass devices explicitly\n");
            return 1;
        }
        touchpad_path = auto_touchpad_path;
        keyboard_count = discover_keyboards(touchpad_path, discovered,
                                            (int)(sizeof discovered / sizeof discovered[0]));
        for (int i = 0; i < keyboard_count; i++)
            keyboard_paths[i] = discovered[i];
    }

    log_touchpad_selection(touchpad_path, quiet_ms);

    for (int i = 0; i < keyboard_count; i++) {
        int fd = open_fd(keyboard_paths[i]);
        if (fd >= 0) {
            printf("keyboard opened path=%s fd=%d\n", keyboard_paths[i], fd);
            kfds[nfds++] = fd;
        }
    }
    if (nfds == 0)
        die("open keyboards");

    struct pollfd fds[16];
    for (int i = 0; i < nfds; i++) {
        fds[i].fd = kfds[i];
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    long long reenable_at_ms = 0;
    int tap_disabled = 0;

    for (;;) {
        if (stop_requested)
            break;

        int timeout = -1;
        if (tap_disabled) {
            long long remaining = reenable_at_ms - now_ms();
            timeout = remaining <= 0 ? 0 : (remaining > 2147483647LL ? 2147483647 : (int)remaining);
        }

        int rc = poll(fds, nfds, timeout);
        if (rc < 0) {
            if (errno == EINTR) continue;
            die("poll");
        }

        if (rc == 0 && tap_disabled) {
            set_tap_to_click(touchpad_path, &kwin, 1);
            tap_disabled = 0;
            if (debug_enabled())
                printf("tap-to-click enabled\n");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN))
                continue;

            struct input_event ev;
            ssize_t n;
            while ((n = read(fds[i].fd, &ev, sizeof ev)) > 0) {
                if (n != sizeof ev)
                    continue;
		if (is_modifier_key(ev.code))
		  continue;		
                if (ev.type == EV_KEY && ev.value != 0) {
                    reenable_at_ms = now_ms() + quiet_ms;
                    if (!tap_disabled) {
                        if (set_tap_to_click(touchpad_path, &kwin, 0) < 0)
                            continue;
                        tap_disabled = 1;
                        if (debug_enabled())
                            printf("key code=%u => tap-to-click disabled\n", ev.code);
                    }
                }
            }
            if (n < 0 && errno != EAGAIN)
                fprintf(stderr, "read keyboard: %s\n", strerror(errno));
        }
        for (int i = 0; i < nfds; i++)
            fds[i].revents = 0;
    }

    if (tap_disabled)
        set_tap_to_click(touchpad_path, &kwin, 1);

    for (int i = 0; i < nfds; i++)
        close(kfds[i]);

    return 0;
}
