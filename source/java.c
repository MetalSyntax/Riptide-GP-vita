/**
 * @file  java.c
 * @brief FalsoJNI tables for Riptide GP (com.vectorunit.blue).
 *
 * Method names come from the strings libBlue.so passes to GetMethodID /
 * GetStaticMethodID (see `strings libBlue.so`), cross-checked against the
 * jadx sources in decompiled/apk_jadx/sources/com/vectorunit/. FalsoJNI
 * resolves method IDs by name only, so e.g. every helper's getInstance()
 * shares one entry.
 *
 * How the engine reaches Java (VuAndroid*::bindJavaMethods):
 *   activity.clazz.getClassLoader().loadClass("com.vectorunit.VuXxxHelper")
 *   -> GetStaticMethodID(cls, "getInstance") -> CallStaticObjectMethod
 *   -> GetMethodID(cls, "<method>") on the returned instance.
 * Every object handed back here is a real heap pointer (strdup) so that
 * NewGlobalRef/DeleteLocalRef on it behave.
 */

#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

#include <stdlib.h>
#include <string.h>

#include <psp2/apputil.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/system_param.h>
#include <so_util/so_util.h>

#include "reimpl/native_activity.h"
#include "utils/logger.h"

extern so_module so_mod;

#define MOGA_DEVICE_ID 305419896

enum {
    M_GET_CLASS_LOADER = 1,
    M_LOAD_CLASS,
    M_GET_INSTANCE,
    M_IS_DEVICE_CONNECTED,
    M_GET_SYSTEM_SERVICE,
    M_GET_DEFAULT_DISPLAY,
    M_GET_ROTATION,
    M_GET_DEFAULT,           // java.util.Locale.getDefault()
    M_GET_LANGUAGE,
    M_GET_COUNTRY,
    M_IS_SUPPORTED,          // VuBlueGojiHelper
    M_IS_SIGNED_IN,          // VuOnlineHelper
    M_OPEN_CONNECTION,       // VuHttpHelper
    M_GET_STRING_GENERIC,

    // void methods, all no-ops on the Vita
    M_VOID_FIRST = 100,
    M_INITIALIZE = M_VOID_FIRST,
    M_SHOW_TOAST,
    M_HANDLE_ERROR,
    M_DEBUG_LOG,
    M_PLAY_MOGA_EFFECT,
    M_SHOW_OVERLAY,
    M_START_CLOUD_LOAD,
    M_START_CLOUD_SAVE,
    M_SEND_REQUEST,
    M_SET_REQUEST_PROPERTY,
    M_SET_TIMEOUT_MS,
    M_LOGIN,
    M_GET_SCORES,
    M_REFRESH_ACHIEVEMENTS,
    M_SHOW_ACHIEVEMENTS,
    M_SHOW_ALL_LEADERBOARDS,
    M_SHOW_DASHBOARD,
    M_SHOW_LEADERBOARD,
    M_SUBMIT_SCORE,
    M_UNLOCK_ACHIEVEMENT,
    M_SHOW_FACEBOOK,
    M_SHOW_GOOGLEPLUS,
    M_SHOW_MARKET,
    M_SHOW_TWITTER,
    M_SHOW_WEBPAGE,
    M_START_PARAMS,
    M_ADD_PARAM,
    M_LOG_EVENT,
    M_GET_ACHIEVEMENTS,
    M_GET_HIDDEN_PLAYERS,
    M_GET_LEADERBOARD_SCORES,
    M_CREATE_DIRECTORY_JAVA,
    M_BEGIN_SIGN_IN,
    M_SIGN_OUT,
    M_GAME_INITIALIZE,
    M_RESET_ACHIEVEMENT,
    M_RESET_ALL_ACHIEVEMENTS,
    M_RESET_LEADERBOARD_SCORES,
    M_HIDE_PLAYER,
    M_UNHIDE_PLAYER,
};

NameToMethodID nameToMethodId[] = {
    { M_GET_CLASS_LOADER,    "getClassLoader",     METHOD_TYPE_OBJECT },
    { M_LOAD_CLASS,          "loadClass",          METHOD_TYPE_OBJECT },
    { M_GET_INSTANCE,        "getInstance",        METHOD_TYPE_OBJECT },
    { M_IS_DEVICE_CONNECTED, "isDeviceConnected",  METHOD_TYPE_BOOLEAN },
    { M_GET_SYSTEM_SERVICE,  "getSystemService",   METHOD_TYPE_OBJECT },
    { M_GET_DEFAULT_DISPLAY, "getDefaultDisplay",  METHOD_TYPE_OBJECT },
    { M_GET_ROTATION,        "getRotation",        METHOD_TYPE_INT },
    { M_GET_DEFAULT,         "getDefault",         METHOD_TYPE_OBJECT },
    { M_GET_LANGUAGE,        "getLanguage",        METHOD_TYPE_OBJECT },
    { M_GET_COUNTRY,         "getCountry",         METHOD_TYPE_OBJECT },
    { M_IS_SUPPORTED,        "isSupported",        METHOD_TYPE_BOOLEAN },
    { M_IS_SIGNED_IN,        "isSignedIn",         METHOD_TYPE_BOOLEAN },
    { M_OPEN_CONNECTION,     "openConnection",     METHOD_TYPE_OBJECT },

    { M_INITIALIZE,             "initialize",               METHOD_TYPE_VOID },
    { M_SHOW_TOAST,             "showToast",                METHOD_TYPE_VOID },
    { M_HANDLE_ERROR,           "handleError",              METHOD_TYPE_VOID },
    { M_DEBUG_LOG,              "debugLog",                 METHOD_TYPE_VOID },
    { M_PLAY_MOGA_EFFECT,       "playMogaEffect",           METHOD_TYPE_VOID },
    { M_SHOW_OVERLAY,           "showOverlay",              METHOD_TYPE_VOID },
    { M_START_CLOUD_LOAD,       "startCloudLoad",           METHOD_TYPE_VOID },
    { M_START_CLOUD_SAVE,       "startCloudSave",           METHOD_TYPE_VOID },
    { M_SEND_REQUEST,           "sendRequest",              METHOD_TYPE_VOID },
    { M_SET_REQUEST_PROPERTY,   "setRequestProperty",       METHOD_TYPE_VOID },
    { M_SET_TIMEOUT_MS,         "setTimeoutMS",             METHOD_TYPE_VOID },
    { M_LOGIN,                  "login",                    METHOD_TYPE_VOID },
    { M_GET_SCORES,             "getScores",                METHOD_TYPE_VOID },
    { M_REFRESH_ACHIEVEMENTS,   "refreshAchievements",      METHOD_TYPE_VOID },
    { M_SHOW_ACHIEVEMENTS,      "showAchievements",         METHOD_TYPE_VOID },
    { M_SHOW_ALL_LEADERBOARDS,  "showAllLeaderboards",      METHOD_TYPE_VOID },
    { M_SHOW_DASHBOARD,         "showDashboard",            METHOD_TYPE_VOID },
    { M_SHOW_LEADERBOARD,       "showLeaderboard",          METHOD_TYPE_VOID },
    { M_SUBMIT_SCORE,           "submitScoreToLeaderboard", METHOD_TYPE_VOID },
    { M_UNLOCK_ACHIEVEMENT,     "unlockAchievement",        METHOD_TYPE_VOID },
    { M_SHOW_FACEBOOK,          "showFacebookPage",         METHOD_TYPE_VOID },
    { M_SHOW_GOOGLEPLUS,        "showGooglePlusPage",       METHOD_TYPE_VOID },
    { M_SHOW_MARKET,            "showMarket",               METHOD_TYPE_VOID },
    { M_SHOW_TWITTER,           "showTwitterPage",          METHOD_TYPE_VOID },
    { M_SHOW_WEBPAGE,           "showWebPage",              METHOD_TYPE_VOID },
    { M_START_PARAMS,           "startParams",              METHOD_TYPE_VOID },
    { M_ADD_PARAM,              "addParam",                 METHOD_TYPE_VOID },
    { M_LOG_EVENT,              "logEvent",                 METHOD_TYPE_VOID },
    { M_GET_ACHIEVEMENTS,       "getAchievements",          METHOD_TYPE_VOID },
    { M_GET_HIDDEN_PLAYERS,     "getHiddenPlayers",         METHOD_TYPE_VOID },
    { M_GET_LEADERBOARD_SCORES, "getLeaderboardScores",     METHOD_TYPE_VOID },
    { M_CREATE_DIRECTORY_JAVA,  "createDirectory",          METHOD_TYPE_VOID },
    { M_BEGIN_SIGN_IN,          "beginUserInitiatedSignIn", METHOD_TYPE_VOID },
    { M_SIGN_OUT,               "signOut",                  METHOD_TYPE_VOID },
    { M_GAME_INITIALIZE,        "gameInitialize",            METHOD_TYPE_VOID },
    { M_RESET_ACHIEVEMENT,      "resetAchievement",          METHOD_TYPE_VOID },
    { M_RESET_ALL_ACHIEVEMENTS, "resetAllAchievements",      METHOD_TYPE_VOID },
    { M_RESET_LEADERBOARD_SCORES, "resetLeaderboardScores",    METHOD_TYPE_VOID },
    { M_HIDE_PLAYER,            "hidePlayer",                METHOD_TYPE_VOID },
    { M_UNHIDE_PLAYER,          "unhidePlayer",              METHOD_TYPE_VOID },
};

/* ---------------------------------------------------------------------------
 * Object methods
 * ------------------------------------------------------------------------ */

static jobject dummy_object(const char *tag) {
    return (jobject)strdup(tag);
}

static jobject getClassLoader(jmethodID id, va_list args) {
    return dummy_object("java/lang/ClassLoader");
}

static jobject loadClass(jmethodID id, va_list args) {
    jstring name = va_arg(args, jstring);
    const char *s = name ? jni->GetStringUTFChars(&jni, name, NULL) : NULL;
    l_debug("JNI: ClassLoader.loadClass(%s)", s ? s : "(null)");
    jobject ret = dummy_object(s ? s : "unknown");
    if (s)
        jni->ReleaseStringUTFChars(&jni, name, s);
    return ret;
}

static jobject getInstance(jmethodID id, va_list args) {
    return dummy_object("com/vectorunit/VuHelperInstance");
}

static jobject getSystemService(jmethodID id, va_list args) {
    return dummy_object("android/view/WindowManager");
}

static jobject getDefaultDisplay(jmethodID id, va_list args) {
    return dummy_object("android/view/Display");
}

static jobject getDefault(jmethodID id, va_list args) {
    return dummy_object("java/util/Locale");
}

static jobject getLanguage(jmethodID id, va_list args) {
    return (jobject)jni->NewStringUTF(&jni, system_language_code());
}

static jobject getCountry(jmethodID id, va_list args) {
    return (jobject)jni->NewStringUTF(&jni, system_country_code());
}

static jobject openConnection(jmethodID id, va_list args) {
    // No networking: VuHttpHelper callers treat NULL as a failed request.
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Primitive methods
 * ------------------------------------------------------------------------ */

static jboolean isDeviceConnected(jmethodID id, va_list args) {
    jint device = va_arg(args, jint);
    // Same rule as VuGamePadHelper.isDeviceConnected(): the MOGA id is always
    // connected (that's where the Vita buttons are routed, see controls.c).
    return device == MOGA_DEVICE_ID ? JNI_TRUE : JNI_FALSE;
}

static jboolean retFalse(jmethodID id, va_list args) {
    return JNI_FALSE;
}

static jint getRotation(jmethodID id, va_list args) {
    // Surface.ROTATION_0: the Vita's natural orientation is landscape.
    return 0;
}

static void voidNoop(jmethodID id, va_list args) {
    fjni_logv_dbg("JNI: void method %i ignored", (int)id);
}

/* ---------------------------------------------------------------------------
 * Google Play Games (VuOnlineHelper) replaced offline.
 *
 * The menu is the Google SKU: without a signed-in player it shows a "Sign In"
 * button, the Achievements entry calls beginUserInitiatedSignIn() and every
 * event leaderboard says "not connected". So gameInitialize() signs in with
 * the Vita user name (what the Java side did with the Google display name),
 * and the calls that used to reach Google are answered locally:
 *  - showAchievements() / showLeaderboard() / showAllLeaderboards(): push the
 *    game's own Screens/Achievements / Screens/Leaderboards (shipped for the
 *    non-Google SKUs, unused on Android).
 *  - getScores(id, scope): one row = the player's personal best for that
 *    event, read from the profile (Stats/<Race|HotLap>/<track>).
 *  - unlockAchievement()/refreshAchievements(): report success; while signed
 *    in, VuAchievementManager waits for that answer.
 * Answers go through online_async(): Java delivered them from the UI thread.
 * Calling the native callbacks inline re-enters VuEventManager from inside
 * an event handler (OnAndroidSignIn -> refreshAchievements -> sendDelayed)
 * and hung the game thread right after sign-in.
 * ------------------------------------------------------------------------ */

typedef struct { uint8_t data[128]; int size; } VuParams; // layout from VuPushScreenUIAction::Trigger

static const struct { const char *id, *track, *mode; } LEADERBOARDS[] = {
    // VuDBAsset/LeaderboardDB: <track>.<Race|HotLap>.GoogleID
    { "CgkI09HCp-cJEAIQIA", "CityA", "Race" },
    { "CgkI09HCp-cJEAIQLA", "CityA", "HotLap" },
    { "CgkI09HCp-cJEAIQJg", "CityA_Reverse", "Race" },
    { "CgkI09HCp-cJEAIQMg", "CityA_Reverse", "HotLap" },
    { "CgkI09HCp-cJEAIQJA", "CityB", "Race" },
    { "CgkI09HCp-cJEAIQMA", "CityB", "HotLap" },
    { "CgkI09HCp-cJEAIQKg", "CityB_Reverse", "Race" },
    { "CgkI09HCp-cJEAIQNg", "CityB_Reverse", "HotLap" },
    { "CgkI09HCp-cJEAIQIg", "CityC", "Race" },
    { "CgkI09HCp-cJEAIQLg", "CityC", "HotLap" },
    { "CgkI09HCp-cJEAIQKA", "CityC_Reverse", "Race" },
    { "CgkI09HCp-cJEAIQNA", "CityC_Reverse", "HotLap" },
    { "CgkI09HCp-cJEAIQIw", "FactoryA", "Race" },
    { "CgkI09HCp-cJEAIQLw", "FactoryA", "HotLap" },
    { "CgkI09HCp-cJEAIQKQ", "FactoryA_Reverse", "Race" },
    { "CgkI09HCp-cJEAIQNQ", "FactoryA_Reverse", "HotLap" },
    { "CgkI09HCp-cJEAIQIQ", "FactoryB", "Race" },
    { "CgkI09HCp-cJEAIQLQ", "FactoryB", "HotLap" },
    { "CgkI09HCp-cJEAIQJw", "FactoryB_Reverse", "Race" },
    { "CgkI09HCp-cJEAIQMw", "FactoryB_Reverse", "HotLap" },
    { "CgkI09HCp-cJEAIQJQ", "FactoryC", "Race" },
    { "CgkI09HCp-cJEAIQMQ", "FactoryC", "HotLap" },
    { "CgkI09HCp-cJEAIQKw", "FactoryC_Reverse", "Race" },
    { "CgkI09HCp-cJEAIQNw", "FactoryC_Reverse", "HotLap" },
};

static char player_name[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE] = "Player";

static void *native(const char *name) {
    void *f = (void *)so_symbol(&so_mod, name);
    if (!f) l_error("online: %s not found in libBlue.so", name);
    return f;
}

static const char *jstr(jstring s) {
    return s ? jni->GetStringUTFChars(&jni, s, NULL) : "";
}

static void gameInitialize(jmethodID id, va_list args) {
    void (*onSignIn)(JNIEnv *, jclass, jstring) = native("Java_com_vectorunit_VuOnlineHelper_onSignIn");
    sceAppUtilSystemParamGetString(SCE_SYSTEM_PARAM_ID_USERNAME, (SceChar8 *)player_name, sizeof(player_name));
    if (onSignIn) onSignIn(&jni, NULL, jni->NewStringUTF(&jni, player_name));
    l_info("online: signed in offline as \"%s\"", player_name);
}

// Same as the game's VuPushScreenUIAction: broadcast OnPushScreen(<screen>).
static void push_screen(const char *screen) {
    void **events = (void **)so_symbol(&so_mod, "_ZN14VuEventManager11mpInterfaceE");
    void (*addValue)(VuParams *, int, const void *, int) = native("_ZN8VuParams8addValueENS_5eTypeEPKvi");
    void (*broadcast)(void *, const char *, const VuParams *) = native("_ZN14VuEventManager9broadcastEPKcRK8VuParams");
    if (!events || !*events || !addValue || !broadcast) return;
    VuParams params = { .size = 0 };
    addValue(&params, 4 /* string, as VuParams::addString */, screen, strlen(screen) + 1);
    broadcast(*events, "OnPushScreen", &params);
}

static void showAchievements(jmethodID id, va_list args) {
    push_screen("Screens/Achievements");
}

// Screens/Leaderboards builds its getScores() id from VuGameUtil's
// Track/GameType, i.e. the last event *played*, not the one highlighted in
// the event list. The event screen's button passes the right GoogleID to
// showLeaderboard(), so remember it and answer getScores() for that one.
static char leaderboard_id[32];

static void showLeaderboard(jmethodID id, va_list args) {
    strncpy(leaderboard_id, jstr(va_arg(args, jstring)), sizeof(leaderboard_id) - 1);
    l_debug("online: showLeaderboard(%s)", leaderboard_id);
    push_screen("Screens/Leaderboards");
}

// Main menu button: no event selected, keep the screen's own id.
static void showAllLeaderboards(jmethodID id, va_list args) {
    leaderboard_id[0] = '\0';
    l_debug("online: showAllLeaderboards()");
    push_screen("Screens/Leaderboards");
}

#define NUM_BOARDS (sizeof(LEADERBOARDS) / sizeof(LEADERBOARDS[0]))

// Personal best in ms for LEADERBOARDS[i], or 0 if none.
static int personal_best(size_t i) {
    void **profile = (void **)so_symbol(&so_mod, "_ZN16VuProfileManager11mpInterfaceE");
    const void *(*get)(const void *, const char *) = native("_ZNK15VuJsonContainerixEPKc");
    _Bool (*get_float)(const void *, float *) = native("_ZNK15VuJsonContainer8getValueERf");
    if (!profile || !*profile || !get || !get_float) return 0;
    int race = strcmp(LEADERBOARDS[i].mode, "Race") == 0;
    // Same path as VuStatsManager::getRaceTime / getHotLapTime.
    const void *node = get((uint8_t *)*profile + 8, "Stats");
    node = get(get(node, LEADERBOARDS[i].mode), LEADERBOARDS[i].track);
    float t;
    // raw score = milliseconds, same as submitScoreToLeaderboard
    return get_float(get(node, race ? "TotalTime" : "BestLapTime"), &t) && t > 0.0f ? (int)(t * 1000.0f + 0.5f) : 0;
}

enum { JOB_SCORES, JOB_UNLOCKED, JOB_REFRESHED };
typedef struct { int ms; char name[48]; } ScoreRow;
typedef struct { int kind; char str[64]; int rows; ScoreRow row[NUM_BOARDS]; } OnlineJob;

static int online_job(SceSize len, void *argp) {
    const OnlineJob *j = argp;
    l_debug("online: callback kind=%d rows=%d str=%s", j->kind, j->rows, j->str);
    if (j->kind == JOB_SCORES) {
        void (*begin)(JNIEnv *, jclass) = native("Java_com_vectorunit_VuOnlineHelper_onGetScoresSuccessBegin");
        void (*addRow)(JNIEnv *, jclass, jint, jint, jstring) = native("Java_com_vectorunit_VuOnlineHelper_onGetScoresSuccessAddRow");
        void (*end)(JNIEnv *, jclass) = native("Java_com_vectorunit_VuOnlineHelper_onGetScoresSuccessEnd");
        if (begin && addRow && end) {
            begin(&jni, NULL);
            for (int r = 0; r < j->rows; r++)
                addRow(&jni, NULL, r + 1, j->row[r].ms, jni->NewStringUTF(&jni, j->row[r].name));
            end(&jni, NULL);
        }
    } else if (j->kind == JOB_UNLOCKED) {
        void (*result)(JNIEnv *, jclass, jstring, jboolean) = native("Java_com_vectorunit_VuOnlineHelper_onUnlockAchievementResult");
        if (result) result(&jni, NULL, jni->NewStringUTF(&jni, j->str), JNI_TRUE);
    } else {
        void (*done)(JNIEnv *, jclass) = native("Java_com_vectorunit_VuOnlineHelper_onRefreshAchievementsDone");
        if (done) done(&jni, NULL);
    }
    return sceKernelExitDeleteThread(0);
}

static void online_start(const OnlineJob *j) {
    SceUID t = sceKernelCreateThread("online_cb", online_job, 0x10000100, 0x10000, 0, 0, NULL);
    // StartThread copies *j onto the new thread's stack.
    if (t < 0 || sceKernelStartThread(t, sizeof(*j), j) < 0)
        l_error("online: callback thread failed (0x%08X)", (unsigned)t);
}

static void online_async(int kind, const char *str) {
    OnlineJob j = { .kind = kind };
    strncpy(j.str, str ? str : "", sizeof(j.str) - 1);
    online_start(&j);
}

// The table asks for VuGameUtil's Track/GameType board, which is empty until an
// event is started, so an unknown id lists every personal best, one row each.
static void getScores(jmethodID id, va_list args) {
    const char *board = jstr(va_arg(args, jstring));
    if (leaderboard_id[0]) board = leaderboard_id;
    OnlineJob j = { .kind = JOB_SCORES };
    size_t match = NUM_BOARDS;
    for (size_t i = 0; i < NUM_BOARDS; i++)
        if (strcmp(LEADERBOARDS[i].id, board) == 0) match = i;
    for (size_t i = 0; i < NUM_BOARDS; i++) {
        if (match != NUM_BOARDS && i != match) continue;
        int ms = personal_best(i); // profile read stays on the game thread
        if (!ms) continue;
        j.row[j.rows].ms = ms;
        if (match != NUM_BOARDS)
            snprintf(j.row[j.rows].name, sizeof(j.row[0].name), "%s", player_name);
        else
            snprintf(j.row[j.rows].name, sizeof(j.row[0].name), "%s %s", LEADERBOARDS[i].track,
                     strcmp(LEADERBOARDS[i].mode, "Race") == 0 ? "Race" : "Hot Lap");
        j.rows++;
    }
    l_debug("online: getScores(%s) -> %d row(s)", board, j.rows);
    online_start(&j);
}

static void unlockAchievement(jmethodID id, va_list args) {
    online_async(JOB_UNLOCKED, jstr(va_arg(args, jstring)));
}

static void refreshAchievements(jmethodID id, va_list args) {
    online_async(JOB_REFRESHED, NULL);
}

MethodsBoolean methodsBoolean[] = {
    { M_IS_DEVICE_CONNECTED, isDeviceConnected },
    { M_IS_SUPPORTED,        retFalse },
    { M_IS_SIGNED_IN,        retFalse },
};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {
    { M_GET_ROTATION, getRotation },
};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
    { M_GET_CLASS_LOADER,    getClassLoader },
    { M_LOAD_CLASS,          loadClass },
    { M_GET_INSTANCE,        getInstance },
    { M_GET_SYSTEM_SERVICE,  getSystemService },
    { M_GET_DEFAULT_DISPLAY, getDefaultDisplay },
    { M_GET_DEFAULT,         getDefault },
    { M_GET_LANGUAGE,        getLanguage },
    { M_GET_COUNTRY,         getCountry },
    { M_OPEN_CONNECTION,     openConnection },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
    { M_INITIALIZE, voidNoop },
    { M_SHOW_TOAST, voidNoop },
    { M_HANDLE_ERROR, voidNoop },
    { M_DEBUG_LOG, voidNoop },
    { M_PLAY_MOGA_EFFECT, voidNoop },
    { M_SHOW_OVERLAY, voidNoop },
    { M_START_CLOUD_LOAD, voidNoop },
    { M_START_CLOUD_SAVE, voidNoop },
    { M_SEND_REQUEST, voidNoop },
    { M_SET_REQUEST_PROPERTY, voidNoop },
    { M_SET_TIMEOUT_MS, voidNoop },
    { M_LOGIN, voidNoop },
    { M_GET_SCORES, getScores },
    { M_REFRESH_ACHIEVEMENTS, refreshAchievements },
    { M_SHOW_ACHIEVEMENTS, showAchievements },
    { M_SHOW_ALL_LEADERBOARDS, showAllLeaderboards },
    { M_SHOW_DASHBOARD, voidNoop },
    { M_SHOW_LEADERBOARD, showLeaderboard },
    { M_SUBMIT_SCORE, voidNoop },
    { M_UNLOCK_ACHIEVEMENT, unlockAchievement },
    { M_SHOW_FACEBOOK, voidNoop },
    { M_SHOW_GOOGLEPLUS, voidNoop },
    { M_SHOW_MARKET, voidNoop },
    { M_SHOW_TWITTER, voidNoop },
    { M_SHOW_WEBPAGE, voidNoop },
    { M_START_PARAMS, voidNoop },
    { M_ADD_PARAM, voidNoop },
    { M_LOG_EVENT, voidNoop },
    { M_GET_ACHIEVEMENTS, voidNoop },
    { M_GET_HIDDEN_PLAYERS, voidNoop },
    { M_GET_LEADERBOARD_SCORES, voidNoop },
    { M_CREATE_DIRECTORY_JAVA, voidNoop },
    { M_BEGIN_SIGN_IN, voidNoop },
    { M_SIGN_OUT, voidNoop },
    { M_GAME_INITIALIZE, gameInitialize },
    { M_RESET_ACHIEVEMENT, voidNoop },
    { M_RESET_ALL_ACHIEVEMENTS, voidNoop },
    { M_RESET_LEADERBOARD_SCORES, voidNoop },
    { M_HIDE_PLAYER, voidNoop },
    { M_UNHIDE_PLAYER, voidNoop },
};

/*
 * JNI Fields
 */

// https://developer.android.com/reference/android/content/Context.html#WINDOW_SERVICE
char WINDOW_SERVICE[] = "window";

// android.os.Build.VERSION.SDK_INT -- 19 = KitKat (targetSdk of the APK is lower,
// the engine only checks it for immersive mode / API availability).
const int SDK_INT = 19;

NameToFieldID nameToFieldId[] = {
    { 0, "WINDOW_SERVICE", FIELD_TYPE_OBJECT },
    { 1, "SDK_INT", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
    { 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
    { 0, WINDOW_SERVICE },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES

/* ---------------------------------------------------------------------------
 * Runtime overrides of stock FalsoJNI behaviour
 * ------------------------------------------------------------------------ */

extern struct JNINativeInterface *_jni;

// FMOD's Java_org_fmod_FMODAudioDevice_fmodProcess() writes the mix into
// GetDirectBufferAddress(byteBuffer). audio.c passes a raw malloc'ed buffer
// as the "ByteBuffer", so hand the pointer straight back (stock returns NULL).
static void *GetDirectBufferAddress_passthrough(JNIEnv *env, jobject buf) {
    return (void *)buf;
}

void java_overrides_init(void) {
    _jni->GetDirectBufferAddress = GetDirectBufferAddress_passthrough;
}
