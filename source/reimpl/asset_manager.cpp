#include "reimpl/asset_manager.h"
#include "utils/logger.h"

#include <pthread.h>
#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <libc_bridge/libc_bridge.h>
#include <string>

typedef struct assetManager {
    int dummy = 0; // TODO: mb we will need to store something here in future
    pthread_mutex_t mLock;
} assetManager;

typedef struct aAsset {
    char * filename;
    FILE* f;
    off_t pos;
    off_t fileSize;
} asset;

#ifdef USE_SCELIBC_IO
#define A_FOPEN  sceLibcBridge_fopen
#define A_FCLOSE sceLibcBridge_fclose
#define A_FREAD  sceLibcBridge_fread
#define A_FSEEK  sceLibcBridge_fseek
#define A_FTELL  sceLibcBridge_ftell
#else
#define A_FOPEN  fopen
#define A_FCLOSE fclose
#define A_FREAD  fread
#define A_FSEEK  fseek
#define A_FTELL  ftell
#endif

static AAssetManager * g_AAssetManager = nullptr;

AAssetManager * AAssetManager_create() {
    if (g_AAssetManager) return g_AAssetManager;

    assetManager am;

    pthread_mutex_init(&am.mLock, nullptr);

    g_AAssetManager = (AAssetManager *) malloc(sizeof(assetManager));
    memcpy(g_AAssetManager, &am, sizeof(assetManager));

    return g_AAssetManager;
}

// NB: the stock boilerplate closed the FILE right after measuring it and
// returned fseek()'s status from AAsset_seek. libBlue.so reads *only* through
// AAsset_read/AAsset_seek (VuAndroidFile::read/seek/tell/size, @0x18fc2a) and
// derives tell/size from AAsset_seek's return value, so the file must stay
// open and seek must return the new absolute offset like bionic does.
AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode) {
    std::string realp = std::string(DATA_PATH) + std::string("assets/") + std::string(filename);

    FILE *f = A_FOPEN(realp.c_str(), "rb");
    if (!f) {
        l_debug("[AAssetManager] AAssetManager_open(%s): not found", realp.c_str());
        return nullptr;
    }

    auto * a = new aAsset;
    a->filename = strdup(realp.c_str());
    a->f = f;
    a->pos = 0;
    A_FSEEK(f, 0, SEEK_END);
    a->fileSize = (off_t) A_FTELL(f);
    A_FSEEK(f, 0, SEEK_SET);

    l_debug("[AAssetManager] AAssetManager_open(%p, %s, %i): %p (%li bytes)", mgr,
            realp.c_str(), mode, a, (long) a->fileSize);
    return (AAsset *) a;
}

void AAsset_close(AAsset* asset) {
    l_debug("AAsset_close(%p)", asset);

    if (asset) {
        auto * a = (aAsset *) asset;
        A_FCLOSE(a->f);
        free(a->filename);
        delete a;
    }
}

int AAsset_read(AAsset* asset, void* buf, size_t count) {
    if (!asset) {
        return -1;
    }

    auto * a = (aAsset *) asset;
    size_t ret = A_FREAD(buf, 1, count, a->f);
    a->pos += (off_t) ret;
    return (int) ret;
}

off_t AAsset_seek(AAsset* asset, off_t offset, int whence) {
    if (!asset) {
        return (off_t) -1;
    }

    auto * a = (aAsset *) asset;
    off_t target;
    switch (whence) {
        case SEEK_SET: target = offset; break;
        case SEEK_CUR: target = a->pos + offset; break;
        case SEEK_END: target = a->fileSize + offset; break;
        default: return (off_t) -1;
    }
    if (target < 0 || target > a->fileSize)
        return (off_t) -1;

    if (target != a->pos) {
        if (A_FSEEK(a->f, (long) target, SEEK_SET) != 0)
            return (off_t) -1;
        a->pos = target;
    }
    return target;
}

off_t AAsset_getRemainingLength(AAsset* asset) {
    if (!asset) {
        return (off_t) -1;
    }

    auto * a = (aAsset *) asset;
    return a->fileSize - a->pos;
}

off_t AAsset_getLength(AAsset* asset) {
    if (!asset) {
        return (off_t) -1;
    }

    auto * a = (aAsset *) asset;
    return a->fileSize;
}

AAssetDir* AAssetManager_openDir(AAssetManager* mgr, const char* dirName) {
    l_error("AAssetManager_openDir: %s", dirName);
    return (AAssetDir *)strdup("dummy");
}

void AAssetDir_close(AAssetDir* assetDir) {
    l_error("AAssetDir_close");
    free(assetDir);
}
