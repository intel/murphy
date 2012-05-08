#!/usr/bin/env python
# -*- coding: latin-1 -*-


import os, sys, re
from lxml import etree

def fix_dummy(broken_xml):
    start = 0
    end = len(broken_xml)
    fixed_xml = ""

    for match in re.finditer(dummy_pattern, broken_xml):
        fixed_xml += broken_xml[start:match.start()]
        start = match.end()

    if start < end:
        fixed_xml += broken_xml[start:end]
    return fixed_xml

def fix_graphs(broken_xml):
    start = 0
    end = len(broken_xml)
    fixed_xml = ""

    for match in re.finditer(graph_pattern, broken_xml):
        fixed_xml += fix_dummy(broken_xml[start:match.start()])
        fixed_xml += "<!ENTITY graph%s \"%s\">" % \
            (match.group(1), os.path.basename(match.group(2)))
        start = match.end()

    if start < end:
        fixed_xml += fix_dummy(broken_xml[start:end])
    return fixed_xml

def fix_files(broken_xml):
    start = 0
    end = len(broken_xml)
    fixed_xml = ""

    for match in re.finditer(file_pattern, broken_xml):
        fixed_xml += fix_graphs(broken_xml[start:match.start()])
        fixed_xml += "<!ENTITY file%s SYSTEM \"%s\">" % \
            (match.group(1), match.group(2))
        start = match.end()

    if start < end:
        fixed_xml += fix_graphs(broken_xml[start:end])
    return fixed_xml


if len(sys.argv) >= 2:
    path = sys.argv[1]
    fnam = os.path.basename(path)
else:
    fnam = "<unknown>"

sys.stderr.write("  DBLYX %s\n" % fnam)

try:
    input  = open(path, "r")
    input_xml = input.read()
    input.close()
except IOError as (errno, strerror):
    print "Input error %d - %s" % (errno, strerror)
    exit(errno)

dummy_pattern = re.compile("<[/]?dummy>")
file_pattern = re.compile("<!ENTITY file([0-9]*) \"([^\"]*)\">")
graph_pattern = re.compile("<!ENTITY graph([0-9]*) \"([^\"]*)\">")

modified_xml = fix_files(input_xml)

parser  = etree.XMLParser(strip_cdata=False)
xmltree = etree.XML(modified_xml, parser)

output_xml = etree.tostring(xmltree, pretty_print=True)

if len(sys.argv) == 3:
    try:
        output = open(sys.argv[2], "w")
        output.write(output_xml)
        output.close()
    except IOError as (errno, strerror):
        print "Output error %d - %s" % (errno, strerror)
else:
    print output_xml
