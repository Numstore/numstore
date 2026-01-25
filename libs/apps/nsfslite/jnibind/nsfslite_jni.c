/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Implements nsfslite.h. JNI (Java Native Interface) bindings for nsfslite, providing
 *   Java access to the NumStore File System Lite C library. Includes connection management,
 *   variable operations, and transaction support with proper error handling and type conversion.
 */

#include "nsfslite.h"

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////
/// Utility Functions

/* Convert Java string to C string */
static inline char *
jstring_to_cstr (JNIEnv *env, jstring jstr)
{
  if (!jstr)
    {
      return NULL;
    }
  const char *str = (*env)->GetStringUTFChars (env, jstr, NULL);
  if (!str)
    {
      return NULL;
    }
  char *result = strdup (str);
  (*env)->ReleaseStringUTFChars (env, jstr, str);
  return result;
}

/* Throw a Java exception */
static void
throw_exception (JNIEnv *env, const char *exception_class, const char *message)
{
  jclass exc_class = (*env)->FindClass (env, exception_class);
  if (exc_class)
    {
      (*env)->ThrowNew (env, exc_class, message);
      (*env)->DeleteLocalRef (env, exc_class);
    }
}

////////////////////////////////////////////
/// Connection Management

JNIEXPORT jlong JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_open (JNIEnv *env, jclass cls,
                                                jstring db_path, jstring wal_path)
{
  char *db_path_str = jstring_to_cstr (env, db_path);
  char *wal_path_str = jstring_to_cstr (env, wal_path);

  if (!db_path_str || !wal_path_str)
    {
      free (db_path_str);
      free (wal_path_str);
      throw_exception (env, "java/lang/IllegalArgumentException",
                       "Invalid path argument");
      return 0;
    }

  printf ("[nsfslite-jni] Opening database: %s with WAL: %s\n", db_path_str,
          wal_path_str);

  nsfslite *handle = nsfslite_open (db_path_str, wal_path_str);

  free (db_path_str);
  free (wal_path_str);

  if (!handle)
    {
      throw_exception (env, "java/io/IOException",
                       "Failed to open nsfslite database");
      return 0;
    }

  return (jlong)(intptr_t)handle;
}

JNIEXPORT void JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_close (JNIEnv *env, jclass cls,
                                                 jlong handle)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  if (n)
    {
      printf ("[nsfslite-jni] Closing connection\n");
      nsfslite_close (n);
    }
}

////////////////////////////////////////////
/// Variable Management

JNIEXPORT jlong JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_createVariable (JNIEnv *env, jclass cls,
                                                          jlong handle, jstring name)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  char *name_str = jstring_to_cstr (env, name);

  if (!name_str)
    {
      throw_exception (env, "java/lang/IllegalArgumentException",
                       "Invalid variable name");
      return -1;
    }

  printf ("[nsfslite-jni] Creating variable '%s'\n", name_str);

  int64_t var_id = nsfslite_new (n, NULL, name_str);

  free (name_str);

  if (var_id < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to create variable");
      return -1;
    }

  return (jlong)var_id;
}

JNIEXPORT jlong JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_getVariable (JNIEnv *env, jclass cls,
                                                       jlong handle, jstring name)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  char *name_str = jstring_to_cstr (env, name);

  if (!name_str)
    {
      throw_exception (env, "java/lang/IllegalArgumentException",
                       "Invalid variable name");
      return -1;
    }

  printf ("[nsfslite-jni] Getting variable '%s'\n", name_str);

  int64_t var_id = nsfslite_get_id (n, name_str);

  free (name_str);

  if (var_id < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Variable not found");
      return -1;
    }

  return (jlong)var_id;
}

JNIEXPORT void JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_deleteVariable (JNIEnv *env, jclass cls,
                                                          jlong handle, jstring name)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  char *name_str = jstring_to_cstr (env, name);

  if (!name_str)
    {
      throw_exception (env, "java/lang/IllegalArgumentException",
                       "Invalid variable name");
      return;
    }

  printf ("[nsfslite-jni] Deleting variable '%s'\n", name_str);

  int result = nsfslite_delete (n, NULL, name_str);

  free (name_str);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to delete variable");
    }
}

JNIEXPORT jlong JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_variableLength (JNIEnv *env, jclass cls,
                                                          jlong handle, jlong var_id)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;

  printf ("[nsfslite-jni] Getting length of variable ID %ld\n", (long)var_id);

  size_t size = nsfslite_fsize (n, (uint64_t)var_id);

  return (jlong)size;
}

////////////////////////////////////////////
/// Variable Operations

JNIEXPORT void JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_insert (JNIEnv *env, jclass cls,
                                                  jlong handle, jlong var_id,
                                                  jlong offset, jbyteArray data)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;

  jsize data_len = (*env)->GetArrayLength (env, data);
  jbyte *data_bytes = (*env)->GetByteArrayElements (env, data, NULL);

  if (!data_bytes)
    {
      throw_exception (env, "java/lang/OutOfMemoryError",
                       "Failed to get byte array elements");
      return;
    }

  printf ("[nsfslite-jni] Inserting %d bytes at offset %ld into variable ID %ld\n",
          data_len, (long)offset, (long)var_id);

  ssize_t result = nsfslite_insert (n, (uint64_t)var_id, NULL,
                                    (const void *)data_bytes, (size_t)offset,
                                    1, (size_t)data_len);

  (*env)->ReleaseByteArrayElements (env, data, data_bytes, JNI_ABORT);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to insert data");
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_read (JNIEnv *env, jclass cls,
                                                jlong handle, jlong var_id,
                                                jlong start, jlong stop, jlong step)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;

  // Calculate the number of elements to read
  long nelems = 0;
  if (step > 0 && stop > start)
    {
      nelems = (stop - start + step - 1) / step;
    }

  if (nelems <= 0)
    {
      // Return empty byte array
      return (*env)->NewByteArray (env, 0);
    }

  printf ("[nsfslite-jni] Reading variable ID %ld [%ld:%ld:%ld] -> %ld elements\n",
          (long)var_id, (long)start, (long)stop, (long)step, nelems);

  // Allocate buffer for reading
  unsigned char *buffer = (unsigned char *)malloc ((size_t)nelems);
  if (!buffer)
    {
      throw_exception (env, "java/lang/OutOfMemoryError",
                       "Failed to allocate read buffer");
      return NULL;
    }

  struct nsfslite_stride stride = {
    .bstart = (size_t)start,
    .stride = (size_t)step,
    .nelems = (size_t)nelems
  };

  ssize_t result = nsfslite_read (n, (uint64_t)var_id, buffer, 1, stride);

  if (result < 0)
    {
      free (buffer);
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to read data");
      return NULL;
    }

  // Create Java byte array
  jbyteArray ret = (*env)->NewByteArray (env, (jsize)nelems);
  if (!ret)
    {
      free (buffer);
      throw_exception (env, "java/lang/OutOfMemoryError",
                       "Failed to create byte array");
      return NULL;
    }

  (*env)->SetByteArrayRegion (env, ret, 0, (jsize)nelems, (jbyte *)buffer);
  free (buffer);

  return ret;
}

JNIEXPORT void JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_write (JNIEnv *env, jclass cls,
                                                 jlong handle, jlong var_id,
                                                 jlong start, jlong stop, jlong step,
                                                 jbyteArray data)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;

  jsize data_len = (*env)->GetArrayLength (env, data);
  jbyte *data_bytes = (*env)->GetByteArrayElements (env, data, NULL);

  if (!data_bytes)
    {
      throw_exception (env, "java/lang/OutOfMemoryError",
                       "Failed to get byte array elements");
      return;
    }

  printf ("[nsfslite-jni] Writing %d bytes to variable ID %ld [%ld:%ld:%ld]\n",
          data_len, (long)var_id, (long)start, (long)stop, (long)step);

  struct nsfslite_stride stride = {
    .bstart = (size_t)start,
    .stride = (size_t)step,
    .nelems = (size_t)data_len
  };

  ssize_t result = nsfslite_write (n, (uint64_t)var_id, NULL,
                                   (const void *)data_bytes, 1, stride);

  (*env)->ReleaseByteArrayElements (env, data, data_bytes, JNI_ABORT);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to write data");
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_remove (JNIEnv *env, jclass cls,
                                                  jlong handle, jlong var_id,
                                                  jlong start, jlong stop, jlong step,
                                                  jboolean return_data)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;

  // Calculate the number of elements to remove
  long nelems = 0;
  if (step > 0 && stop > start)
    {
      nelems = (stop - start + step - 1) / step;
    }

  if (nelems <= 0)
    {
      // Return empty byte array
      return (*env)->NewByteArray (env, 0);
    }

  printf ("[nsfslite-jni] Removing from variable ID %ld [%ld:%ld:%ld] -> %ld elements (return=%d)\n",
          (long)var_id, (long)start, (long)stop, (long)step, nelems, return_data);

  unsigned char *buffer = NULL;
  if (return_data)
    {
      buffer = (unsigned char *)malloc ((size_t)nelems);
      if (!buffer)
        {
          throw_exception (env, "java/lang/OutOfMemoryError",
                           "Failed to allocate remove buffer");
          return NULL;
        }
    }

  struct nsfslite_stride stride = {
    .bstart = (size_t)start,
    .stride = (size_t)step,
    .nelems = (size_t)nelems
  };

  ssize_t result = nsfslite_remove (n, (uint64_t)var_id, NULL, buffer, 1, stride);

  if (result < 0)
    {
      free (buffer);
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to remove data");
      return NULL;
    }

  if (return_data && buffer)
    {
      // Create Java byte array with removed data
      jbyteArray ret = (*env)->NewByteArray (env, (jsize)nelems);
      if (!ret)
        {
          free (buffer);
          throw_exception (env, "java/lang/OutOfMemoryError",
                           "Failed to create byte array");
          return NULL;
        }

      (*env)->SetByteArrayRegion (env, ret, 0, (jsize)nelems, (jbyte *)buffer);
      free (buffer);

      return ret;
    }

  free (buffer);
  return (*env)->NewByteArray (env, 0);
}

////////////////////////////////////////////
/// Variable Operations with Transaction Support

JNIEXPORT void JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_insertTx (JNIEnv *env, jclass cls,
                                                    jlong handle, jlong var_id,
                                                    jlong txn_handle, jlong offset,
                                                    jbyteArray data)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  nsfslite_txn *txn = (nsfslite_txn *)(intptr_t)txn_handle;

  jsize data_len = (*env)->GetArrayLength (env, data);
  jbyte *data_bytes = (*env)->GetByteArrayElements (env, data, NULL);

  if (!data_bytes)
    {
      throw_exception (env, "java/lang/OutOfMemoryError",
                       "Failed to get byte array elements");
      return;
    }

  printf ("[nsfslite-jni] Inserting %d bytes at offset %ld into variable ID %ld (with txn)\n",
          data_len, (long)offset, (long)var_id);

  ssize_t result = nsfslite_insert (n, (uint64_t)var_id, txn,
                                    (const void *)data_bytes, (size_t)offset,
                                    1, (size_t)data_len);

  (*env)->ReleaseByteArrayElements (env, data, data_bytes, JNI_ABORT);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to insert data");
    }
}

JNIEXPORT void JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_writeTx (JNIEnv *env, jclass cls,
                                                   jlong handle, jlong var_id,
                                                   jlong txn_handle, jlong start,
                                                   jlong stop, jlong step,
                                                   jbyteArray data)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  nsfslite_txn *txn = (nsfslite_txn *)(intptr_t)txn_handle;

  jsize data_len = (*env)->GetArrayLength (env, data);
  jbyte *data_bytes = (*env)->GetByteArrayElements (env, data, NULL);

  if (!data_bytes)
    {
      throw_exception (env, "java/lang/OutOfMemoryError",
                       "Failed to get byte array elements");
      return;
    }

  printf ("[nsfslite-jni] Writing %d bytes to variable ID %ld [%ld:%ld:%ld] (with txn)\n",
          data_len, (long)var_id, (long)start, (long)stop, (long)step);

  struct nsfslite_stride stride = {
    .bstart = (size_t)start,
    .stride = (size_t)step,
    .nelems = (size_t)data_len
  };

  ssize_t result = nsfslite_write (n, (uint64_t)var_id, txn,
                                   (const void *)data_bytes, 1, stride);

  (*env)->ReleaseByteArrayElements (env, data, data_bytes, JNI_ABORT);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to write data");
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_removeTx (JNIEnv *env, jclass cls,
                                                    jlong handle, jlong var_id,
                                                    jlong txn_handle, jlong start,
                                                    jlong stop, jlong step,
                                                    jboolean return_data)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  nsfslite_txn *txn = (nsfslite_txn *)(intptr_t)txn_handle;

  // Calculate the number of elements to remove
  long nelems = 0;
  if (step > 0 && stop > start)
    {
      nelems = (stop - start + step - 1) / step;
    }

  if (nelems <= 0)
    {
      // Return empty byte array
      return (*env)->NewByteArray (env, 0);
    }

  printf ("[nsfslite-jni] Removing from variable ID %ld [%ld:%ld:%ld] -> %ld elements (return=%d, with txn)\n",
          (long)var_id, (long)start, (long)stop, (long)step, nelems, return_data);

  unsigned char *buffer = NULL;
  if (return_data)
    {
      buffer = (unsigned char *)malloc ((size_t)nelems);
      if (!buffer)
        {
          throw_exception (env, "java/lang/OutOfMemoryError",
                           "Failed to allocate remove buffer");
          return NULL;
        }
    }

  struct nsfslite_stride stride = {
    .bstart = (size_t)start,
    .stride = (size_t)step,
    .nelems = (size_t)nelems
  };

  ssize_t result = nsfslite_remove (n, (uint64_t)var_id, txn, buffer, 1, stride);

  if (result < 0)
    {
      free (buffer);
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to remove data");
      return NULL;
    }

  if (return_data && buffer)
    {
      // Create Java byte array with removed data
      jbyteArray ret = (*env)->NewByteArray (env, (jsize)nelems);
      if (!ret)
        {
          free (buffer);
          throw_exception (env, "java/lang/OutOfMemoryError",
                           "Failed to create byte array");
          return NULL;
        }

      (*env)->SetByteArrayRegion (env, ret, 0, (jsize)nelems, (jbyte *)buffer);
      free (buffer);

      return ret;
    }

  free (buffer);
  return (*env)->NewByteArray (env, 0);
}

////////////////////////////////////////////
/// Transaction Management

JNIEXPORT jlong JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_beginTransaction (JNIEnv *env, jclass cls,
                                                            jlong handle)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;

  printf ("[nsfslite-jni] Beginning transaction\n");

  nsfslite_txn *txn = nsfslite_begin_txn (n);

  if (!txn)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to begin transaction");
      return 0;
    }

  return (jlong)(intptr_t)txn;
}

JNIEXPORT void JNICALL
Java_com_numstore_nsfslite_NsfsliteNative_commitTransaction (JNIEnv *env, jclass cls,
                                                             jlong handle, jlong txn_handle)
{
  nsfslite *n = (nsfslite *)(intptr_t)handle;
  nsfslite_txn *txn = (nsfslite_txn *)(intptr_t)txn_handle;

  printf ("[nsfslite-jni] Committing transaction\n");

  int result = nsfslite_commit (n, txn);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      throw_exception (env, "java/io/IOException",
                       error ? error : "Failed to commit transaction");
    }
}
