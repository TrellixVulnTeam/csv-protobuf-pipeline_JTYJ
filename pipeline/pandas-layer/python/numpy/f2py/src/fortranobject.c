#define FORTRANOBJECT_C
#include "fortranobject.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>




int
F2PyDict_SetItemString(PyObject *dict, char *name, PyObject *obj)
{
    if (obj==NULL) {
        fprintf(stderr, "Error loading %s\n", name);
        if (PyErr_Occurred()) {
            PyErr_Print();
            PyErr_Clear();
        }
        return -1;
    }
    return PyDict_SetItemString(dict, name, obj);
}




typedef PyObject *(*fortranfunc)(PyObject *,PyObject *,PyObject *,void *);

PyObject *
PyFortranObject_New(FortranDataDef* defs, f2py_void_func init) {
    int i;
    PyFortranObject *fp = NULL;
    PyObject *v = NULL;
    if (init!=NULL)                           

        (*(init))();
    if ((fp = PyObject_New(PyFortranObject, &PyFortran_Type))==NULL) return NULL;
    if ((fp->dict = PyDict_New())==NULL) return NULL;
    fp->len = 0;
    while (defs[fp->len].name != NULL) fp->len++;
    if (fp->len == 0) goto fail;
    fp->defs = defs;
    for (i=0;i<fp->len;i++)
        if (fp->defs[i].rank == -1) {                      

            v = PyFortranObject_NewAsAttr(&(fp->defs[i]));
            if (v==NULL) return NULL;
            PyDict_SetItemString(fp->dict,fp->defs[i].name,v);
        } else
            if ((fp->defs[i].data)!=NULL) { 

                if (fp->defs[i].type == NPY_STRING) {
                    int n = fp->defs[i].rank-1;
                    v = PyArray_New(&PyArray_Type, n, fp->defs[i].dims.d,
                                    NPY_STRING, NULL, fp->defs[i].data, fp->defs[i].dims.d[n],
                                    NPY_ARRAY_FARRAY, NULL);
                }
                else {
                    v = PyArray_New(&PyArray_Type, fp->defs[i].rank, fp->defs[i].dims.d,
                                    fp->defs[i].type, NULL, fp->defs[i].data, 0, NPY_ARRAY_FARRAY,
                                    NULL);
                }
                if (v==NULL) return NULL;
                PyDict_SetItemString(fp->dict,fp->defs[i].name,v);
            }
    Py_XDECREF(v);
    return (PyObject *)fp;
 fail:
    Py_XDECREF(v);
    return NULL;
}

PyObject *
PyFortranObject_NewAsAttr(FortranDataDef* defs) { 

    PyFortranObject *fp = NULL;
    fp = PyObject_New(PyFortranObject, &PyFortran_Type);
    if (fp == NULL) return NULL;
    if ((fp->dict = PyDict_New())==NULL) return NULL;
    fp->len = 1;
    fp->defs = defs;
    return (PyObject *)fp;
}




static void
fortran_dealloc(PyFortranObject *fp) {
    Py_XDECREF(fp->dict);
    PyMem_Del(fp);
}


#if PY_VERSION_HEX >= 0x03000000
#else
static PyMethodDef fortran_methods[] = {
    {NULL,          NULL}           

};
#endif




static Py_ssize_t
format_def(char *buf, Py_ssize_t size, FortranDataDef def)
{
    char *p = buf;
    int i, n;

    n = PyOS_snprintf(p, size, "array(%" NPY_INTP_FMT, def.dims.d[0]);
    if (n < 0 || n >= size) {
        return -1;
    }
    p += n;
    size -= n;

    for (i = 1; i < def.rank; i++) {
        n = PyOS_snprintf(p, size, ",%" NPY_INTP_FMT, def.dims.d[i]);
        if (n < 0 || n >= size) {
            return -1;
        }
        p += n;
        size -= n;
    }

    if (size <= 0) {
        return -1;
    }

    *p++ = ')';
    size--;

    if (def.data == NULL) {
        static const char notalloc[] = ", not allocated";
        if (size < sizeof(notalloc)) {
            return -1;
        }
        memcpy(p, notalloc, sizeof(notalloc));
    }

    return p - buf;
}

static PyObject *
fortran_doc(FortranDataDef def)
{
    char *buf, *p;
    PyObject *s = NULL;
    Py_ssize_t n, origsize, size = 100;

    if (def.doc != NULL) {
        size += strlen(def.doc);
    }
    origsize = size;
    buf = p = (char *)PyMem_Malloc(size);
    if (buf == NULL) {
        return PyErr_NoMemory();
    }

    if (def.rank == -1) {
        if (def.doc) {
            n = strlen(def.doc);
            if (n > size) {
                goto fail;
            }
            memcpy(p, def.doc, n);
            p += n;
            size -= n;
        }
        else {
            n = PyOS_snprintf(p, size, "%s - no docs available", def.name);
            if (n < 0 || n >= size) {
                goto fail;
            }
            p += n;
            size -= n;
        }
    }
    else {
        PyArray_Descr *d = PyArray_DescrFromType(def.type);
        n = PyOS_snprintf(p, size, "'%c'-", d->type);
        Py_DECREF(d);
        if (n < 0 || n >= size) {
            goto fail;
        }
        p += n;
        size -= n;

        if (def.data == NULL) {
            n = format_def(p, size, def) == -1;
            if (n < 0) {
                goto fail;
            }
            p += n;
            size -= n;
        }
        else if (def.rank > 0) {
            n = format_def(p, size, def);
            if (n < 0) {
                goto fail;
            }
            p += n;
            size -= n;
        }
        else {
            n = strlen("scalar");
            if (size < n) {
                goto fail;
            }
            memcpy(p, "scalar", n);
            p += n;
            size -= n;
        }
    }
    if (size <= 1) {
        goto fail;
    }
    *p++ = '\n';
    size--;

    

#if PY_VERSION_HEX >= 0x03000000
    s = PyUnicode_FromStringAndSize(buf, p - buf);
#else
    s = PyString_FromStringAndSize(buf, p - buf);
#endif

    PyMem_Free(buf);
    return s;

 fail:
    fprintf(stderr, "fortranobject.c: fortran_doc: len(p)=%zd>%zd=size:"
                    " too long docstring required, increase size\n",
            p - buf, origsize);
    PyMem_Free(buf);
    return NULL;
}

static FortranDataDef *save_def; 

static void set_data(char *d,npy_intp *f) {  

    if (*f)                                  

        save_def->data = d;
    else
        save_def->data = NULL;
    

}

static PyObject *
fortran_getattr(PyFortranObject *fp, char *name) {
    int i,j,k,flag;
    if (fp->dict != NULL) {
        PyObject *v = PyDict_GetItemString(fp->dict, name);
        if (v != NULL) {
            Py_INCREF(v);
            return v;
        }
    }
    for (i=0,j=1;i<fp->len && (j=strcmp(name,fp->defs[i].name));i++);
    if (j==0)
        if (fp->defs[i].rank!=-1) {                   

            if (fp->defs[i].func==NULL) return NULL;
            for(k=0;k<fp->defs[i].rank;++k)
                fp->defs[i].dims.d[k]=-1;
            save_def = &fp->defs[i];
            (*(fp->defs[i].func))(&fp->defs[i].rank,fp->defs[i].dims.d,set_data,&flag);
            if (flag==2)
                k = fp->defs[i].rank + 1;
            else
                k = fp->defs[i].rank;
            if (fp->defs[i].data !=NULL) {              

                PyObject *v = PyArray_New(&PyArray_Type, k, fp->defs[i].dims.d,
                                          fp->defs[i].type, NULL, fp->defs[i].data, 0, NPY_ARRAY_FARRAY,
                                          NULL);
                if (v==NULL) return NULL;
                

                return v;
            } else {                                    

                Py_RETURN_NONE;
            }
        }
    if (strcmp(name,"__dict__")==0) {
        Py_INCREF(fp->dict);
        return fp->dict;
    }
    if (strcmp(name,"__doc__")==0) {
#if PY_VERSION_HEX >= 0x03000000
        PyObject *s = PyUnicode_FromString(""), *s2, *s3;
        for (i=0;i<fp->len;i++) {
            s2 = fortran_doc(fp->defs[i]);
            s3 = PyUnicode_Concat(s, s2);
            Py_DECREF(s2);
            Py_DECREF(s);
            s = s3;
        }
#else
        PyObject *s = PyString_FromString("");
        for (i=0;i<fp->len;i++)
            PyString_ConcatAndDel(&s,fortran_doc(fp->defs[i]));
#endif
        if (PyDict_SetItemString(fp->dict, name, s))
            return NULL;
        return s;
    }
    if ((strcmp(name,"_cpointer")==0) && (fp->len==1)) {
        PyObject *cobj = F2PyCapsule_FromVoidPtr((void *)(fp->defs[0].data),NULL);
        if (PyDict_SetItemString(fp->dict, name, cobj))
            return NULL;
        return cobj;
    }
#if PY_VERSION_HEX >= 0x03000000
    if (1) {
        PyObject *str, *ret;
        str = PyUnicode_FromString(name);
        ret = PyObject_GenericGetAttr((PyObject *)fp, str);
        Py_DECREF(str);
        return ret;
    }
#else
    return Py_FindMethod(fortran_methods, (PyObject *)fp, name);
#endif
}

static int
fortran_setattr(PyFortranObject *fp, char *name, PyObject *v) {
    int i,j,flag;
    PyArrayObject *arr = NULL;
    for (i=0,j=1;i<fp->len && (j=strcmp(name,fp->defs[i].name));i++);
    if (j==0) {
        if (fp->defs[i].rank==-1) {
            PyErr_SetString(PyExc_AttributeError,"over-writing fortran routine");
            return -1;
        }
        if (fp->defs[i].func!=NULL) { 

            npy_intp dims[F2PY_MAX_DIMS];
            int k;
            save_def = &fp->defs[i];
            if (v!=Py_None) {     

                for(k=0;k<fp->defs[i].rank;k++) dims[k]=-1;
                if ((arr = array_from_pyobj(fp->defs[i].type,dims,fp->defs[i].rank,F2PY_INTENT_IN,v))==NULL)
                    return -1;
                (*(fp->defs[i].func))(&fp->defs[i].rank,PyArray_DIMS(arr),set_data,&flag);
            } else {             

                for(k=0;k<fp->defs[i].rank;k++) dims[k]=0;
                (*(fp->defs[i].func))(&fp->defs[i].rank,dims,set_data,&flag);
                for(k=0;k<fp->defs[i].rank;k++) dims[k]=-1;
            }
            memcpy(fp->defs[i].dims.d,dims,fp->defs[i].rank*sizeof(npy_intp));
        } else {                     

            if ((arr = array_from_pyobj(fp->defs[i].type,fp->defs[i].dims.d,fp->defs[i].rank,F2PY_INTENT_IN,v))==NULL)
                return -1;
        }
        if (fp->defs[i].data!=NULL) { 

            npy_intp s = PyArray_MultiplyList(fp->defs[i].dims.d,PyArray_NDIM(arr));
            if (s==-1)
                s = PyArray_MultiplyList(PyArray_DIMS(arr),PyArray_NDIM(arr));
            if (s<0 ||
                (memcpy(fp->defs[i].data,PyArray_DATA(arr),s*PyArray_ITEMSIZE(arr)))==NULL) {
                if ((PyObject*)arr!=v) {
                    Py_DECREF(arr);
                }
                return -1;
            }
            if ((PyObject*)arr!=v) {
                Py_DECREF(arr);
            }
        } else return (fp->defs[i].func==NULL?-1:0);
        return 0; 

    }
    if (fp->dict == NULL) {
        fp->dict = PyDict_New();
        if (fp->dict == NULL)
            return -1;
    }
    if (v == NULL) {
        int rv = PyDict_DelItemString(fp->dict, name);
        if (rv < 0)
            PyErr_SetString(PyExc_AttributeError,"delete non-existing fortran attribute");
        return rv;
    }
    else
        return PyDict_SetItemString(fp->dict, name, v);
}

static PyObject*
fortran_call(PyFortranObject *fp, PyObject *arg, PyObject *kw) {
    int i = 0;
    

    if (fp->defs[i].rank==-1) {

        if (fp->defs[i].func==NULL) {
            PyErr_Format(PyExc_RuntimeError, "no function to call");
            return NULL;
        }
        else if (fp->defs[i].data==NULL)
            

            return (*((fortranfunc)(fp->defs[i].func)))((PyObject *)fp,arg,kw,NULL);
        else
            return (*((fortranfunc)(fp->defs[i].func)))((PyObject *)fp,arg,kw,
                                                        (void *)fp->defs[i].data);
    }
    PyErr_Format(PyExc_TypeError, "this fortran object is not callable");
    return NULL;
}

static PyObject *
fortran_repr(PyFortranObject *fp)
{
    PyObject *name = NULL, *repr = NULL;
    name = PyObject_GetAttrString((PyObject *)fp, "__name__");
    PyErr_Clear();
#if PY_VERSION_HEX >= 0x03000000
    if (name != NULL && PyUnicode_Check(name)) {
        repr = PyUnicode_FromFormat("<fortran %U>", name);
    }
    else {
        repr = PyUnicode_FromString("<fortran object>");
    }
#else
    if (name != NULL && PyString_Check(name)) {
        repr = PyString_FromFormat("<fortran %s>", PyString_AsString(name));
    }
    else {
        repr = PyString_FromString("<fortran object>");
    }
#endif
    Py_XDECREF(name);
    return repr;
}


PyTypeObject PyFortran_Type = {
#if PY_VERSION_HEX >= 0x03000000
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(0)
    0,                            

#endif
    "fortran",                    

    sizeof(PyFortranObject),      

    0,                            

    

    (destructor)fortran_dealloc,  

    0,                            

    (getattrfunc)fortran_getattr, 

    (setattrfunc)fortran_setattr, 

    0,                            

    (reprfunc)fortran_repr,       

    0,                            

    0,                            

    0,                            

    0,                            

    (ternaryfunc)fortran_call,    

};




#ifdef F2PY_REPORT_ATEXIT
static int passed_time = 0;
static int passed_counter = 0;
static int passed_call_time = 0;
static struct timeb start_time;
static struct timeb stop_time;
static struct timeb start_call_time;
static struct timeb stop_call_time;
static int cb_passed_time = 0;
static int cb_passed_counter = 0;
static int cb_passed_call_time = 0;
static struct timeb cb_start_time;
static struct timeb cb_stop_time;
static struct timeb cb_start_call_time;
static struct timeb cb_stop_call_time;

extern void f2py_start_clock(void) { ftime(&start_time); }
extern
void f2py_start_call_clock(void) {
    f2py_stop_clock();
    ftime(&start_call_time);
}
extern
void f2py_stop_clock(void) {
    ftime(&stop_time);
    passed_time += 1000*(stop_time.time - start_time.time);
    passed_time += stop_time.millitm - start_time.millitm;
}
extern
void f2py_stop_call_clock(void) {
    ftime(&stop_call_time);
    passed_call_time += 1000*(stop_call_time.time - start_call_time.time);
    passed_call_time += stop_call_time.millitm - start_call_time.millitm;
    passed_counter += 1;
    f2py_start_clock();
}

extern void f2py_cb_start_clock(void) { ftime(&cb_start_time); }
extern
void f2py_cb_start_call_clock(void) {
    f2py_cb_stop_clock();
    ftime(&cb_start_call_time);
}
extern
void f2py_cb_stop_clock(void) {
    ftime(&cb_stop_time);
    cb_passed_time += 1000*(cb_stop_time.time - cb_start_time.time);
    cb_passed_time += cb_stop_time.millitm - cb_start_time.millitm;
}
extern
void f2py_cb_stop_call_clock(void) {
    ftime(&cb_stop_call_time);
    cb_passed_call_time += 1000*(cb_stop_call_time.time - cb_start_call_time.time);
    cb_passed_call_time += cb_stop_call_time.millitm - cb_start_call_time.millitm;
    cb_passed_counter += 1;
    f2py_cb_start_clock();
}

static int f2py_report_on_exit_been_here = 0;
extern
void f2py_report_on_exit(int exit_flag,void *name) {
    if (f2py_report_on_exit_been_here) {
        fprintf(stderr,"             %s\n",(char*)name);
        return;
    }
    f2py_report_on_exit_been_here = 1;
    fprintf(stderr,"                      /-----------------------\\\n");
    fprintf(stderr,"                     < F2PY performance report >\n");
    fprintf(stderr,"                      \\-----------------------/\n");
    fprintf(stderr,"Overall time spent in ...\n");
    fprintf(stderr,"(a) wrapped (Fortran/C) functions           : %8d msec\n",
            passed_call_time);
    fprintf(stderr,"(b) f2py interface,           %6d calls  : %8d msec\n",
            passed_counter,passed_time);
    fprintf(stderr,"(c) call-back (Python) functions            : %8d msec\n",
            cb_passed_call_time);
    fprintf(stderr,"(d) f2py call-back interface, %6d calls  : %8d msec\n",
            cb_passed_counter,cb_passed_time);

    fprintf(stderr,"(e) wrapped (Fortran/C) functions (actual) : %8d msec\n\n",
            passed_call_time-cb_passed_call_time-cb_passed_time);
    fprintf(stderr,"Use -DF2PY_REPORT_ATEXIT_DISABLE to disable this message.\n");
    fprintf(stderr,"Exit status: %d\n",exit_flag);
    fprintf(stderr,"Modules    : %s\n",(char*)name);
}
#endif




#ifdef F2PY_REPORT_ON_ARRAY_COPY
static void f2py_report_on_array_copy(PyArrayObject* arr) {
    const npy_intp arr_size = PyArray_Size((PyObject *)arr);
    if (arr_size>F2PY_REPORT_ON_ARRAY_COPY) {
        fprintf(stderr,"copied an array: size=%ld, elsize=%"NPY_INTP_FMT"\n",
                arr_size, (npy_intp)PyArray_ITEMSIZE(arr));
    }
}
static void f2py_report_on_array_copy_fromany(void) {
    fprintf(stderr,"created an array from object\n");
}

#define F2PY_REPORT_ON_ARRAY_COPY_FROMARR f2py_report_on_array_copy((PyArrayObject *)arr)
#define F2PY_REPORT_ON_ARRAY_COPY_FROMANY f2py_report_on_array_copy_fromany()
#else
#define F2PY_REPORT_ON_ARRAY_COPY_FROMARR
#define F2PY_REPORT_ON_ARRAY_COPY_FROMANY
#endif








static int check_and_fix_dimensions(const PyArrayObject* arr,
                                    const int rank,
                                    npy_intp *dims);

static int
count_negative_dimensions(const int rank,
                          const npy_intp *dims) {
    int i=0,r=0;
    while (i<rank) {
        if (dims[i] < 0) ++r;
        ++i;
    }
    return r;
}

#ifdef DEBUG_COPY_ND_ARRAY
void dump_dims(int rank, npy_intp* dims) {
    int i;
    printf("[");
    for(i=0;i<rank;++i) {
        printf("%3" NPY_INTP_FMT, dims[i]);
    }
    printf("]\n");
}
void dump_attrs(const PyArrayObject* obj) {
    const PyArrayObject_fields *arr = (const PyArrayObject_fields*) obj;
    int rank = PyArray_NDIM(arr);
    npy_intp size = PyArray_Size((PyObject *)arr);
    printf("\trank = %d, flags = %d, size = %" NPY_INTP_FMT  "\n",
           rank,arr->flags,size);
    printf("\tstrides = ");
    dump_dims(rank,arr->strides);
    printf("\tdimensions = ");
    dump_dims(rank,arr->dimensions);
}
#endif

#define SWAPTYPE(a,b,t) {t c; c = (a); (a) = (b); (b) = c; }

static int swap_arrays(PyArrayObject* obj1, PyArrayObject* obj2) {
    PyArrayObject_fields *arr1 = (PyArrayObject_fields*) obj1,
                         *arr2 = (PyArrayObject_fields*) obj2;
    SWAPTYPE(arr1->data,arr2->data,char*);
    SWAPTYPE(arr1->nd,arr2->nd,int);
    SWAPTYPE(arr1->dimensions,arr2->dimensions,npy_intp*);
    SWAPTYPE(arr1->strides,arr2->strides,npy_intp*);
    SWAPTYPE(arr1->base,arr2->base,PyObject*);
    SWAPTYPE(arr1->descr,arr2->descr,PyArray_Descr*);
    SWAPTYPE(arr1->flags,arr2->flags,int);
    

    return 0;
}

#define ARRAY_ISCOMPATIBLE(arr,type_num)                                \
    (  (PyArray_ISINTEGER(arr) && PyTypeNum_ISINTEGER(type_num))        \
       ||(PyArray_ISFLOAT(arr) && PyTypeNum_ISFLOAT(type_num))          \
       ||(PyArray_ISCOMPLEX(arr) && PyTypeNum_ISCOMPLEX(type_num))      \
       ||(PyArray_ISBOOL(arr) && PyTypeNum_ISBOOL(type_num))            \
       )

extern
PyArrayObject* array_from_pyobj(const int type_num,
                                npy_intp *dims,
                                const int rank,
                                const int intent,
                                PyObject *obj) {
    

    char mess[200];
    PyArrayObject *arr = NULL;
    PyArray_Descr *descr;
    char typechar;
    int elsize;

    if ((intent & F2PY_INTENT_HIDE)
        || ((intent & F2PY_INTENT_CACHE) && (obj==Py_None))
        || ((intent & F2PY_OPTIONAL) && (obj==Py_None))
        ) {
        

        if (count_negative_dimensions(rank,dims) > 0) {
            int i;
            strcpy(mess, "failed to create intent(cache|hide)|optional array"
                   "-- must have defined dimensions but got (");
            for(i=0;i<rank;++i)
                sprintf(mess+strlen(mess),"%" NPY_INTP_FMT ",",dims[i]);
            strcat(mess, ")");
            PyErr_SetString(PyExc_ValueError,mess);
            return NULL;
        }
        arr = (PyArrayObject *)
            PyArray_New(&PyArray_Type, rank, dims, type_num,
                        NULL,NULL,1,
                        !(intent&F2PY_INTENT_C),
                        NULL);
        if (arr==NULL) return NULL;
        if (!(intent & F2PY_INTENT_CACHE))
            PyArray_FILLWBYTE(arr, 0);
        return arr;
    }

    descr = PyArray_DescrFromType(type_num);
    

    if (type_num == NPY_STRING) {
        PyArray_DESCR_REPLACE(descr);
        if (descr == NULL) {
            return NULL;
        }
        descr->elsize = 1;
        descr->type = NPY_CHARLTR;
    }
    elsize = descr->elsize;
    typechar = descr->type;
    Py_DECREF(descr);
    if (PyArray_Check(obj)) {
        arr = (PyArrayObject *)obj;

        if (intent & F2PY_INTENT_CACHE) {
            

            if (PyArray_ISONESEGMENT(arr)
                && PyArray_ITEMSIZE(arr)>=elsize) {
                if (check_and_fix_dimensions(arr, rank, dims)) {
                    return NULL;
                }
                if (intent & F2PY_INTENT_OUT)
                    Py_INCREF(arr);
                return arr;
            }
            strcpy(mess, "failed to initialize intent(cache) array");
            if (!PyArray_ISONESEGMENT(arr))
                strcat(mess, " -- input must be in one segment");
            if (PyArray_ITEMSIZE(arr)<elsize)
                sprintf(mess+strlen(mess),
                        " -- expected at least elsize=%d but got %" NPY_INTP_FMT,
                        elsize,
                        (npy_intp)PyArray_ITEMSIZE(arr)
                        );
            PyErr_SetString(PyExc_ValueError,mess);
            return NULL;
        }

        


        if (check_and_fix_dimensions(arr, rank, dims)) {
            return NULL;
        }
        

        if ((! (intent & F2PY_INTENT_COPY))
            && PyArray_ITEMSIZE(arr)==elsize
            && ARRAY_ISCOMPATIBLE(arr,type_num)
            && F2PY_CHECK_ALIGNMENT(arr, intent)
            ) {
            if ((intent & F2PY_INTENT_C)?PyArray_ISCARRAY(arr):PyArray_ISFARRAY(arr)) {
                if ((intent & F2PY_INTENT_OUT)) {
                    Py_INCREF(arr);
                }
                

                return arr;
            }
        }

        if (intent & F2PY_INTENT_INOUT) {
            strcpy(mess, "failed to initialize intent(inout) array");
            if ((intent & F2PY_INTENT_C) && !PyArray_ISCARRAY(arr))
                strcat(mess, " -- input not contiguous");
            if (!(intent & F2PY_INTENT_C) && !PyArray_ISFARRAY(arr))
                strcat(mess, " -- input not fortran contiguous");
            if (PyArray_ITEMSIZE(arr)!=elsize)
                sprintf(mess+strlen(mess),
                        " -- expected elsize=%d but got %" NPY_INTP_FMT,
                        elsize,
                        (npy_intp)PyArray_ITEMSIZE(arr)
                        );
            if (!(ARRAY_ISCOMPATIBLE(arr,type_num)))
                sprintf(mess+strlen(mess)," -- input '%c' not compatible to '%c'",
                        PyArray_DESCR(arr)->type,typechar);
            if (!(F2PY_CHECK_ALIGNMENT(arr, intent)))
              sprintf(mess+strlen(mess)," -- input not %d-aligned", F2PY_GET_ALIGNMENT(intent));
            PyErr_SetString(PyExc_ValueError,mess);
            return NULL;
        }

        


        {
            PyArrayObject * retarr;
            retarr = (PyArrayObject *) \
                PyArray_New(&PyArray_Type, PyArray_NDIM(arr), PyArray_DIMS(arr), type_num,
                            NULL,NULL,1,
                            !(intent&F2PY_INTENT_C),
                            NULL);
            if (retarr==NULL)
                return NULL;
            F2PY_REPORT_ON_ARRAY_COPY_FROMARR;
            if (PyArray_CopyInto(retarr, arr)) {
                Py_DECREF(retarr);
                return NULL;
            }
            if (intent & F2PY_INTENT_INPLACE) {
                if (swap_arrays(arr,retarr))
                    return NULL; 

                Py_XDECREF(retarr);
                if (intent & F2PY_INTENT_OUT)
                    Py_INCREF(arr);
            } else {
                arr = retarr;
            }
        }
        return arr;
    }

    if ((intent & F2PY_INTENT_INOUT) ||
            (intent & F2PY_INTENT_INPLACE) ||
            (intent & F2PY_INTENT_CACHE)) {
        PyErr_SetString(PyExc_TypeError,
                        "failed to initialize intent(inout|inplace|cache) "
                        "array, input not an array");
        return NULL;
    }

    {
        PyArray_Descr * descr = PyArray_DescrFromType(type_num);
        

        if (type_num == NPY_STRING) {
            PyArray_DESCR_REPLACE(descr);
            if (descr == NULL) {
                return NULL;
            }
            descr->elsize = 1;
            descr->type = NPY_CHARLTR;
        }
        F2PY_REPORT_ON_ARRAY_COPY_FROMANY;
        arr = (PyArrayObject *) \
            PyArray_FromAny(obj, descr, 0,0,
                            ((intent & F2PY_INTENT_C)?NPY_ARRAY_CARRAY:NPY_ARRAY_FARRAY) \
                            | NPY_ARRAY_FORCECAST, NULL);
        if (arr==NULL)
            return NULL;
        if (check_and_fix_dimensions(arr, rank, dims)) {
            return NULL;
        }
        return arr;
    }

}








static
int check_and_fix_dimensions(const PyArrayObject* arr, const int rank, npy_intp *dims)
{
    

    const npy_intp arr_size = (PyArray_NDIM(arr))?PyArray_Size((PyObject *)arr):1;
#ifdef DEBUG_COPY_ND_ARRAY
    dump_attrs(arr);
    printf("check_and_fix_dimensions:init: dims=");
    dump_dims(rank,dims);
#endif
    if (rank > PyArray_NDIM(arr)) { 

        npy_intp new_size = 1;
        int free_axe = -1;
        int i;
        npy_intp d;
        

        for(i=0;i<PyArray_NDIM(arr);++i) {
            d = PyArray_DIM(arr,i);
            if (dims[i] >= 0) {
                if (d>1 && dims[i]!=d) {
                    PyErr_Format(PyExc_ValueError,
                                 "%d-th dimension must be fixed to %"
                                 NPY_INTP_FMT " but got %" NPY_INTP_FMT "\n",
                                 i, dims[i], d);
                    return 1;
                }
                if (!dims[i]) dims[i] = 1;
            } else {
                dims[i] = d ? d : 1;
            }
            new_size *= dims[i];
        }
        for(i=PyArray_NDIM(arr);i<rank;++i)
            if (dims[i]>1) {
                PyErr_Format(PyExc_ValueError,
                             "%d-th dimension must be %" NPY_INTP_FMT
                             " but got 0 (not defined).\n",
                             i, dims[i]);
                return 1;
            } else if (free_axe<0)
                free_axe = i;
            else
                dims[i] = 1;
        if (free_axe>=0) {
            dims[free_axe] = arr_size/new_size;
            new_size *= dims[free_axe];
        }
        if (new_size != arr_size) {
            PyErr_Format(PyExc_ValueError,
                         "unexpected array size: new_size=%" NPY_INTP_FMT
                         ", got array with arr_size=%" NPY_INTP_FMT
                         " (maybe too many free indices)\n",
                         new_size, arr_size);
            return 1;
        }
    } else if (rank==PyArray_NDIM(arr)) {
        npy_intp new_size = 1;
        int i;
        npy_intp d;
        for (i=0; i<rank; ++i) {
            d = PyArray_DIM(arr,i);
            if (dims[i]>=0) {
                if (d > 1 && d!=dims[i]) {
                    PyErr_Format(PyExc_ValueError,
                                 "%d-th dimension must be fixed to %"
                                 NPY_INTP_FMT " but got %" NPY_INTP_FMT "\n",
                                 i, dims[i], d);
                    return 1;
                }
                if (!dims[i]) dims[i] = 1;
            } else dims[i] = d;
            new_size *= dims[i];
        }
        if (new_size != arr_size) {
            PyErr_Format(PyExc_ValueError,
                         "unexpected array size: new_size=%" NPY_INTP_FMT
                         ", got array with arr_size=%" NPY_INTP_FMT "\n",
                         new_size, arr_size);
            return 1;
        }
    } else { 

        int i,j;
        npy_intp d;
        int effrank;
        npy_intp size;
        for (i=0,effrank=0;i<PyArray_NDIM(arr);++i)
            if (PyArray_DIM(arr,i)>1) ++effrank;
        if (dims[rank-1]>=0)
            if (effrank>rank) {
                PyErr_Format(PyExc_ValueError,
                             "too many axes: %d (effrank=%d), "
                             "expected rank=%d\n",
                             PyArray_NDIM(arr), effrank, rank);
                return 1;
            }

        for (i=0,j=0;i<rank;++i) {
            while (j<PyArray_NDIM(arr) && PyArray_DIM(arr,j)<2) ++j;
            if (j>=PyArray_NDIM(arr)) d = 1;
            else d = PyArray_DIM(arr,j++);
            if (dims[i]>=0) {
                if (d>1 && d!=dims[i]) {
                    PyErr_Format(PyExc_ValueError,
                                 "%d-th dimension must be fixed to %"
                                 NPY_INTP_FMT " but got %" NPY_INTP_FMT
                                 " (real index=%d)\n",
                                 i, dims[i], d, j-1);
                    return 1;
                }
                if (!dims[i]) dims[i] = 1;
            } else
                dims[i] = d;
        }

        for (i=rank;i<PyArray_NDIM(arr);++i) { 

            while (j<PyArray_NDIM(arr) && PyArray_DIM(arr,j)<2) ++j;
            if (j>=PyArray_NDIM(arr)) d = 1;
            else d = PyArray_DIM(arr,j++);
            dims[rank-1] *= d;
        }
        for (i=0,size=1;i<rank;++i) size *= dims[i];
        if (size != arr_size) {
            char msg[200];
            int len;
            snprintf(msg, sizeof(msg),
                     "unexpected array size: size=%" NPY_INTP_FMT
                     ", arr_size=%" NPY_INTP_FMT
                     ", rank=%d, effrank=%d, arr.nd=%d, dims=[",
                     size, arr_size, rank, effrank, PyArray_NDIM(arr));
            for (i = 0; i < rank; ++i) {
                len = strlen(msg);
                snprintf(msg + len, sizeof(msg) - len,
                         " %" NPY_INTP_FMT, dims[i]);
            }
            len = strlen(msg);
            snprintf(msg + len, sizeof(msg) - len, " ], arr.dims=[");
            for (i = 0; i < PyArray_NDIM(arr); ++i) {
                len = strlen(msg);
                snprintf(msg + len, sizeof(msg) - len,
                         " %" NPY_INTP_FMT, PyArray_DIM(arr, i));
            }
            len = strlen(msg);
            snprintf(msg + len, sizeof(msg) - len, " ]\n");
            PyErr_SetString(PyExc_ValueError, msg);
            return 1;
        }
    }
#ifdef DEBUG_COPY_ND_ARRAY
    printf("check_and_fix_dimensions:end: dims=");
    dump_dims(rank,dims);
#endif
    return 0;
}







extern
int copy_ND_array(const PyArrayObject *arr, PyArrayObject *out)
{
    F2PY_REPORT_ON_ARRAY_COPY_FROMARR;
    return PyArray_CopyInto(out, (PyArrayObject *)arr);
}








#if PY_VERSION_HEX >= 0x03000000

PyObject *
F2PyCapsule_FromVoidPtr(void *ptr, void (*dtor)(PyObject *))
{
    PyObject *ret = PyCapsule_New(ptr, NULL, dtor);
    if (ret == NULL) {
        PyErr_Clear();
    }
    return ret;
}

void *
F2PyCapsule_AsVoidPtr(PyObject *obj)
{
    void *ret = PyCapsule_GetPointer(obj, NULL);
    if (ret == NULL) {
        PyErr_Clear();
    }
    return ret;
}

int
F2PyCapsule_Check(PyObject *ptr)
{
    return PyCapsule_CheckExact(ptr);
}

#else

PyObject *
F2PyCapsule_FromVoidPtr(void *ptr, void (*dtor)(void *))
{
    return PyCObject_FromVoidPtr(ptr, dtor);
}

void *
F2PyCapsule_AsVoidPtr(PyObject *ptr)
{
    return PyCObject_AsVoidPtr(ptr);
}

int
F2PyCapsule_Check(PyObject *ptr)
{
    return PyCObject_Check(ptr);
}

#endif


#ifdef __cplusplus
}
#endif


