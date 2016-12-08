#include <glib-2.0/gio/gio.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/glib/gprintf.h>
#include <glib-2.0/glib/gstdio.h>

#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "../../general/src/general_helper.h"

#include "../../general/src/debug_macro.h"

#ifdef UNIT_TESTING
#include <cmockery/cmockery_override.h>
#endif

#define MAX_NUM_THREADS 8

#define SYNC_PATH_FILE "sync_path.txt"

//#define SYNC_PATH_FILE
//"/home/onionhuang/programming_projects/c/file_sync/sync_path.txt"

#define NEW_LINE_CHAR "\n"
#define SYNC_PATH_SPLIT "->"

#ifdef DEBUG

static gboolean inSyncPath(const char* path)
{
  gboolean re = FALSE;

  ENSURE_MSG(g_file_test(SYNC_PATH_FILE, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", SYNC_PATH_FILE);

  char* syncPathContents = fileReadContents(SYNC_PATH_FILE);

  ENSURE(syncPathContents != NULL);

  char** syncNewLineSplit = g_strsplit(syncPathContents, NEW_LINE_CHAR, 0);

  ENSURE(syncNewLineSplit != NULL);

  int i = 0;
  while (1) {
    if (syncNewLineSplit[i] == NULL) {
      break;
    }

    ENSURE(syncNewLineSplit[i] != NULL);

    char** splitSrcDes = g_strsplit(syncNewLineSplit[i], SYNC_PATH_SPLIT, 0);

    if (splitSrcDes[0] == NULL || splitSrcDes[1] == NULL) {
      goto clean;
    }

    ENSURE(splitSrcDes[0] != NULL);
    ENSURE(splitSrcDes[1] != NULL);

    char* srcPath = g_strstrip(splitSrcDes[0]);
    char* desPath = g_strstrip(splitSrcDes[1]);

    ENSURE(path != NULL);

    ENSURE(srcPath != NULL);
    ENSURE(desPath != NULL);

    ENSURE_MSG(g_file_test(srcPath, G_FILE_TEST_EXISTS),
        "FilePath does not exist: %s\n", srcPath);
    ENSURE_MSG(g_file_test(desPath, G_FILE_TEST_EXISTS),
        "FilePath does not exist: %s\n", desPath);

    if (g_str_match_string(srcPath, path, TRUE) || g_str_match_string(desPath, path, TRUE)) {
      re = TRUE;
      break;
    }

  clean:
    i++;
  }

  return re;
}

#endif

static gboolean fileDiffTime(const char* src, const char* des)
{
  ENSURE(src != NULL);
  ENSURE(des != NULL);

  ENSURE_MSG(g_file_test(src, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", src);
  ENSURE_MSG(g_file_test(des, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", des);

  ENSURE_MSG(inSyncPath(src), "FilePath does not locate in the SyncPath: %s\n",
      src);
  ENSURE_MSG(inSyncPath(des), "FilePath does not locate in the SyncPath: %s\n",
      des);

  gboolean re = FALSE;

  GError* err = NULL;

  GFile* srcFile = g_file_new_for_path(src);
  GFile* desFile = g_file_new_for_path(des);

  GFileInfo* srcFileInfo = g_file_query_info(srcFile, G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  GFileInfo* desFileInfo = g_file_query_info(desFile, G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  guint64 srcMtime = g_file_info_get_attribute_uint64(
      srcFileInfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  guint64 desMtime = g_file_info_get_attribute_uint64(
      desFileInfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  ENSURE(srcMtime != 0);
  ENSURE(desMtime != 0);

  if (srcMtime > desMtime) {
    re = TRUE;
  }

  g_object_unref(srcFile);
  g_object_unref(desFile);
  g_object_unref(srcFileInfo);
  g_object_unref(desFileInfo);

  return re;
}

static gboolean fileDiffRefresh(const char* src, const char* des)
{
  ENSURE(src != NULL);
  ENSURE(des != NULL);

  ENSURE_MSG(g_file_test(src, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", src);
  ENSURE_MSG(g_file_test(des, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", des);

  ENSURE_MSG(inSyncPath(src), "FilePath does not locate in the SyncPath: %s\n",
      src);
  ENSURE_MSG(inSyncPath(des), "FilePath does not locate in the SyncPath: %s\n",
      des);

  return TRUE;
}

typedef gboolean fileDiffFunc(const char* src, const char* des);

////////////////////////////////////////////////////////////////////////////////////

static GThreadPool* mainThreadPoolSrcToDes;
static GThreadPool* mainThreadPoolDesToSrc;

static int srcToDesCount = 0;
static int desToSrcCount = 0;

G_LOCK_DEFINE(srcToDesCount);
G_LOCK_DEFINE(desToSrcCount);

struct SyncPath {
  char* firstArg;
  char* secondArg;
};

static struct SyncPath* syncPathNew(char* first, char* second)
{
  struct SyncPath* re = DEFENSE_MALLOC(sizeof(struct SyncPath), mallocFailAbort, NULL);
  re->firstArg = first;
  re->secondArg = second;

  return re;
}

////////////////////////////////////////////////////////////////////////////////////

static fileDiffFunc* diffFunc = fileDiffTime;

static void syncSrctoDes(const char* srcPath, const char* desPath)
{
  ENSURE(srcPath != NULL);
  ENSURE(desPath != NULL);

  ENSURE_MSG(g_file_test(srcPath, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", srcPath);
  ENSURE_MSG(g_file_test(desPath, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", desPath);

  ENSURE_MSG(inSyncPath(srcPath),
      "FilePath does not locate in the SyncPath: %s\n", srcPath);
  ENSURE_MSG(inSyncPath(desPath),
      "FilePath does not locate in the SyncPath: %s\n", desPath);

  GFile* src = g_file_new_for_path(srcPath);

  GError* err = NULL;
  GFileEnumerator* srcEnum = g_file_enumerate_children(
      src, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  while (1) {
    GFileInfo* file = g_file_enumerator_next_file(srcEnum, NULL, &err);
    if (file == NULL) {
      break;
    }

    ENSURE(file != NULL);

    const char* fileName = g_file_info_get_name(file);

    ENSURE(fileName != NULL);

    char* srcFilePath = pathJoin(srcPath, fileName);
    char* desFilePath = pathJoin(desPath, fileName);

    ENSURE(srcFilePath != NULL);
    ENSURE(desFilePath != NULL);

    gboolean desExist = g_file_test(desFilePath, G_FILE_TEST_EXISTS);

    if (g_file_test(srcFilePath, G_FILE_TEST_IS_DIR)) {
      if (!desExist) {
        ENSURE(inSyncPath(desFilePath));

        g_mkdir(desFilePath, 0777);
        printf("make directory:\n%s\n\n", desFilePath);
      }

      ENSURE_MSG(g_file_test(srcFilePath, G_FILE_TEST_EXISTS),
          "FilePath does not exist: %s\n", srcFilePath);
      ENSURE_MSG(g_file_test(desFilePath, G_FILE_TEST_EXISTS),
          "FilePath does not exist: %s\n", desFilePath);

      ENSURE_MSG(inSyncPath(srcFilePath),
          "FilePath does not locate in the SyncPath: %s\n", srcFilePath);
      ENSURE_MSG(inSyncPath(desFilePath),
          "FilePath does not locate in the SyncPath: %s\n", desFilePath);

      struct SyncPath* pathSrcToDes = syncPathNew(srcFilePath, desFilePath);

      g_thread_pool_push(mainThreadPoolSrcToDes, pathSrcToDes, &err);

      // syncSrctoDes(srcFilePath, desFilePath);

    } else {
      if ((!desExist) || diffFunc(srcFilePath, desFilePath)) {
        ENSURE_MSG(inSyncPath(srcFilePath),
            "FilePath does not locate in the SyncPath: %s\n",
            srcFilePath);
        ENSURE_MSG(inSyncPath(desFilePath),
            "FilePath does not locate in the SyncPath: %s\n",
            desFilePath);

        fileCopy(srcFilePath, desFilePath);
        printf("copy file:\nFrom: %s\nTo: %s\n\n", srcFilePath, desFilePath);
      }
    }

    g_object_unref(file);
  }

  g_object_unref(srcEnum);
  g_object_unref(src);
}

static void rmdirWithContents(const char* dir)
{
  ENSURE(dir != NULL);

  ENSURE_MSG(g_file_test(dir, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", dir);

  ENSURE_MSG(inSyncPath(dir), "FilePath does not locate in the SyncPath: %s\n",
      dir);

  GFile* dirFile = g_file_new_for_path(dir);

  GError* err = NULL;
  GFileEnumerator* dirEnum = g_file_enumerate_children(dirFile, G_FILE_ATTRIBUTE_STANDARD_NAME,
      G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  while (1) {
    GFileInfo* file = g_file_enumerator_next_file(dirEnum, NULL, &err);
    if (file == NULL) {
      break;
    }

    ENSURE(file != NULL);

    const char* fileName = g_file_info_get_name(file);
    ENSURE(fileName != NULL);

    char* filePath = pathJoin(dir, fileName);
    ENSURE(filePath != NULL);

    if (g_file_test(filePath, G_FILE_TEST_IS_DIR)) {
      ENSURE_MSG(inSyncPath(filePath),
          "FilePath does not locate in the SyncPath: %s\n", filePath);

      rmdirWithContents(filePath);
      g_rmdir(filePath);

      printf("remove directory:\n%s\n\n", filePath);
    } else {
      ENSURE_MSG(inSyncPath(filePath),
          "FilePath does not locate in the SyncPath: %s\n", filePath);

      g_remove(filePath);
      printf("remove file:\n%s\n\n", filePath);
    }

    free(filePath);
    g_object_unref(file);
  }

  g_object_unref(dirEnum);
  g_object_unref(dirFile);
}

static void syncDesToSrc(const char* desPath, const char* srcPath)
{
  ENSURE(srcPath != NULL);
  ENSURE(desPath != NULL);

  ENSURE_MSG(g_file_test(srcPath, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", srcPath);
  ENSURE_MSG(g_file_test(desPath, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", desPath);

  ENSURE_MSG(inSyncPath(srcPath),
      "FilePath does not locate in the SyncPath: %s\n", srcPath);
  ENSURE_MSG(inSyncPath(desPath),
      "FilePath does not locate in the SyncPath: %s\n", desPath);

  GFile* des = g_file_new_for_path(desPath);

  GError* err = NULL;
  GFileEnumerator* desEnum = g_file_enumerate_children(
      des, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  while (1) {
    GFileInfo* file = g_file_enumerator_next_file(desEnum, NULL, &err);
    if (file == NULL) {
      break;
    }

    ENSURE(file != NULL);

    const char* fileName = g_file_info_get_name(file);
    ENSURE(fileName != NULL);

    char* desFilePath = pathJoin(desPath, fileName);
    char* srcFilePath = pathJoin(srcPath, fileName);

    ENSURE(srcFilePath != NULL);
    ENSURE(desFilePath != NULL);

    gboolean srcExist = g_file_test(srcFilePath, G_FILE_TEST_EXISTS);

    if (g_file_test(desFilePath, G_FILE_TEST_IS_DIR)) {
      if (!srcExist) {
        rmdirWithContents(desFilePath);
        g_rmdir(desFilePath);
        printf("remove directory:\n%s\n\n", desFilePath);
      } else {
        ENSURE_MSG(g_file_test(srcFilePath, G_FILE_TEST_EXISTS),
            "FilePath does not exist: %s\n", srcFilePath);
        ENSURE_MSG(g_file_test(desFilePath, G_FILE_TEST_EXISTS),
            "FilePath does not exist: %s\n", desFilePath);

        ENSURE_MSG(inSyncPath(srcFilePath),
            "FilePath does not locate in the SyncPath: %s\n",
            srcFilePath);
        ENSURE_MSG(inSyncPath(desFilePath),
            "FilePath does not locate in the SyncPath: %s\n",
            desFilePath);

        struct SyncPath* pathDesToSrc = syncPathNew(desFilePath, srcFilePath);

        g_thread_pool_push(mainThreadPoolDesToSrc, pathDesToSrc, &err);

        // syncDesToSrc(desFilePath, srcFilePath);
      }

    } else {
      if (!srcExist) {
        ENSURE_MSG(inSyncPath(desFilePath),
            "FilePath does not locate in the SyncPath: %s\n",
            desFilePath);

        g_remove(desFilePath);
        printf("remove file:\n%s\n\n", desFilePath);
      }
    }

    g_object_unref(file);
  }

  g_object_unref(desEnum);
  g_object_unref(des);
}

static void threadPoolSrcToDes(void* dataFromPush, void* dataFromNew)
{
  struct SyncPath* path = (struct SyncPath*)dataFromPush;

  G_LOCK(srcToDesCount);
  srcToDesCount++;
  G_UNLOCK(srcToDesCount);

  syncSrctoDes(path->firstArg, path->secondArg);

  // syncDesToSrc(desPath, srcPath);

  G_LOCK(srcToDesCount);
  srcToDesCount--;
  G_UNLOCK(srcToDesCount);
}

static void threadPoolDesToSrc(void* dataFromPush, void* dataFromNew)
{
  struct SyncPath* path = (struct SyncPath*)dataFromPush;

  G_LOCK(desToSrcCount);
  desToSrcCount++;
  G_UNLOCK(desToSrcCount);

  syncDesToSrc(path->firstArg, path->secondArg);

  // syncSrctoDes(srcPath, desPath);

  G_LOCK(desToSrcCount);
  desToSrcCount--;
  G_UNLOCK(desToSrcCount);
}

int main(const int argv, const char** args)
{
  GError* err;

  mainThreadPoolSrcToDes = g_thread_pool_new(threadPoolSrcToDes, NULL, MAX_NUM_THREADS, TRUE, &err);

  mainThreadPoolDesToSrc = g_thread_pool_new(threadPoolDesToSrc, NULL, MAX_NUM_THREADS, TRUE, &err);

  // exit(EXIT_SUCCESS);

  if (argv == 2 && (strcmp(args[1], "refresh") == 0)) {
    printf("refresh destination files!\n");
    diffFunc = fileDiffRefresh;

  } else if (argv > 1) {
    printf("unknow command: %s\nperforming usual operation\n", args[1]);
  }

  GTimer* timer = g_timer_new();
  g_timer_start(timer);

  ENSURE_MSG(g_file_test(SYNC_PATH_FILE, G_FILE_TEST_EXISTS),
      "FilePath does not exist: %s\n", SYNC_PATH_FILE);

  char* syncPathContents = fileReadContents(SYNC_PATH_FILE);

  ENSURE(syncPathContents != NULL);

  char** syncNewLineSplit = g_strsplit(syncPathContents, NEW_LINE_CHAR, 0);

  ENSURE(syncNewLineSplit != NULL);

  int i = 0;
  while (1) {
    if (syncNewLineSplit[i] == NULL) {
      break;
    }

    ENSURE(syncNewLineSplit[i] != NULL);

    char** splitSrcDes = g_strsplit(syncNewLineSplit[i], SYNC_PATH_SPLIT, 0);

    if (splitSrcDes[0] == NULL || splitSrcDes[1] == NULL) {
      goto clean;
    }

    ENSURE(splitSrcDes[0] != NULL);
    ENSURE(splitSrcDes[1] != NULL);

    char* srcPath = g_strstrip(splitSrcDes[0]);
    char* desPath = g_strstrip(splitSrcDes[1]);

    ENSURE(srcPath != NULL);
    ENSURE(desPath != NULL);

    if (!g_file_test(srcPath, G_FILE_TEST_EXISTS)) {
      printf("Source Directory does not exits: %s\n", srcPath);
      goto clean;
    }

    if (!g_file_test(desPath, G_FILE_TEST_EXISTS)) {
      printf("Destination Directory does not exits: %s\n", desPath);
      goto clean;
    }

    ENSURE_MSG(g_file_test(srcPath, G_FILE_TEST_EXISTS),
        "FilePath does not exist: %s\n", srcPath);
    ENSURE_MSG(g_file_test(desPath, G_FILE_TEST_EXISTS),
        "FilePath does not exist: %s\n", desPath);

    ENSURE_MSG(inSyncPath(srcPath),
        "FilePath does not locate in the SyncPath: %s\n", srcPath);
    ENSURE_MSG(inSyncPath(desPath),
        "FilePath does not locate in the SyncPath: %s\n", desPath);

    struct SyncPath* pathSrcToDes = syncPathNew(srcPath, desPath);

    g_thread_pool_push(mainThreadPoolSrcToDes, pathSrcToDes, &err);

    struct SyncPath* pathDesToSrc = syncPathNew(desPath, srcPath);

    g_thread_pool_push(mainThreadPoolDesToSrc, pathDesToSrc, &err);

  clean:
    i++;
  }

  g_strfreev(syncNewLineSplit);
  free(syncPathContents);

  while (true) {
    g_thread_yield();

    G_LOCK(srcToDesCount);
    G_LOCK(desToSrcCount);

    bool finish = (srcToDesCount == 0 && desToSrcCount == 0);

    G_UNLOCK(srcToDesCount);
    G_UNLOCK(desToSrcCount);

    if (finish) {
      break;
    }
  }

  G_LOCK(srcToDesCount);
  G_LOCK(desToSrcCount);

  ENSURE(srcToDesCount == 0);
  ENSURE(desToSrcCount == 0);

  G_UNLOCK(srcToDesCount);
  G_UNLOCK(desToSrcCount);

  g_thread_pool_free(mainThreadPoolSrcToDes, FALSE, TRUE);
  g_thread_pool_free(mainThreadPoolDesToSrc, FALSE, TRUE);

  g_timer_stop(timer);
  double executionTime = g_timer_elapsed(timer, NULL);

  printf("execution time: %f seconds\n", executionTime);

  g_timer_destroy(timer);

  //  syncSrctoDes(srcPath, desPath);
  //  syncDesToSrc(desPath, srcPath);

  return 0;
}
