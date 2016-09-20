#include <glib-2.0/gio/gio.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/glib/gprintf.h>
#include <glib-2.0/glib/gstdio.h>

#include <stdio.h>
#include <string.h>

#include <omp.h>

#include "../../general/src/general_helper.h"

//#include <cmockery/pbc.h>
#include "../../general/src/debug_macro.h"

#ifdef UNIT_TESTING
#include <cmockery/cmockery_override.h>
#endif

#define MIN_NUM_THREADS 1
#define MAX_NUM_THREADS 8

#define SYNC_PATH_FILE "sync_path.txt"

#define NEW_LINE_CHAR "\n"
#define SYNC_PATH_SPLIT "->"

#ifdef DEBUG

static gboolean inSyncPath(const char *path) {
  gboolean re = FALSE;

  REQUIRE(g_file_test(SYNC_PATH_FILE, G_FILE_TEST_EXISTS));

  char *syncPathContents = readFile(SYNC_PATH_FILE);

  ENSURE(syncPathContents != NULL);

  char **syncNewLineSplit = g_strsplit(syncPathContents, NEW_LINE_CHAR, 0);

  ENSURE(syncNewLineSplit != NULL);

  int i = 0;
  while (1) {
    if (syncNewLineSplit[i] == NULL) {
      break;
    }

    ENSURE(syncNewLineSplit[i] != NULL);

    char **splitSrcDes = g_strsplit(syncNewLineSplit[i], SYNC_PATH_SPLIT, 0);

    if (splitSrcDes[0] == NULL || splitSrcDes[1] == NULL) {
      goto clean;
    }

    ENSURE(splitSrcDes[0] != NULL);
    ENSURE(splitSrcDes[1] != NULL);

    char *srcPath = g_strstrip(splitSrcDes[0]);
    char *desPath = g_strstrip(splitSrcDes[1]);

    ENSURE(path != NULL);

    ENSURE(srcPath != NULL);
    ENSURE(desPath != NULL);

    ENSURE(g_file_test(srcPath, G_FILE_TEST_EXISTS));
    ENSURE(g_file_test(desPath, G_FILE_TEST_EXISTS));

    if (g_str_match_string(srcPath, path, TRUE) ||
        g_str_match_string(desPath, path, TRUE)) {
      re = TRUE;
      break;
    }

  clean:
    i++;
  }

  return re;
}

#endif

static gboolean fileDiffTime(const char *src, const char *des) {
  REQUIRE(src != NULL);
  REQUIRE(des != NULL);

  REQUIRE(g_file_test(src, G_FILE_TEST_EXISTS));
  REQUIRE(g_file_test(des, G_FILE_TEST_EXISTS));

  REQUIRE(inSyncPath(src));
  REQUIRE(inSyncPath(des));

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

static gboolean fileDiffRefresh(const char *src, const char *des) {
  REQUIRE(src != NULL);
  REQUIRE(des != NULL);

  REQUIRE(g_file_test(src, G_FILE_TEST_EXISTS));
  REQUIRE(g_file_test(des, G_FILE_TEST_EXISTS));

  REQUIRE(inSyncPath(src));
  REQUIRE(inSyncPath(des));

  return TRUE;
}

typedef gboolean fileDiffFunc(const char *src, const char *des);

static fileDiffFunc *diffFunc = fileDiffTime;

static void syncSrctoDes(const char *srcPath, const char *desPath) {
  REQUIRE(srcPath != NULL);
  REQUIRE(desPath != NULL);

  REQUIRE(g_file_test(srcPath, G_FILE_TEST_EXISTS));
  REQUIRE(g_file_test(desPath, G_FILE_TEST_EXISTS));

  REQUIRE(inSyncPath(srcPath));
  REQUIRE(inSyncPath(desPath));

  GFile *src = g_file_new_for_path(srcPath);

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

    ENSURE(fileName != NULL);

    char *srcFilePath = pathJoin(srcPath, fileName);
    char *desFilePath = pathJoin(desPath, fileName);

    ENSURE(srcFilePath != NULL);
    ENSURE(desFilePath != NULL);

    GFileType fileType = g_file_info_get_file_type(file);
    gboolean desExist = g_file_test(desFilePath, G_FILE_TEST_EXISTS);

    if (fileType == G_FILE_TYPE_DIRECTORY) {
      if (!desExist) {
        REQUIRE(inSyncPath(desFilePath));

        g_mkdir(desFilePath, 0777);
        printf("make directory:\n%s\n\n", desFilePath);
      }

      ENSURE(g_file_test(srcFilePath, G_FILE_TEST_EXISTS));
      ENSURE(g_file_test(desFilePath, G_FILE_TEST_EXISTS));

      REQUIRE(inSyncPath(srcFilePath));
      REQUIRE(inSyncPath(desFilePath));

      syncSrctoDes(srcFilePath, desFilePath);

    } else {
      if ((!desExist) || diffFunc(srcFilePath, desFilePath)) {
        REQUIRE(inSyncPath(srcFilePath));
        REQUIRE(inSyncPath(desFilePath));

        copy_file(srcFilePath, desFilePath);
        printf("copy file:\nFrom: %s\nTo: %s\n\n", srcFilePath, desFilePath);
      }
    }

    g_object_unref(file);
  }

  g_object_unref(srcEnum);
  g_object_unref(src);
}

static void rmdirWithContents(const char *dir) {
  REQUIRE(dir != NULL);
  REQUIRE(g_file_test(dir, G_FILE_TEST_EXISTS));

  REQUIRE(inSyncPath(dir));

  GFile *dirFile = g_file_new_for_path(dir);

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
    ENSURE(fileName != NULL);

    char *filePath = pathJoin(dir, fileName);
    ENSURE(filePath != NULL);

    GFileType fileType = g_file_info_get_file_type(file);

    if (fileType == G_FILE_TYPE_DIRECTORY) {
      REQUIRE(inSyncPath(filePath));

      rmdirWithContents(filePath);
      g_rmdir(filePath);
      printf("remove directory:\n%s\n\n", filePath);
    } else {
      REQUIRE(inSyncPath(filePath));

      g_remove(filePath);
      printf("remove file:\n%s\n\n", filePath);
    }

    free(filePath);
    g_object_unref(file);
  }

  g_object_unref(dirEnum);
  g_object_unref(dirFile);
}

static void syncDesToSrc(const char *desPath, const char *srcPath) {
  REQUIRE(srcPath != NULL);
  REQUIRE(desPath != NULL);

  REQUIRE(g_file_test(srcPath, G_FILE_TEST_EXISTS));
  REQUIRE(g_file_test(desPath, G_FILE_TEST_EXISTS));

  REQUIRE(inSyncPath(srcPath));
  REQUIRE(inSyncPath(desPath));

  GFile *des = g_file_new_for_path(desPath);

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
    ENSURE(fileName != NULL);

    char *desFilePath = pathJoin(desPath, fileName);
    char *srcFilePath = pathJoin(srcPath, fileName);

    ENSURE(srcFilePath != NULL);
    ENSURE(desFilePath != NULL);

    GFileType fileType = g_file_info_get_file_type(file);
    gboolean srcExist = g_file_test(srcFilePath, G_FILE_TEST_EXISTS);

    if (fileType == G_FILE_TYPE_DIRECTORY) {
      if (!srcExist) {
        rmdirWithContents(desFilePath);
        g_rmdir(desFilePath);
        printf("remove directory:\n%s\n\n", desFilePath);
      } else {
        ENSURE(g_file_test(srcFilePath, G_FILE_TEST_EXISTS));
        ENSURE(g_file_test(desFilePath, G_FILE_TEST_EXISTS));

        REQUIRE(inSyncPath(srcFilePath));
        REQUIRE(inSyncPath(desFilePath));

        syncDesToSrc(desFilePath, srcFilePath);
      }
    } else {
      if (!srcExist) {
        REQUIRE(inSyncPath(desFilePath));

        g_remove(desFilePath);
        printf("remove file:\n%s\n\n", desFilePath);
      }
    }

    g_object_unref(file);
  }

  g_object_unref(desEnum);
  g_object_unref(des);
}

int main(const int argv, const char **args) {
  omp_set_num_threads(MIN_NUM_THREADS);

  if (argv == 2 && (strcmp(args[1], "refresh") == 0)) {
    printf("refresh destination files!\n");
    diffFunc = fileDiffRefresh;

    omp_set_num_threads(MAX_NUM_THREADS);
  } else if (argv == 2 && (strcmp(args[1], "max") == 0)) {
    printf("copy files with max threads!\n");
    omp_set_num_threads(MAX_NUM_THREADS);
  } else if (argv > 1) {
    printf("unknow command: %s\nperforming usual operation\n", args[1]);
  }

  GTimer *timer = g_timer_new();
  g_timer_start(timer);

  REQUIRE(g_file_test(SYNC_PATH_FILE, G_FILE_TEST_EXISTS));

  char *syncPathContents = readFile(SYNC_PATH_FILE);

  ENSURE(syncPathContents != NULL);

  char **syncNewLineSplit = g_strsplit(syncPathContents, NEW_LINE_CHAR, 0);

  ENSURE(syncNewLineSplit != NULL);

#pragma omp parallel
  {
    int i = 0;
    while (1) {
      if (syncNewLineSplit[i] == NULL) {
        break;
      }

      ENSURE(syncNewLineSplit[i] != NULL);

      char **splitSrcDes = g_strsplit(syncNewLineSplit[i], SYNC_PATH_SPLIT, 0);

      if (splitSrcDes[0] == NULL || splitSrcDes[1] == NULL) {
        goto clean;
      }

      ENSURE(splitSrcDes[0] != NULL);
      ENSURE(splitSrcDes[1] != NULL);

      char *srcPath = g_strstrip(splitSrcDes[0]);
      char *desPath = g_strstrip(splitSrcDes[1]);

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

      ENSURE(g_file_test(srcPath, G_FILE_TEST_EXISTS));
      ENSURE(g_file_test(desPath, G_FILE_TEST_EXISTS));

      ENSURE(inSyncPath(srcPath));
      ENSURE(inSyncPath(desPath));

#pragma omp task
      {
        syncSrctoDes(srcPath, desPath);
        syncDesToSrc(desPath, srcPath);
      }

    clean:
      i++;
    }
  }

  g_strfreev(syncNewLineSplit);
  free(syncPathContents);

  g_timer_stop(timer);
  double executionTime = g_timer_elapsed(timer, NULL);

  printf("execution time: %f seconds\n", executionTime);

  g_timer_destroy(timer);

  //  syncSrctoDes(srcPath, desPath);
  //  syncDesToSrc(desPath, srcPath);

  return 0;
}
