#include <sys/syscall.h>
#include <poll.h>

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

static int pidfd_open(pid_t pid, unsigned int flags) {
    return syscall(SYS_pidfd_open, pid, flags);
}

// ... Dans le superviseur ...

int status;
int intervalle_travail_ms = 5000; // Exemple : 5 secondes
int pfd = pidfd_open(child_pid, 0);

if (pfd >= 0) {
    struct pollfd pfd_struct;
    pfd_struct.fd = pfd;
    pfd_struct.events = POLLIN;

    while (1) {
        // poll() bloque ici. Il se réveille SI le conteneur meurt, 
        // OU si le timeout de 5 secondes expire.
        int ready = poll(&pfd_struct, 1, intervalle_travail_ms);

        if (ready > 0) {
            // Le pidfd est lisible ! Le conteneur vient de mourir à l'instant.
            printf("[%s] Détection instantanée : Le conteneur est sorti !\n", HST);
            waitpid(child_pid, &status, 0); // Nettoyage final immédiat
            break;
        } else if (ready == 0) {
            // Le timeout a expiré (5 secondes se sont écoulées)
            // --- ZONE DE TRAVAIL PÉRIODIQUE ---
            // C'est ici que vous mettez à jour meminfo, loadavg, etc.
            // ----------------------------------
            printf("[%s] Cycle de travail périodique...\n", HST);
        } else {
            if (errno == EINTR) continue;
            perror("Erreur poll");
            break;
        }
    }
    close(pfd);
}
