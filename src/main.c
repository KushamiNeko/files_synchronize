#include <glib-2.0/gio/gio.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/glib/gstdio.h>

#include <stdio.h>
#include <string.h>

#include "../../general/src/general_helper.h"

#include <cmockery/pbc.h>

#ifdef UNIT_TESTING
#include <cmockery/cmockery_override.h>
#endif

static const char *srcPath =
    "/run/media/onionhuang/Internal_Disk/programming_testing_field/"
    "file_sync_test/01";

static const char *desPath =
    "/run/media/onionhuang/Internal_Disk/programming_testing_field/"
    "file_sync_test/02";

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

  if ((srcLength != desLength) || (strcmp(srcContents, desContents) == 0)) {
    re = TRUE;
  }

  g_free(srcContents);
  g_free(desContents);

  return re;
}

typedef gboolean fileDiffFunc(const char *src, const char *des);
static fileDiffFunc *diffFunc = fileDiffTime;

static void syncSrctoDes(const char *srcPath, const char *desPath) {
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

      syncSrctoDes(srcFilePath, desFilePath);

    } else {
      // if ((!desExist) || fileDiffTime(srcFilePath, desFilePath)) {
      //  copy_file(srcFilePath, desFilePath);
      //}

      // if ((!desExist) || fileDiffContents(srcFilePath, desFilePath)) {
      //  copy_file(srcFilePath, desFilePath);
      //}

      if ((!desExist) || diffFunc(srcFilePath, desFilePath)) {
        copy_file(srcFilePath, desFilePath);
      }
    }

    free(srcFilePath);
    free(desFilePath);
    g_object_unref(file);
  }

  g_object_unref(srcEnum);
  g_object_unref(src);
}

int main(int argv, const char **args) {
  diffFunc = fileDiffContents;
  syncSrctoDes(srcPath, desPath);

  return 0;
}
