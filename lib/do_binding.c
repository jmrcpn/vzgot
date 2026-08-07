/************************************************/
/* */
/* procedure to bind host's /proc/ target       */
/* to container's /proc/ target AFTER pivot.    */
/* */
/************************************************/
_Bool do_binding(pid_t contpid, const char *target)
{
#define OPEP    "unicnt2.c:do_binding,"

    _Bool isok = false;
    int status;
    int fd_ns_host = -1;
    int fd_ns_cont = -1;
    char nspath[128];
    char ppath[512]; // Chemin source (du pov du conteneur ou partagé)
    char dpath[512]; // Chemin destination interne au conteneur

    // 1. Sauvegarder le namespace de montage actuel du superviseur (hôte)
    fd_ns_host = open("/proc/self/ns/mnt", O_RDONLY);
    
    // 2. Ouvrir le namespace de montage du conteneur ciblé
    (void)snprintf(nspath, sizeof(nspath), "/proc/%d/ns/mnt", contpid);
    fd_ns_cont = open(nspath, O_RDONLY);

    if (fd_ns_host == -1 || fd_ns_cont == -1) {
        (void) log_alert(0, "%s unable to access namespaces (error=<%s>)", OPEP, strerror(errno));
        goto cleanup;
    }

    // 3. Sauter à l'intérieur du namespace de montage du conteneur
    if (setns(fd_ns_cont, CLONE_NEWNS) < 0) {
        (void) log_alert(0, "%s unable to join container namespace (error=<%s>)", OPEP, strerror(errno));
        goto cleanup;
    }

    /* ----------------------------------------------------------------- */
    /* ZONE INTERNE AU CONTENEUR                                         */
    /* Du point de vue d'ici, la racine a pivoté !                       */
    /* Le 'rootfs' n'existe plus, on cible directement les vrais chemins */
    /* ----------------------------------------------------------------- */
    
    // Chemin source : L'endroit où le superviseur écrit sur l'hôte, 
    // qui doit être accessible via un bind antérieur ou un volume partagé,
    // ou alors on utilise le chemin de transit de l'hôte si visible.
    // NOTE : Si /proc/loadavg dans le container doit refléter /vzgot/test/NAME/proc/loadavg,
    // assurez-vous que ce chemin d'attente est bien monté dans le container au boot.
    (void)snprintf(ppath, sizeof(ppath), "/etc/vzgot/%s", target); // ou votre point d'entrée partagé
    (void)snprintf(dpath, sizeof(dpath), "/proc/%s", target);

    // Étape A : Bind Mount
    status = MS_BIND | MS_REC;
    if (mount(ppath, dpath, (const char *)0, status, (const void *)0) < 0) {
        (void) log_alert(0, "%s mount bind failed inside container (error=<%s>)", OPEP, strerror(errno));
        goto restore;
    }

    // Étape B : Propagation Shared
    status = MS_REC | MS_SHARED;
    (void)mount((const char *)0, dpath, (const char *)0, status, (const void *)0);

    // Étape C : Passage en Read-Only
    status = MS_BIND | MS_REMOUNT | MS_RDONLY;
    if (mount(ppath, dpath, (const char *)0, status, (const void *)0) < 0) {
        (void) log_alert(0, "%s remount readonly failed inside container (error=<%s>)", OPEP, strerror(errno));
        goto restore;
    }

    isok = true;

restore:
    /* ----------------------------------------------------------------- */
    /* RETOUR À LA RÉALITÉ                                               */
    /* On force le superviseur à revenir dans son namespace global       */
    /* ----------------------------------------------------------------- */
    if (setns(fd_ns_host, CLONE_NEWNS) < 0) {
        (void) log_alert(0, "%s FATAL: unable to restore supervisor namespace!", OPEP);
        exit(EXIT_FAILURE); // Si on ne peut pas revenir, on coupe pour éviter les catastrophes
    }

cleanup:
    if (fd_ns_host != -1) close(fd_ns_host);
    if (fd_ns_cont != -1) close(fd_ns_cont);

    return isok;

#undef  OPEP
}
