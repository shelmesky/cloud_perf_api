#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <python2.7/Python.h>

PyThreadState *mainThreadState = NULL;


void *run(void *);
char * PyCall(const char *, const char *, const char *, ... );

char * PyCall(const char * module, const char *func, const char *format, ... ) {
    PyObject *pModule = NULL;
    PyObject *pFunc = NULL;
    PyObject *pParm = NULL;
    PyObject *pRetVal = NULL;

    pModule = PyImport_ImportModule(module);
    if(pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, func);
        if(pFunc != NULL) {
            va_list vargs;
            va_start(vargs, format);
            pParm = Py_VaBuildValue(format, vargs);
            va_end(vargs);

            pRetVal = PyEval_CallObject(pFunc, pParm);

            char *ret;
            PyArg_Parse(pRetVal, "s", &ret);
            return ret;
        }
        else {
            printf("import function error\n");
        }
    }
    else {
        printf("import module error\n");
    }
}


void *run(void *arg) {
	/*
	PyEval_AcquireLock();
	PyInterpreterState *mainInterpreterState = mainThreadState->interp;
	PyThreadState * ownThreadState = PyThreadState_New(mainInterpreterState);
	PyEval_ReleaseLock();

	PyEval_AcquireLock();
	PyThreadState_Swap(ownThreadState);
	*/

	PyGILState_STATE state;
	state = PyGILState_Ensure();
	//execute function
	char *ret = PyCall("get", "get", "(s)", "hahaha");
	printf("ret = %s\n", ret);
	PyGILState_Release(state);

	/*
	PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();

	PyEval_AcquireLock();
	PyThreadState_Swap(NULL);
	PyThreadState_Clear(ownThreadState);
	PyThreadState_Delete(ownThreadState);
	PyEval_ReleaseLock();
	*/
}


int main(int argc,  char **argv) {
    //init python
    Py_Initialize();
    PyEval_InitThreads();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('./')");
    
    //mainThreadState = PyThreadState_Get();
    PyEval_ReleaseLock();

	pthread_t threads[10];
	int i, ret;

	for(i=0; i<10; i++) {
		ret = pthread_create(&threads[i], NULL, run, NULL);	
		if(ret != 0) {
			printf("error create threads\n");
			exit(1);
		}
	}

	for(i=0; i<10; i++) {
		pthread_join(threads[i], NULL);
	}

	//PyEval_AcquireLock();
	PyGILState_Ensure();
    Py_Finalize();
	return 0;
}

