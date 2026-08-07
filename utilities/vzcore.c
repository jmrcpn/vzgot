#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pty.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <linux/sched.h>

#define PRG "vzgot_core"

#define SYNC_DEV_READY 'R'
#define SYNC_PTY_DONE  'D'
#define SYNC_ERR       'X'

struct container_config {
    int pipe_m2c[2];
    int pipe_c2m[2];
    const char *rootfs;
};

static char read_sync_pipe(int fd) {
    char chr = '\0';
    if (read(fd, &chr, 1) == 1) return chr;
    return SYNC_ERR;
}

static void write_sync_pipe(int fd, char signal) {
    (void)write(fd, &signal, 1);
}

static _Bool do_sysmount(const char *rootfs) {
    char ppath[PATH_MAX];
    int mntflags;

    snprintf(ppath, sizeof(ppath), "%s/proc", rootfs);
    (void)mkdir(ppath, 0755);
    mntflags = MS_NOSUID | MS_NODEV | MS_NOEXEC;
    if (mount("proc", ppath, "proc", mntflags, NULL) < 0) return false;

    snprintf(ppath, sizeof(ppath), "%s/sys", rootfs);
    (void)mkdir(ppath, 0755);
    mntflags = MS_NOSUID | MS_NODEV | MS_NOEXEC ;
    if (mount("sysfs", ppath, "sysfs", mntflags, NULL) < 0) return false;

    snprintf(ppath, sizeof(ppath), "%s/dev", rootfs);
    (void)mkdir(ppath, 0755);
    mntflags = MS_NOSUID | MS_STRICTATIME;
    /* Note : On garde tmpfs ici pour /dev, systemd y montera ses propres nodes */
    if (mount("tmpfs", ppath, "tmpfs", mntflags, "mode=755") < 0) return false;

    if (mount(NULL, ppath, NULL, MS_PRIVATE, NULL) < 0) return false;

    snprintf(ppath, sizeof(ppath), "%s/dev/pts", rootfs);
    (void)mkdir(ppath, 0755);
    if (mount("devpts", ppath, "devpts", 0, "newinstance,ptmxmode=0666") < 0) return false;

    struct { const char *name; int mode; int maj; int min; } devs[] = {
        {"null", 0666, 1, 3},
        {"zero", 0666, 1, 5},
        {"random", 0666, 1, 8},
        {"urandom", 0666, 1, 9}
    };
    for (size_t i = 0; i < sizeof(devs)/sizeof(devs[0]); i++) {
        snprintf(ppath, sizeof(ppath), "%s/dev/%s", rootfs, devs[i].name);
        (void)unlink(ppath);
        if (mknod(ppath, S_IFCHR | devs[i].mode, makedev(devs[i].maj, devs[i].min)) < 0) return false;
    }

    return true;
}

static int child_exec(void *arg) {
    struct container_config *config = (struct container_config *)arg;
    char path_putold[PATH_MAX];
    char slave_pty[PATH_MAX];
    int idx = 0;
    char c;

    /* Fermeture des extrémités inutilisées des tubes */
    close(config->pipe_m2c[1]);
    close(config->pipe_c2m[0]);
    
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) return 1;
    if (!do_sysmount(config->rootfs)) return 1;

    /* 1. On signale au Maître que le VFS de base est prêt */
    write_sync_pipe(config->pipe_c2m[1], SYNC_DEV_READY);

    /* 2. Récupération du chemin absolu du PTY esclave depuis le Maître */
    while (read(config->pipe_m2c[0], &c, 1) == 1 && c != '\n' && idx < PATH_MAX - 1) {
        slave_pty[idx++] = c;
    }
    slave_pty[idx] = '\0';

    /* 3. OUVERTURE IN-FLIGHT DU PTY AVANT LE PIVOT ROOT 
          Indispensable pour contourner les restrictions MS_NODEV sur le /dev local */
    int fd_tty = open(slave_pty, O_RDWR | O_NOCTTY);
    if (fd_tty < 0) {
        write_sync_pipe(config->pipe_c2m[1], SYNC_ERR);
        return 1;
    }

    /* === LE FIX : ON APPORTE LE TERMINAL DANS LE /DEV DU CONTENEUR ICI === */
    char target_console[PATH_MAX];
    snprintf(target_console, sizeof(target_console), "%s/dev/console", config->rootfs);

    /* On crée le point d'ancrage vide (un fichier régulier obligatoire pour le bind mount) */
    int shadow_fd = open(target_console, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if (shadow_fd >= 0) {
        close(shadow_fd);
        /* On bind-mounte le chemin réel de l'hôte sur le futur /dev/console du conteneur */
        (void)mount(slave_pty, target_console, NULL, MS_BIND, NULL);
    }

    /* On valide l'étape auprès du Maître */
    write_sync_pipe(config->pipe_c2m[1], SYNC_PTY_DONE);

    /* 4. ISOLEMENT VIA PIVOT_ROOT */
    snprintf(path_putold, sizeof(path_putold), "%s/.oldroot", config->rootfs);
    (void)mkdir(path_putold, 0700);

    /* On valide l'étape auprès du Maître */
    write_sync_pipe(config->pipe_c2m[1], SYNC_PTY_DONE);

    /* 4. ISOLEMENT VIA PIVOT_ROOT */
    snprintf(path_putold, sizeof(path_putold), "%s/.oldroot", config->rootfs);
    (void)mkdir(path_putold, 0700);

    if (syscall(SYS_pivot_root, config->rootfs, path_putold) < 0) return 1;
    if (chdir("/") < 0) return 1;
    (void)umount2("/.oldroot", MNT_DETACH);
    (void)rmdir("/.oldroot");

    close(config->pipe_m2c[0]);
    close(config->pipe_c2m[1]);

    /* 5. ACQUISITION DU TERMINAL DE CONTRÔLE INTERNE POUR SYSTEMD */

    /* On force la création d'une nouvelle session dont l'init est le leader */
    (void)setsid();

    /* Le coup de grâce : on ré-ouvre /dev/console MAINTENANT qu'il est bind-monté
       à l'intérieur de la nouvelle racine du conteneur. Le fait de l'ouvrir
       en tant que leader de session SANS le drapeau O_NOCTTY force le noyau
       Linux à l'attribuer automatiquement comme terminal de contrôle (ctty) ! */
    int fd_console = open("/dev/console", O_RDWR);
    if (fd_console >= 0) {
        /* On écrase les flux hérités par ce descripteur parfaitement légitime */
        (void)dup2(fd_console, STDIN_FILENO);
        (void)dup2(fd_console, STDOUT_FILENO);
        if (fd_console > 2) close(fd_console);
    } else {
        /* Repli sur le descripteur volant en cas de problème */
        (void)dup2(fd_tty, STDIN_FILENO);
        (void)dup2(fd_tty, STDOUT_FILENO);
        (void)dup2(fd_tty, STDERR_FILENO);
    }
    if (fd_tty > 2) close(fd_tty);

    /* === VOTRE MESSAGE DE SIGNON EN DIRECT DANS LA CONSOLE === */
    const char *signon = "\n"
                         "=========================================\n"
                         "  VZGOT Container Infrastructure Active  \n"
                         "  Console log stream redirection: OK     \n"
                         "  Try 1				   \n"
                         "=========================================\n\n";
    (void)write(STDOUT_FILENO, signon, strlen(signon));


    /* Lancement de l'Init ou de Bash (Pour vos tests de faisabilité systemd) */
#ifdef	SIMPLE
    char *exec_args[] = {"/bin/bash", "-m", NULL};
    char *exec_env[] = {"TERM=xterm", "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "HOME=/root", NULL};
#else
    char *exec_args[] = {"/lib/systemd/systemd","--log-target=console","--log-level=debug",NULL};
    char *exec_env[] = {
        "TERM=xterm",
        "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
        "container=vzgot", /* Indispensable pour que systemd sache qu'il est en conteneur */
	"SYSTEMD_LOG_LEVEL=debug",   // Force le niveau debug au plus bas niveau
        "SYSTEMD_LOG_TARGET=console", // Force l'écriture brute sur le descripteur 2 (stderr)
        "SYSTEMD_LOG_COLOR=no",
        NULL
    };
#endif
    
    execve(exec_args[0], exec_args, exec_env);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <chemin_absolu_rootfs>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct container_config config;
    config.rootfs = argv[1];

    int master_fd, slave_fd;
    char slave_name[PATH_MAX];

    /* Allocation du PTY matériel */
    if (openpty(&master_fd, &slave_fd, slave_name, NULL, NULL) < 0) {
        perror(PRG ": openpty a échoué");
        return EXIT_FAILURE;
    }
    close(slave_fd); /* L'enfant ouvrira le périphérique via son chemin slave_name */

    if (pipe(config.pipe_m2c) < 0 || pipe(config.pipe_c2m) < 0) {
        perror(PRG ": Échec de création des tubes de synchro");
        close(master_fd);
        return EXIT_FAILURE;
    }

    if (mount(config.rootfs, config.rootfs, NULL, MS_BIND | MS_REC, NULL) < 0) {
        perror(PRG ": Impossible de binder le rootfs");
        close(master_fd);
        return EXIT_FAILURE;
    }
    if (mount(NULL, config.rootfs, NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        perror(PRG ": Impossible de passer le rootfs en MS_PRIVATE");
        close(master_fd);
        return EXIT_FAILURE;
    }

    struct clone_args cl_args = {0};
    cl_args.flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWCGROUP;
    cl_args.exit_signal = SIGCHLD;

    pid_t child_pid = syscall(SYS_clone3, &cl_args, sizeof(cl_args));
    if (child_pid < 0) {
        perror(PRG ": Erreur fatale sur clone3");
        close(master_fd);
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        exit(child_exec(&config));
    }

    /* --- LOGIQUE DU MAÎTRE SUPERVISEUR --- */
    close(config.pipe_m2c[0]);
    close(config.pipe_c2m[1]);

    /* Ouverture immédiate du fichier de destination sur l'hôte */
    int log_fd = open("/vzgot/test/dummy3/console", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror(PRG ": Impossible de créer ou d'ouvrir le fichier de log de la console");
        kill(child_pid, SIGKILL);
        close(master_fd);
        return EXIT_FAILURE;
    }

    printf(PRG ": Try 6 Supervision active. Enfant PID = %d\n", child_pid);

    /* Protocole de passage du nom du TTY à l'enfant */
    if (read_sync_pipe(config.pipe_c2m[0]) == SYNC_DEV_READY) {
        char msg[PATH_MAX + 1];
        snprintf(msg, sizeof(msg), "%s\n", slave_name);
        if (write(config.pipe_m2c[1], msg, strlen(msg)) != (ssize_t)strlen(msg)) {
            kill(child_pid, SIGKILL);
            close(log_fd);
            close(master_fd);
            return EXIT_FAILURE;
        }
        
        if (read_sync_pipe(config.pipe_c2m[0]) != SYNC_PTY_DONE) {
            fprintf(stderr, PRG ": L'enfant a échoué à initialiser le PTY.\n");
            kill(child_pid, SIGKILL);
            close(log_fd);
            close(master_fd);
            return EXIT_FAILURE;
        }
        printf(PRG ": Liaison PTY etablie. Collecte active.\n");
    } else {
        kill(child_pid, SIGKILL);
        close(log_fd);
        close(master_fd);
        return EXIT_FAILURE;
    }

    close(config.pipe_m2c[1]);
    close(config.pipe_c2m[0]);

    /* --- BOUCLE DE SÉCURISATION ET REDIRECTION FLUX --- */
    char buffer[4096];
    ssize_t nbytes;

    /* Le Maître aspire tout ce que l'enfant écrit sur son TTY (stdout/stderr) 
       et l'injecte de manière transparente dans le fichier de log de l'hôte */
    while ((nbytes = read(master_fd, buffer, sizeof(buffer))) > 0) {
        (void)write(log_fd, buffer, nbytes);
    }

    close(log_fd);
    close(master_fd);

    int status;
    waitpid(child_pid, &status, 0);
    printf("\n" PRG ": Fin de l'exécution du conteneur.\n");

    return EXIT_SUCCESS;
}
