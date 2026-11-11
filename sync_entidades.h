#ifndef SYNC_ENTIDADES_H
#define SYNC_ENTIDADES_H

#include "sync.h"

/** Agrega uuid/timestamps, tombstones y triggers a camisetas y botines. */
int sync_entidades_init_db(void);

/** Sube camisetas, botines y tombstones locales a PocketBase. */
int sync_entidades_subir(const char *base, const SyncConfig *cfg,
                         SyncResultado *res);

/** Baja camisetas y botines remotos y los aplica localmente. */
int sync_entidades_bajar(const char *base, const SyncConfig *cfg,
                         SyncResultado *res);

#endif /* SYNC_ENTIDADES_H */
