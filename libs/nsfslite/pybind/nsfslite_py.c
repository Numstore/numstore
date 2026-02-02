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
 *   Implements nsfslite.h. Python extension module bindings for nsfslite, providing
 *   Python access to the NumStore File System Lite C library. Includes connection management,
 *   variable operations, and transaction support with proper memory handling and error propagation.
 */

#define PY_SSIZE_T_CLEAN

#include "nsfslite.h"

#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////
/// Utils

/* safely extract void pointer from PyObject */
static inline void *
extract_handle (PyObject *obj)
{
  if (!obj)
    {
      PyErr_SetString (PyExc_TypeError, "Expected a handle object, got NULL");
      return NULL;
    }
  void *ptr = PyLong_AsVoidPtr (obj);
  if (!ptr && PyErr_Occurred ())
    {
      return NULL;
    }
  return ptr;
}

////////////////////////////////////////////
/// Connection Management

static PyObject *
py_nsfslite_open (PyObject *Py_UNUSED (self), PyObject *args)
{
  const char *db_path;
  const char *wal_path;

  if (!PyArg_ParseTuple (args, "ss", &db_path, &wal_path))
    {
      return NULL;
    }

  nsfslite *handle = nsfslite_open (db_path, wal_path);
  if (!handle)
    {
      PyErr_SetString (PyExc_IOError, "Failed to open nsfslite database");
      return NULL;
    }

  PyObject *result = PyLong_FromVoidPtr (handle);
  if (!result)
    {
      nsfslite_close (handle);
      PyErr_SetString (PyExc_RuntimeError, "Failed to create Python handle object");
      return NULL;
    }

  return result;
}

static PyObject *
py_nsfslite_close (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *ptr_obj;

  if (!PyArg_ParseTuple (args, "O", &ptr_obj))
    {
      return NULL;
    }

  void *ptr = PyLong_AsVoidPtr (ptr_obj);
  if (!ptr && PyErr_Occurred ())
    {
      return NULL;
    }

  nsfslite *handle = (nsfslite *)ptr;
  if (handle)
    {
      nsfslite_close (handle);
    }

  Py_RETURN_NONE;
}

////////////////////////////////////////////
/// Variable Management

static PyObject *
nsfslite_create_variable (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  const char *name;

  if (!PyArg_ParseTuple (args, "Os", &conn_obj, &name))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  int64_t var_id = nsfslite_new (n, NULL, name);

  if (var_id < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to create variable");
      return NULL;
    }

  return PyLong_FromLongLong (var_id);
}

static PyObject *
nsfslite_get_variable (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  const char *name;

  if (!PyArg_ParseTuple (args, "Os", &conn_obj, &name))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  int64_t var_id = nsfslite_get_id (n, name);

  if (var_id < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Variable not found");
      return NULL;
    }

  return PyLong_FromLongLong (var_id);
}

static PyObject *
nsfslite_delete_variable (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  const char *name;

  if (!PyArg_ParseTuple (args, "Os", &conn_obj, &name))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  int result = nsfslite_delete (n, NULL, name);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to delete variable");
      return NULL;
    }

  Py_RETURN_NONE;
}

////////////////////////////////////////////
/// Variable Operations

static PyObject *
nsfslite_variable_len (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;

  if (!PyArg_ParseTuple (args, "OL", &conn_obj, &var_id))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  size_t size = nsfslite_fsize (n, (uint64_t)var_id);

  return PyLong_FromSize_t (size);
}

static PyObject *
nsfslite_variable_insert (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;
  long offset;
  Py_buffer data_buffer;

  if (!PyArg_ParseTuple (args, "OLly*", &conn_obj, &var_id, &offset, &data_buffer))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      PyBuffer_Release (&data_buffer);
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;

  ssize_t result = nsfslite_insert (n, (uint64_t)var_id, NULL,
                                    data_buffer.buf, (size_t)offset,
                                    1, (size_t)data_buffer.len);

  PyBuffer_Release (&data_buffer);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to insert data");
      return NULL;
    }

  Py_RETURN_NONE;
}

static PyObject *
nsfslite_variable_read (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;
  long start, stop, step;

  if (!PyArg_ParseTuple (args, "OLlll", &conn_obj, &var_id, &start, &stop, &step))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;

  // Calculate the number of elements to read
  long nelems = 0;
  if (step > 0 && stop > start)
    {
      nelems = (stop - start + step - 1) / step;
    }

  if (nelems <= 0)
    {
      return PyBytes_FromStringAndSize ("", 0);
    }

  // Allocate buffer for reading
  unsigned char *buffer = (unsigned char *)malloc ((size_t)nelems);
  if (!buffer)
    {
      PyErr_SetString (PyExc_MemoryError, "Failed to allocate read buffer");
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
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to read data");
      return NULL;
    }

  PyObject *ret = PyBytes_FromStringAndSize ((char *)buffer, nelems);
  free (buffer);

  if (!ret)
    {
      PyErr_SetString (PyExc_RuntimeError, "Failed to create bytes object");
      return NULL;
    }

  return ret;
}

static PyObject *
nsfslite_variable_write (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;
  long start, stop, step;
  Py_buffer data_buffer;

  if (!PyArg_ParseTuple (args, "OLllly*", &conn_obj, &var_id, &start, &stop, &step, &data_buffer))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      PyBuffer_Release (&data_buffer);
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;

  struct nsfslite_stride stride = {
    .bstart = (size_t)start,
    .stride = (size_t)step,
    .nelems = (size_t)data_buffer.len
  };

  ssize_t result = nsfslite_write (n, (uint64_t)var_id, NULL,
                                   data_buffer.buf, 1, stride);

  PyBuffer_Release (&data_buffer);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to write data");
      return NULL;
    }

  Py_RETURN_NONE;
}

static PyObject *
nsfslite_variable_remove (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;
  long start, stop, step;
  int return_data;

  if (!PyArg_ParseTuple (args, "OLlllp", &conn_obj, &var_id, &start, &stop, &step, &return_data))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;

  // Calculate the number of elements to remove
  long nelems = 0;
  if (step > 0 && stop > start)
    {
      nelems = (stop - start + step - 1) / step;
    }

  if (nelems <= 0)
    {
      return PyBytes_FromStringAndSize ("", 0);
    }

  unsigned char *buffer = NULL;
  if (return_data)
    {
      buffer = (unsigned char *)malloc ((size_t)nelems);
      if (!buffer)
        {
          PyErr_SetString (PyExc_MemoryError, "Failed to allocate remove buffer");
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
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to remove data");
      return NULL;
    }

  if (return_data && buffer)
    {
      PyObject *ret = PyBytes_FromStringAndSize ((char *)buffer, nelems);
      free (buffer);
      if (!ret)
        {
          PyErr_SetString (PyExc_RuntimeError, "Failed to create bytes object");
          return NULL;
        }
      return ret;
    }

  free (buffer);
  Py_RETURN_NONE;
}

////////////////////////////////////////////
/// Transaction Management

static PyObject *
nsfslite_begin_transaction (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;

  if (!PyArg_ParseTuple (args, "O", &conn_obj))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  nsfslite_txn *txn = nsfslite_begin_txn (n);

  if (!txn)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to begin transaction");
      return NULL;
    }

  return PyLong_FromVoidPtr (txn);
}

static PyObject *
nsfslite_commit_transaction (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  PyObject *txn_obj;

  if (!PyArg_ParseTuple (args, "OO", &conn_obj, &txn_obj))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  void *txn_ptr = extract_handle (txn_obj);
  if (!txn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  nsfslite_txn *txn = (nsfslite_txn *)txn_ptr;

  int result = nsfslite_commit (n, txn);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to commit transaction");
      return NULL;
    }

  Py_RETURN_NONE;
}

static PyObject *
nsfslite_variable_insert_tx (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;
  PyObject *txn_obj;
  long offset;
  Py_buffer data_buffer;

  if (!PyArg_ParseTuple (args, "OLOly*", &conn_obj, &var_id, &txn_obj, &offset, &data_buffer))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      PyBuffer_Release (&data_buffer);
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  nsfslite_txn *txn = NULL;

  if (txn_obj != Py_None)
    {
      txn = (nsfslite_txn *)extract_handle (txn_obj);
      if (!txn)
        {
          PyBuffer_Release (&data_buffer);
          return NULL;
        }
    }

  ssize_t result = nsfslite_insert (n, (uint64_t)var_id, txn,
                                    data_buffer.buf, (size_t)offset,
                                    1, (size_t)data_buffer.len);

  PyBuffer_Release (&data_buffer);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to insert data");
      return NULL;
    }

  Py_RETURN_NONE;
}

static PyObject *
nsfslite_variable_write_tx (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;
  PyObject *txn_obj;
  long start, stop, step;
  Py_buffer data_buffer;

  if (!PyArg_ParseTuple (args, "OLOllly*", &conn_obj, &var_id, &txn_obj, &start, &stop, &step, &data_buffer))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      PyBuffer_Release (&data_buffer);
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  nsfslite_txn *txn = NULL;

  if (txn_obj != Py_None)
    {
      txn = (nsfslite_txn *)extract_handle (txn_obj);
      if (!txn)
        {
          PyBuffer_Release (&data_buffer);
          return NULL;
        }
    }

  struct nsfslite_stride stride = {
    .bstart = (size_t)start,
    .stride = (size_t)step,
    .nelems = (size_t)data_buffer.len
  };

  ssize_t result = nsfslite_write (n, (uint64_t)var_id, txn,
                                   data_buffer.buf, 1, stride);

  PyBuffer_Release (&data_buffer);

  if (result < 0)
    {
      const char *error = nsfslite_error (n);
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to write data");
      return NULL;
    }

  Py_RETURN_NONE;
}

static PyObject *
nsfslite_variable_remove_tx (PyObject *Py_UNUSED (self), PyObject *args)
{
  PyObject *conn_obj;
  long long var_id;
  PyObject *txn_obj;
  long start, stop, step;
  int return_data;

  if (!PyArg_ParseTuple (args, "OLOlllp", &conn_obj, &var_id, &txn_obj, &start, &stop, &step, &return_data))
    {
      return NULL;
    }

  void *conn_ptr = extract_handle (conn_obj);
  if (!conn_ptr)
    {
      return NULL;
    }

  nsfslite *n = (nsfslite *)conn_ptr;
  nsfslite_txn *txn = NULL;

  if (txn_obj != Py_None)
    {
      txn = (nsfslite_txn *)extract_handle (txn_obj);
      if (!txn)
        {
          return NULL;
        }
    }

  // Calculate the number of elements to remove
  long nelems = 0;
  if (step > 0 && stop > start)
    {
      nelems = (stop - start + step - 1) / step;
    }

  if (nelems <= 0)
    {
      return PyBytes_FromStringAndSize ("", 0);
    }

  unsigned char *buffer = NULL;
  if (return_data)
    {
      buffer = (unsigned char *)malloc ((size_t)nelems);
      if (!buffer)
        {
          PyErr_SetString (PyExc_MemoryError, "Failed to allocate remove buffer");
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
      PyErr_SetString (PyExc_IOError, error ? error : "Failed to remove data");
      return NULL;
    }

  if (return_data && buffer)
    {
      PyObject *ret = PyBytes_FromStringAndSize ((char *)buffer, nelems);
      free (buffer);
      if (!ret)
        {
          PyErr_SetString (PyExc_RuntimeError, "Failed to create bytes object");
          return NULL;
        }
      return ret;
    }

  free (buffer);
  Py_RETURN_NONE;
}

////////////////////////////////////////////
/// Module Bindings

static PyMethodDef NsfsliteMethods[] = {
  { "open", py_nsfslite_open, METH_VARARGS, "Open a database connection" },
  { "close", py_nsfslite_close, METH_VARARGS, "Close a database connection" },
  { "create_variable", nsfslite_create_variable, METH_VARARGS, "Create a new variable" },
  { "get_variable", nsfslite_get_variable, METH_VARARGS, "Get an existing variable" },
  { "delete_variable", nsfslite_delete_variable, METH_VARARGS, "Delete a variable" },
  { "variable_len", nsfslite_variable_len, METH_VARARGS, "Get variable length" },
  { "variable_insert", nsfslite_variable_insert, METH_VARARGS, "Insert data into variable" },
  { "variable_read", nsfslite_variable_read, METH_VARARGS, "Read data from variable" },
  { "variable_write", nsfslite_variable_write, METH_VARARGS, "Write data to variable" },
  { "variable_remove", nsfslite_variable_remove, METH_VARARGS, "Remove data from variable" },
  { "begin_transaction", nsfslite_begin_transaction, METH_VARARGS, "Begin a transaction" },
  { "commit_transaction", nsfslite_commit_transaction, METH_VARARGS, "Commit a transaction" },
  { "variable_insert_tx", nsfslite_variable_insert_tx, METH_VARARGS, "Insert data with transaction" },
  { "variable_write_tx", nsfslite_variable_write_tx, METH_VARARGS, "Write data with transaction" },
  { "variable_remove_tx", nsfslite_variable_remove_tx, METH_VARARGS, "Remove data with transaction" },
  { NULL, NULL, 0, NULL }
};

static struct PyModuleDef nsfslitemodule = {
  PyModuleDef_HEAD_INIT,
  "_nsfslite",
  "NumStore File System Lite C extension module",
  -1,
  NsfsliteMethods,
  NULL, /* m_slots */
  NULL, /* m_traverse */
  NULL, /* m_clear */
  NULL  /* m_free */
};

PyMODINIT_FUNC PyInit__nsfslite (void);

PyMODINIT_FUNC
PyInit__nsfslite (void)
{
  return PyModule_Create (&nsfslitemodule);
}
