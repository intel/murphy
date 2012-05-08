#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
   Making Doxygen dependencies
   ===========================

   Usage: doxydeps.py [OPTIONS] [depname] [doxyfile] [dir]

   <depname>       - is the dependency name, ie. the dependency file will
                     look like:
                         <depname>: file1.h file2.c ...
                     Depname defaults to basename of <doxyfile>.
   <doxyfile>      - is the path to the doxygen configuration file.
                     The default value is ./Doxyfile
   <dir>           - where the dependency file will be put.
                     The default value is .deps
                     if dir is '--' the dependencies will be printed on stdout

   doxydeps reads the doxygen configuration file and produces a dependency
   file with identical name + '.P' suffix under dependency directory.


   :license: BSD.
"""

import sys
import os
import re
import glob
import commands
import errno

# TODO: get the compilers from autoconf
c_compiler = "/usr/bin/gcc"

def _main():
    depname = None
    doxyfile = 'doxyfile'
    depdir = '.deps'

    idx = 1
    for arg in sys.argv[idx:]:
        if arg == '-h' or arg == '--help':
            _usage()
        elif arg.startswith('-'):
            _usage("invalid option %s" % arg)
        else:
            break
    if len(sys.argv) > idx:
        depname = sys.argv[idx]
        idx += 1
    if len(sys.argv) > idx:
        doxyfile = sys.argv[idx]
        idx += 1
    if len(sys.argv) > idx:
        depdir = sys.argv[idx]
        idx += 1
    if len(sys.argv) > idx:
        _usage("extra arguments %s" % " ".join(sys.argv[idx:]))
    if depname == None:
        depname = os.path.basename(doxyfile)

    if depdir == '--':
        output = sys.stdout
        close_output = False
    else:
        depfile = "%s.P" % os.path.basename(doxyfile)
        deppath = os.path.join(depdir, depfile)
        sys.stderr.write("  DOXYD %s\n" % depfile)

        try:
            os.makedirs(depdir)
        except OSError as (errcode, strerror):
            if errcode != errno.EEXIST:
                sys.stderr.write("can't create directory '%s': %s\n" % 
                                 (depdir, strerror))
                exit(errcode)
        try:
            output = open(deppath, "w")
        except IOError as (errcode, strerror):
            sys.stderr.write("failed to open '%s': %s\n" %
                             (deppath, strerror))
            exit(errcode)
        close_output = True

    deps = DoxygenDependencies(doxyfile)
    deps.insert(0, "%s:" % depname)
    
    output.write(" ".join(list("%s \\\n" % d for d in deps))[:-3] + "\n")

    if close_output:
        output.close()


def _usage(errmsg):
    if len(errmsg) < 1:
        err = 0
    else:
        err = 1
        sys.stderr.write("Error: " + errmsg + "\n\n")
    sys.stderr.write(__doc__)
    sys.exit(err)


def DoxygenDependencies(doxyfile):
    dirs = _parse_doxyfile(doxyfile)
    files = _find_doxygen_input_files(dirs) 
    deps  = files
    return deps

def _parse_doxyfile(doxyfile):
    home = os.path.dirname(doxyfile)
    filt = re.compile(r"(#.*)")
    inp  = []
    iext = []
    irec = False
    exa  = []
    eext = []
    erec = False
    try:
        df = open(doxyfile, "r")
    except IOError as (errno, strerror):
        sys.stderr.write("failed to open '%s': %s" %  (doxyfile, strerror))
        exit(errno)

    for line in df.readlines():
        assign = re.sub(filt, "", line).strip().split('=')
        if len(assign) != 2:
            continue
        key = assign[0].strip().upper()
        value = assign[1].strip()

        if len(value) < 1:
            continue

        if key == 'INPUT':
            inp += value.split(' ')
        elif key == 'FILE_PATTERNS':
            iext += value.split(' ')
        elif key == 'RECURSIVE':
            if value == 'YES':
                irec = True
        elif key == 'EXAMPLE_PATH':
            exa += value.split(' ')
        elif key == 'EXAMPLE_PATTERNS':
            iext += value.split(' ')
        elif key == 'EXAMPLE_RECURSIVE':
            if value == 'YES':
                recurs = True
                    
    df.close()

    dirs = []
    if len(iext) < 1:
        iext = ['*']
    for i in inp:
        if not i.startswith('/'):
            i = os.path.join(home, i)
        dirs.append((os.path.abspath(i), iext, irec))
    if len(eext) < 1:
        eext = ['*']
    for e in exa:
        if not e.startswith('/'):
            e = os.path.join(home, e)
        dirs.append((os.path.abspath(e), eext, erec))

    return dirs

def _find_doxygen_input_files(dirs, includes=[]):
    allfile = set()
    for d in dirs:
        includes.append(os.path.dirname(d[0]))
    for path, exts, recurse in dirs:
        if os.path.isfile(path):
            allfile.add(path)
            allfile = allfile.union(_find_doxygen_dependencies(path, includes))
        if not os.path.isdir(path):
            continue
        for e in exts:
            matches = glob.glob(os.path.join(path, e)) 
            allfile = allfile.union(matches)
            for m in matches:
                allfile = allfile.union(_find_doxygen_dependencies(m,includes))
        if recurse:
            for d in os.dirlist(path):
                if os.path.isdir(d):
                    allfile.union(_find_doxygen_input_files((d,exts,recurse),
                                                              includes))
    files = []
    for f in allfile:
        if len(f) > 0 and \
           not f.startswith('/usr/include/') and \
           not f.startswith('/usr/lib/'):
            files.append(f)
    files.sort()
    return files

def _find_doxygen_dependencies(path, includes):
    fnam = os.path.basename(path)
    depgen = "doxygen_%s_dependencies" % fnam[fnam.rfind('.'):][1:]
    if depgen in globals():
        return globals()[depgen](path, includes)
    return set()

def doxygen_c_dependencies(fnam, includes):
    options = " ".join(list("-I%s" % f for f in includes)) + " -M"
    cmd = "%s %s %s" % (c_compiler, options, fnam)
    status, cdeps = commands.getstatusoutput(cmd)
    if status == 0:
        deps = re.sub(r" +", " ",
               re.sub(r"\\\n", "",
               re.sub(r"^.*\.o: ", "", cdeps)))
        return set(deps.split(" "))
    return set()

doxygen_h_dependencies = doxygen_c_dependencies


if __name__ == '__main__':
    _main()
