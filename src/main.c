#include <glib-2.0/gio/gio.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/glib/gprintf.h>
#include <glib-2.0/glib/gstdio.h>

#include <stdio.h>
#include <string.h>

#include "../../general/src/general_helper.h"

#include <cmockery/pbc.h>

#ifdef UNIT_TESTING
#include <cmockery/cmockery_override.h>
#endif

static gboolean fileDiffTime(const char *src, const char *des) {
  gboolean re = FALSE;

  GError *err = NULL;

  GFile *srcFile = g_file_new_for_path(src);
  GFile *desFile = g_file_new_for_path(des);

  GFileInfo *srcFileInfo =
      g_file_query_info(srcFile, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                        G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  GFileInfo *desFileInfo =
      g_file_query_info(desFile, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                        G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  guint64 srcMtime = g_file_info_get_attribute_uint64(
      srcFileInfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  guint64 desMtime = g_file_info_get_attribute_uint64(
      desFileInfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  if (srcMtime > desMtime) {
    re = TRUE;
  }

  g_object_unref(srcFile);
  g_object_unref(desFile);
  g_object_unref(srcFileInfo);
  g_object_unref(desFileInfo);

  return re;
}

static gboolean fileDiffContents(const char *src, const char *des) {
  gboolean re = FALSE;

  GError *err = NULL;

  char *srcContents;
  gsize srcLength;
  char *desContents;
  gsize desLength;

  g_file_get_contents(src, &srcContents, &srcLength, &err);
  g_file_get_contents(des, &desContents, &desLength, &err);

  if ((srcLength != desLength) || (strcmp(srcContents, desContents) != 0)) {
    re = TRUE;
  }

  g_free(srcContents);
  g_free(desContents);

  return re;
}

typedef gboolean fileDiffFunc(const char *src, const char *des);
typedef void syncFunc(const char *src, const char *des);

static fileDiffFunc *diffFunc = fileDiffTime;

struct SyncPath {
  char *src;
  char *des;
};

// static void syncSrctoDes(const char *srcPath, const char *desPath);
// static void syncDesToSrc(const char *desPath, const char *srcPath);

static void syncSrctoDes(const char *srcPath, const char *desPath) {
  //  printf("srcPath: %s\n", srcPath);
  //  printf("desPath: %s\n", desPath);

  GFile *src = g_file_new_for_path(srcPath);

  REQUIRE(g_file_query_exists(src, NULL));

  GError *err = NULL;
  GFileEnumerator *srcEnum = g_file_enumerate_children(
      src, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  while (1) {
    GFileInfo *file = g_file_enumerator_next_file(srcEnum, NULL, &err);
    if (file == NULL) {
      break;
    }

    ENSURE(file != NULL);

    const char *fileName = g_file_info_get_name(file);
    char *srcFilePath = pathJoin(srcPath, fileName);
    char *desFilePath = pathJoin(desPath, fileName);

    GFileType fileType = g_file_info_get_file_type(file);
    gboolean desExist = g_file_test(desFilePath, G_FILE_TEST_EXISTS);

    if (fileType == G_FILE_TYPE_DIRECTORY) {
      if (!desExist) {
        g_mkdir(desFilePath, 0777);
      }

      struct SyncPath *pathSrcToDes = g_malloc(sizeof(struct SyncPath) * 1);
      REQUIRE(pathSrcToDes != NULL);
      pathSrcToDes->src = srcFilePath;
      pathSrcToDes->des = desFilePath;

      // struct SyncPath *pathDesToSrc = g_malloc(sizeof(struct SyncPath) * 1);
      // REQUIRE(pathDesToSrc != NULL);
      // pathDesToSrc->src = desPath;
      // pathDesToSrc->des = srcPath;

      g_thread_pool_push(poolSrcToDes, pathSrcToDes, &err);
      ENSURE(err == NULL);

      // g_thread_pool_push(poolDesToSrc, pathDesToSrc, &err);
      // ENSURE(err == NULL);

      // syncSrctoDes(srcFilePath, desFilePath);

    } else {
      // if ((!desExist) || fileDiffTime(srcFilePath, desFilePath)) {
      //  copy_file(srcFilePath, desFilePath);
      //}

      // if ((!desExist) || fileDiffContents(srcFilePath, desFilePath)) {
      //  copy_file(srcFilePath, desFilePath);
      //}

      if ((!desExist) || diffFunc(srcFilePath, desFilePath)) {
        // copy_file(srcFilePath, desFilePath);
        printf("copy file: %s\n", srcFilePath);
      }
    }

    //  free(srcFilePath);
    //  free(desFilePath);
    g_object_unref(file);
  }

  g_object_unref(srcEnum);
  g_object_unref(src);
}

static void rmdirWithContents(const char *dir) {
  GFile *dirFile = g_file_new_for_path(dir);

  REQUIRE(g_file_query_exists(dirFile, NULL));

  GError *err = NULL;
  GFileEnumerator *dirEnum =
      g_file_enumerate_children(dirFile, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  while (1) {
    GFileInfo *file = g_file_enumerator_next_file(dirEnum, NULL, &err);
    if (file == NULL) {
      break;
    }

    ENSURE(file != NULL);

    const char *fileName = g_file_info_get_name(file);
    char *filePath = pathJoin(dir, fileName);

    GFileType fileType = g_file_info_get_file_type(file);

    if (fileType == G_FILE_TYPE_DIRECTORY) {
      rmdirWithContents(filePath);
      // g_rmdir(filePath);
    } else {
      // g_remove(filePath);
      // printf("remove file: %s\n", filePath);
    }

    free(filePath);
    g_object_unref(file);
  }

  g_object_unref(dirEnum);
  g_object_unref(dirFile);
}

static void syncDesToSrc(const char *desPath, const char *srcPath) {
  //  printf("srcPath: %s\n", srcPath);
  //  printf("desPath: %s\n", desPath);

  GFile *des = g_file_new_for_path(desPath);

  REQUIRE(g_file_query_exists(des, NULL));

  GError *err = NULL;
  GFileEnumerator *desEnum = g_file_enumerate_children(
      des, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, NULL, &err);

  ENSURE(err == NULL);

  while (1) {
    GFileInfo *file = g_file_enumerator_next_file(desEnum, NULL, &err);
    if (file == NULL) {
      break;
    }

    ENSURE(file != NULL);

    const char *fileName = g_file_info_get_name(file);
    char *desFilePath = pathJoin(desPath, fileName);
    char *srcFilePath = pathJoin(srcPath, fileName);

    GFileType fileType = g_file_info_get_file_type(file);
    gboolean srcExist = g_file_test(srcFilePath, G_FILE_TEST_EXISTS);

    if (fileType == G_FILE_TYPE_DIRECTORY) {
      if (!srcExist) {
        rmdirWithContents(desFilePath);
        // g_rmdir(desFilePath);
      } else {
        // struct SyncPath *pathSrcToDes = g_malloc(sizeof(struct SyncPath) *
        // 1);
        // REQUIRE(pathSrcToDes != NULL);
        // pathSrcToDes->src = srcPath;
        // pathSrcToDes->des = desPath;

        struct SyncPath *pathDesToSrc = g_malloc(sizeof(struct SyncPath) * 1);
        REQUIRE(pathDesToSrc != NULL);
        pathDesToSrc->src = desFilePath;
        pathDesToSrc->des = srcFilePath;

        // g_thread_pool_push(poolSrcToDes, pathSrcToDes, &err);
        // ENSURE(err == NULL);

        g_thread_pool_push(poolDesToSrc, pathDesToSrc, &err);
        ENSURE(err == NULL);
        // syncDesToSrc(desFilePath, srcFilePath);
      }
    } else {
      if (!srcExist) {
        // g_remove(desFilePath);
        // printf("remove file: %s\n", desFilePath);
      }
    }

    //  free(srcFilePath);
    //  free(desFilePath);
    g_object_unref(file);
  }

  g_object_unref(desEnum);
  g_object_unref(des);
}

static void threadFunc(void *pathData, void *func) {
  syncFunc *sync = (syncFunc *)func;
  struct SyncPath *path = (struct SyncPath *)pathData;

  sync(path->src, path->des);
}

#define MAX_NUM_THREADS 8

#define SYNC_PATH_FILE "sync_path.txt"

int main(const int argv, const char **args) {
  if (argv == 2 && (strcmp(args[1], "refresh") == 0)) {
    printf("refresh destination file!\n");
    diffFunc = fileDiffContents;
  } else if (argv > 1) {
    printf("unknow command: %s\nperforming usual operation\n", args[1]);
  }

  GError *err = NULL;
  GThreadPool *poolSrcToDes =
      g_thread_pool_new(threadFunc, syncSrctoDes, MAX_NUM_THREADS, TRUE, &err);
  ENSURE(err == NULL);

  GThreadPool *poolDesToSrc =
      g_thread_pool_new(threadFunc, syncDesToSrc, MAX_NUM_THREADS, TRUE, &err);
  ENSURE(err == NULL);

  char *syncPathContents = readFile(SYNC_PATH_FILE);
  // printf("contents:\n%s\n", syncPathContents);

  const char *newLine = "\n";
  const char *syncPathSplit = "->";

  char **syncNewLineSplit = g_strsplit(syncPathContents, newLine, 0);
  int i = 0;
  while (1) {
    if (syncNewLineSplit[i] == NULL) {
      break;
    }

    ENSURE(syncNewLineSplit[i] != NULL);

    char **splitSrcDes = g_strsplit(syncNewLineSplit[i], syncPathSplit, 0);

    if (splitSrcDes[0] == NULL || splitSrcDes[1] == NULL) {
      goto clean;
    }

    ENSURE(splitSrcDes[0] != NULL);
    ENSURE(splitSrcDes[1] != NULL);

    char *srcPath = g_strstrip(splitSrcDes[0]);
    char *desPath = g_strstrip(splitSrcDes[1]);

    REQUIRE(srcPath != NULL);
    REQUIRE(desPath != NULL);
    //
    //   char *srcPath =
    //      "/run/media/onionhuang/Internal_Disk/programming_testing_field/"
    //      "file_sync_test/01";
    //
    //   char *desPath =
    //      "/run/media/onionhuang/Internal_Disk/programming_testing_field/"
    //      "file_sync_test/02";

    // struct SyncPath pathSrcToDes = {srcPath, desPath};
    // struct SyncPath pathDesToSrc = {desPath, srcPath};

    struct SyncPath *pathSrcToDes = g_malloc(sizeof(struct SyncPath) * 1);
    REQUIRE(pathSrcToDes != NULL);
    pathSrcToDes->src = srcPath;
    pathSrcToDes->des = desPath;

    struct SyncPath *pathDesToSrc = g_malloc(sizeof(struct SyncPath) * 1);
    REQUIRE(pathDesToSrc != NULL);
    pathDesToSrc->src = desPath;
    pathDesToSrc->des = srcPath;

    g_thread_pool_push(poolSrcToDes, pathSrcToDes, &err);
    ENSURE(err == NULL);

    g_thread_pool_push(poolDesToSrc, pathDesToSrc, &err);
    ENSURE(err == NULL);

    printf("srcPath:\n%s\n", srcPath);
    printf("desPath:\n%s\n", desPath);

  clean:
    i++;
  }

  // char *srcPath =
  //    "/run/media/onionhuang/Internal_Disk/programming_testing_field/"
  //    "file_sync_test/01";

  // char *desPath =
  //    "/run/media/onionhuang/Internal_Disk/programming_testing_field/"
  //    "file_sync_test/02";

  //  struct SyncPath testPathSrcToDes = {srcPath, desPath};
  //  struct SyncPath testPathDesToSrc = {desPath, srcPath};
  //
  //  g_thread_pool_push(poolSrcToDes, &testPathSrcToDes, &err);
  //  ENSURE(err == NULL);
  //
  //  g_thread_pool_push(poolDesToSrc, &testPathDesToSrc, &err);
  //  ENSURE(err == NULL);

  g_thread_pool_free(poolSrcToDes, FALSE, TRUE);
  g_thread_pool_free(poolDesToSrc, FALSE, TRUE);

  g_strfreev(syncNewLineSplit);
  free(syncPathContents);

  //  syncSrctoDes(srcPath, desPath);
  //  syncDesToSrc(desPath, srcPath);

  return 0;
}
