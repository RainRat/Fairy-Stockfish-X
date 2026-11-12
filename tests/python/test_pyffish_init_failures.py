"""Fault-inject Python C API failures during pyffish module initialization."""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import sysconfig
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SHIM_SOURCE = r"""
#include <Python.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

PyObject *PyErr_NewException(const char *name, PyObject *base, PyObject *dict) {
    if (getenv("FSX_PYFFISH_FAIL") && !strcmp(getenv("FSX_PYFFISH_FAIL"), "exception")
        && !strcmp(name, "pyffish.error")) {
        PyErr_SetString(PyExc_MemoryError, "injected exception allocation failure");
        return NULL;
    }
    if (getenv("FSX_PYFFISH_FAIL") && !strcmp(getenv("FSX_PYFFISH_FAIL"), "probe")
        && !strcmp(name, "probe.marker")) {
        PyErr_SetString(PyExc_MemoryError, "injected probe failure");
        return NULL;
    }
    static PyObject *(*real_func)(const char *, PyObject *, PyObject *);
    if (!real_func) real_func = dlsym(RTLD_NEXT, "PyErr_NewException");
    return real_func(name, base, dict);
}

int PyModule_AddObject(PyObject *module, const char *name, PyObject *value) {
    if (getenv("FSX_PYFFISH_FAIL") && !strcmp(getenv("FSX_PYFFISH_FAIL"), "constant")
        && !strcmp(name, "VALUE_MATE")) {
        PyErr_SetString(PyExc_MemoryError, "injected constant insertion failure");
        return -1;
    }
    static int (*real_func)(PyObject *, const char *, PyObject *);
    if (!real_func) real_func = dlsym(RTLD_NEXT, "PyModule_AddObject");
    return real_func(module, name, value);
}
"""

PROBE_SOURCE = r"""
#include <Python.h>

static PyMethodDef methods[] = { {NULL, NULL, 0, NULL} };
static struct PyModuleDef mod = { PyModuleDef_HEAD_INIT, "probe", NULL, -1, methods };

PyMODINIT_FUNC PyInit_probe(void) {
    PyObject *marker = PyErr_NewException("probe.marker", NULL, NULL);
    if (!marker) return NULL;
    Py_DECREF(marker);
    return PyModule_Create(&mod);
}
"""


def main():
    if not sys.platform.startswith("linux"):
        print("pyffish initialization fault-injection checks skipped (requires Linux LD_PRELOAD)")
        return
    if not sysconfig.get_config_var("Py_ENABLE_SHARED"):
        # A statically linked interpreter exports its own Py* symbols, which take
        # precedence over LD_PRELOAD, so the shim cannot interpose the C API calls.
        print("pyffish initialization fault-injection checks skipped (requires a shared libpython)")
        return
    build_dir = ROOT / ".local" / "build"
    build_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="pyffish-init-", dir=build_dir) as temp:
        temp = Path(temp)
        source = temp / "fail_api.c"
        shim = temp / "fail_api.so"
        source.write_text(SHIM_SOURCE)
        compiler = shlex.split(sysconfig.get_config_var("CC") or "cc")
        includes = ["-I" + sysconfig.get_path("include")]
        platinclude = sysconfig.get_path("platinclude")
        if platinclude and platinclude not in includes:
            includes.append("-I" + platinclude)
        subprocess.run(compiler + ["-shared", "-fPIC", *includes, str(source), "-ldl", "-o", str(shim)], check=True)

        # Probe whether LD_PRELOAD interposition actually reaches extension
        # modules on this interpreter. Interpreters with the runtime statically
        # linked into the executable (e.g. Debian/Ubuntu /usr/bin/python3.x)
        # bind extension references to the executable, bypassing the shim, so
        # a clean import below means the platform defeats the mechanism rather
        # than the error handling being broken.
        probe_source = temp / "probe.c"
        probe_source.write_text(PROBE_SOURCE)
        ext_suffix = sysconfig.get_config_var("EXT_SUFFIX") or ".so"
        probe = temp / ("probe" + ext_suffix)
        subprocess.run(compiler + ["-shared", "-fPIC", *includes, str(probe_source), "-o", str(probe)], check=True)
        env = os.environ.copy()
        env["LD_PRELOAD"] = str(shim) + (":" + env["LD_PRELOAD"] if env.get("LD_PRELOAD") else "")
        env["FSX_PYFFISH_FAIL"] = "probe"
        probe_run = subprocess.run(
            [sys.executable, "-c", """
import sys
try:
    import probe
except MemoryError:
    assert 'probe' not in sys.modules
    print("PROBE_INTERPOSED")
else:
    print("PROBE_BYPASSED")
"""],
            cwd=str(temp),
            env=env,
            check=True,
            capture_output=True,
            text=True,
        )
        if "PROBE_BYPASSED" in probe_run.stdout:
            print("pyffish initialization fault-injection checks skipped "
                  "(LD_PRELOAD interposition is defeated by this interpreter)")
            return

        for failure in ("exception", "constant"):
            env = os.environ.copy()
            env["LD_PRELOAD"] = str(shim) + (":" + env["LD_PRELOAD"] if env.get("LD_PRELOAD") else "")
            env["FSX_PYFFISH_FAIL"] = failure
            subprocess.run(
                [sys.executable, "-c", """
import sys
try:
    import pyffish
except MemoryError:
    assert 'pyffish' not in sys.modules
else:
    raise SystemExit('injected initialization failure was ignored')
"""],
                cwd=ROOT,
                env=env,
                check=True,
            )

    print("pyffish initialization fault-injection checks passed")


if __name__ == "__main__":
    main()
