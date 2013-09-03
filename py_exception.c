#include "include/common.h"

void
process_python_exception(void)
{
  char buf[512], *buf_p = buf;
  PyObject *type_obj, *value_obj, *traceback_obj;
  PyErr_Fetch(&type_obj, &value_obj, &traceback_obj);
  if (value_obj == NULL)
    return;

  if (!PyString_Check(value_obj))
    return;

  char *value = PyString_AsString(value_obj);
  size_t szbuf = sizeof(buf);
  int l;
  PyCodeObject *codeobj;

  l = snprintf(buf_p, szbuf, "Error Message:\n%s", value);
  buf_p += l;
  szbuf -= l;

  if (traceback_obj != NULL) {
    l = snprintf(buf_p, szbuf, "\n\nTraceback:\n");
    buf_p += l;
    szbuf -= l;

    PyTracebackObject *traceback = (PyTracebackObject *)traceback_obj;
    for (;traceback && szbuf > 0; traceback = traceback->tb_next) {
      codeobj = (PyCodeObject *)traceback->tb_frame->f_code;
      l = snprintf(buf_p, szbuf, "%s: %s(# %d)\n",
                   PyString_AsString(codeobj->co_name),
                   PyString_AsString(codeobj->co_filename),
                   traceback->tb_lineno);
      buf_p += l;
      szbuf -= l;
    }
  }

  fprintf(stderr, "%s\n", buf);

  Py_XDECREF(type_obj);
  Py_XDECREF(value_obj);
  Py_XDECREF(traceback_obj);
}