#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
   Doxygen XML to DocBook converter
   ================================

   Usage: doxml2db.py [OPTIONS] <dir> <doxml-files> {<docbook-file>|--}

   <dir>           - is the directory where the doxml files are
   <doxml-file>    - list of one or more whitespace separated doxml input files
   <docbook-file>  - the output file
   --              - docbook is printed to stdout

   OPTIONS
   -h or --help    - print this help message
   --title=<title> - main chapter or section title
   --depth=<depth> - 0 = chapter, 1+ = section

   :license: BSD.
"""
import sys
import os
import re
import lxml.etree as ET
from copy import deepcopy

_files   = set()
_sources = {}
_comment_pattern = re.compile(r"(/\*.*\*/)|(//.*)")

def _main():
    title = None
    depth = 1
    sect  = {'enum':[], 'struct':[], 'union':[],
             'typedef':[], 'function':[], 'define':[]}

    fidx = 1
    for arg in sys.argv[fidx:]:
        if arg.startswith('-') and arg != '--':
            if arg == '-h' or arg == '--help':
                _usage()
            elif arg.startswith('--title='):
                title = arg[8:]
            elif arg.startswith('--depth='):
                depth = int(arg[8:])
            else:
                _usage("invalid option '%s'" % arg)
            fidx += 1
    if len(sys.argv) < fidx + 3:
        _usage("too few arguments")
    else:
        if os.path.isdir(sys.argv[fidx]):
            doxydir = sys.argv[fidx]
        else:
            _usage(sys.argv[fidx] + " not a directory")
        for f in sys.argv[fidx+1:-1]:
            _add_doxml_file(doxydir, f, sect)
        if sys.argv[-1] == '--':
            dbfile = sys.stdout
            close_dbfile = False
        else:
            dbfile = open(sys.argv[-1], "w")
            close_dbfile = True


    # parse all the doxml files
    # store the result in variable 'sect'
    for name, doxml in _files:
        if name[0] != '<':
            sys.stderr.write("  DOXML %s\n" % name)
        ParseDoxmlFile(doxml, sect)

    # process 'sect' (eg. resolve typedefs, skip useless entries)
    # merge evetrything into a sorted list ('defs')
    defs = ProcessSections(sect)

    dbroot = BuildDBTree(defs, title, depth)

    # dump the dbtree
    if dbroot.tag == 'root':
        for section in dbroot.iterchildren():
            dbfile.write(ET.tostring(section, pretty_print=True))
    else:
        dbfile.write(ET.tostring(dbroot, pretty_print=True))

    if close_dbfile:
        dbfile.close()


def _usage(errmsg):
    if len(errmsg) < 1:
        err = 0
    else:
        err = 1
        sys.stderr.write("Error: " + errmsg + "\n\n")
    sys.stderr.write(__doc__)
    sys.exit(err)

def _print_sections(sect):
    for s in sect:
        _print_item("*** %s" % s, sect[s], 0)

def _print_item(name, value, indent):
    if value.__class__.__name__ == "list":
        if len(name) > 0:
            print "%s%s" % (" "*indent, name)
        for lval in value:
            _print_item('', lval, indent+4)
    elif value.__class__.__name__ == "dict":
        if len(name) < 1 and 'name' in value:
            name = value['name']
        if len(name) > 0:
            print "%s%s" % (" "*indent, name)
        for dnam, dval in value.iteritems():
            _print_item(dnam+':', dval, indent+4)
    else:
        print "%s%s %s" % (" "*indent, name, value)


####################### parse doxml to internal format #######################


def ParseDoxmlFile(doxml_file, sect):
    parser = ET.XMLParser(remove_comments=True, strip_cdata=False)
    tree = ET.parse(doxml_file, parser=parser)
    root = tree.getroot()
    _traverse(root, sect, {})

def _add_doxml_file(doxydir, path, sect):
    name = re.sub("(\.)([hc]$)", "_8\\2.xml", os.path.basename(path))
    doxml = os.path.join(doxydir, name)
    if os.path.isfile(doxml):
        _files.add((name, doxml))
        _find_includes_in(doxydir, doxml, sect)
        return True
    return False

def _find_includes_in(doxydir, filnam, sect):
    tree = ET.parse(filnam)
    root = tree.getroot()
    for topel in root.getchildren():
        if topel.tag == "compounddef" and topel.get("kind") == "file":
            for el in topel.getchildren():
                if el.tag == "includes" and len(el.text) > 0:
                    _add_doxml_file(doxydir, el.text, sect)
                elif el.tag == "innerclass" and len(el.get("refid")) > 0:
                    refid = el.get("refid")
                    fpath = os.path.join(doxydir, refid + ".xml")
                    if os.path.isfile(fpath):
                        for snam in sect:
                            if refid.startswith(snam):
                                _files.add(("<" + el.text + ">", fpath))
                                break
    return

def _traverse(el, sect, entry):
    if el.tag in globals():
        entry = globals()[el.tag](el, sect, entry)
    return entry

def _traverse_children(el, sect, entry):
    for i in el.getchildren():
            entry = _traverse(i, sect, entry)
    return entry

def _attribute_tag(el, entry, name):
    if el.text is not None and len(el.text) > 0:
        entry[name] = el.text
    return entry

def _text_markup(el, sect, entry, name):
    text = _traverse_children(el, sect, [])
    if len(text) > 0:
        entry[name] = text
    return entry


def _list_collector(el, sect, entry, name, init):
    if name not in entry:
        entry[name] = []
    list_entry = _traverse_children(el, sect, {})
    if len(list_entry) > 0:
        if init is not None and len(init) > 0:
            list_entry.update(init)
        entry[name].append(list_entry)
    return entry

def _get_code(entry, path, start, end):
    global _sources, _comment_pattern
    if path not in _sources:
        if not os.path.isfile(path):
            return
        f = open(path, "r")
        _sources[path] = f.readlines()
        f.close()
    code =  "".join(_sources[path][start-1:end])
    entry['code'] = re.sub(_comment_pattern, '', code).rstrip()
    return entry


def compounddef(el, sect, entry):
    t = el.get('kind')
    if t in ['struct', 'union']:
        sect[t].append(_traverse_children(el, sect, {}))
    elif t == 'file':
        entry = _traverse_children(el, sect, entry)
    return entry

def compoundname(el, sect, entry):
    if el.getparent().get('kind') in ['struct', 'union']:
        entry['name'] = el.text
    return entry
    
def sectiondef(el, sect, entry):
    if el.get('kind') in ['enum', 'typedef', 'func', 'define'] or \
       el.getparent().get('kind') in ['struct', 'union']:
        entry = _traverse_children(el, sect, entry)
    return entry


def memberdef(el, sect, entry):
    t = el.get('kind')
    if t in sect:
        sect[t].append(_traverse_children(el, sect, {}))
    elif t == 'variable':
        entry = _list_collector(el, sect, entry, 'variables', None)
    return entry

def location(el, sect, entry):
    f = el.get('bodyfile')
    s = int(el.get('bodystart', -1))
    e = int(el.get('bodyend', -1))
    if f is not None and s > 0 and e > 0:
        p = el.getparent()
        if p.tag == 'compounddef' and p.get('kind') in ['struct', 'union']:
            entry['file'] = {'path':f, 'start':s, 'end':e}
            entry = _get_code(entry, f, s,e)
    return entry


def enumvalue(el, sect, entry):
    return _list_collector(el, sect, entry, 'enumvalues', None)

def name(el, sect, entry):
    return _attribute_tag(el, entry, 'name')

def type(el, sect, entry):
    return _attribute_tag(el, entry, 'type')

def declname(el, sect, entry):
    return _attribute_tag(el, entry, 'declname')

def definition(el, sect, entry):
    return _attribute_tag(el, entry, 'def')

def argsstring(el, sect, entry):
    return _attribute_tag(el, entry, 'args')

def initializer(el, sect, entry):
    return _attribute_tag(el, entry, 'value')

def briefdescription(el, sect, entry):
    return _text_markup(el, sect, entry, 'brief')

def detaileddescription(el, sect, entry):
    return _text_markup(el, sect, entry, 'descr')

def para(el, sect, entry):
    if entry.__class__.__name__ == 'list':
        if el.text is not None and len(el.text.strip()) > 0:
            entry.append({'para' : el.text.strip()})
        else:
            entry = _traverse_children(el, sect, entry)
    return entry

def param(el, sect, entry):
    return _list_collector(el, sect, entry, 'param', None)

def parameterlist(el, sect, entry):
    if entry.__class__.__name__ == 'list':
        entry.append({el.get('kind') : _traverse_children(el, sect, [])})
    return entry

def parameteritem(el, sect, entry):
    if entry.__class__.__name__ == 'list':
        entry.append(_traverse_children(el, sect, {}))
    return entry

def parametername(el, sect, entry):
    if el.text is not None:
        entry['name'] = el.text
    return entry

def parameterdescription(el, sect, entry):
    entry['descr'] = _traverse_children(el, sect, [])
    return entry

def simplesect(el, sect, entry):
    if entry.__class__.__name__ == 'list':
        if el.get('kind') in ['return']:
            entry.append(_text_markup(el, sect, {}, el.get('kind')))
    return entry

def programlisting(el, sect, entry):
    program = _traverse_children(el, sect, '')
    if len(program) > 0:
        if entry.__class__.__name__ == 'list':
            entry.append({'code' : program})
        elif entry.__class__.__name__ == 'dict':
            entry['code'] = program
    return entry

def codeline(el, sect, entry):
    entry += _traverse_children(el, sect, '') + '\n'
    return entry

def highlight(el, sect, entry):
    if el.text is not None:
        entry += el.text
    entry += _traverse_children(el, sect, '')
    if el.tail is not None:
        entry += el.tail
    return entry

def sp(el, sect, entry):
    entry += ' '
    if el.tail is not None:
        entry += el.tail
    return entry



doxygen = _traverse_children
parameternamelist = _traverse_children
osectiondef = sectiondef

########################## Build output tree ################################

def ProcessSections(sect):
    index = {'define':0, 'typedef':1, 'enum':2,
             'struct':3, 'union':3, 'function':4}
    #_print_sections(sect)
    td = {}
    defs = []


    for i in sect['typedef']:
        if i['type'] not in td:
            td[i['type']] = []
        td[i['type']].append({'name':i['name'], 'def':i['def'], 'print':True})

    for i in ['struct', 'union', 'enum']:
        for s in sect[i]:
            if 'name' in s and s['name'].startswith('@'):
                continue
            de  = {'sect':i, 'index':index[i]}
            for key, value in s.iteritems():
                tdk = "%s %s" % (i, value)
                if key == 'name' and tdk in td and len(td[tdk]) == 1:
                    value = td[tdk][0]['name']
                    td[tdk][0]['print'] = False
                de[key] = value
            defs.append(de)
    for t,l in td.iteritems():
        for d in l:
            if not d['print']:
                continue
            df = {'type':t, 'sect':'typedef', 'index':index['typedef']}
            for key, value in d.iteritems():
                if key != 'print':
                    df[key] = value
            defs.append(df)
    for t in ['define', 'function']:
        extra = {'sect':t, 'index':index[t]}
        for s in sect[t]:
            if 'name' in s and not s['name'].startswith('@'):
                de = deepcopy(s)
                de.update(extra)
                defs.append(de)
    defs.sort(_cmpfunc)
    return defs

def _cmpfunc(a, b):
    if 'index' in a and 'index' in b:
        if a['index'] > b['index']:
            return 1
        if a['index'] < b['index']:
            return -1
    if 'name' in a and 'name' in b:
        if a['name'] > b['name']:
            return 1
        elif a['name'] < b['name']:
            return -1
    return 0

############################# BuildDBTree ###################################
def BuildDBTree(defs, title, depth):
    global _escape_pattern, _escape_map

    _escape_pattern = re.compile(r"([\"&<>\x89-\xff])")
    _escape_map = {"'" : '&apos;',
                   '"' : '&quot;',
                   '&' : '&amp;' ,
                   '<' : '&lt;'  ,
                   '>' : '&gt;'   }

    index = -1
    root, section_depth = _make_root(title, depth)

    for d in defs:
        if d['index'] != index:
            index = d['index']
            container, new_depth = _make_container(root, index, section_depth)
        entry_builder = "build_%s_entry" % d['sect']
        if entry_builder in globals():
            globals()[entry_builder](container, d, new_depth)

    return root


def _make_root(title, depth):
    if title is None:
        section_depth = depth
        root = ET.Element('root')
    else:
        root = _make_section(None, title, depth=depth)
        section_depth = depth + 1
    return root, section_depth

def _make_container(parent, index, section_depth):
    container_builder = ['build_define_container',
                         'build_typedef_container',
                         'build_enum_container',
                         'build_struct_container',
                         'build_function_container']

    if container_builder[index] in globals():
        return globals()[container_builder[index]](parent, section_depth)

    return parent, section_depth


def _make_section(parent, title, depth):
    if depth is None or depth.__class__.__name__ != 'int' or depth > 4:
        ttl = ET.Element('para')
        ttl.text = _escape_text(title.strip())
        section = ET.Element('para')
        if parent is not None:
            parent.append(ttl)
    else:
        if depth == 0:
            tag = 'chapter'
        else:
            tag = "sect%d" % depth

        section = ET.Element(tag)

        ttl = ET.Element('title')
        ttl.text = _escape_text(title.strip())
        section.append(ttl)

    if parent is not None:
        parent.append(section)

    return section


def _make_refentry(parent, title, name, volid="3"):
    if parent is not None:
        parent.append(ET.Element('beginpage'))

    ref_entry = ET.Element('refentry', {'id':name})
    parent.append(ref_entry)

    ref_meta = ET.Element('refmeta')
    ref_entry.append(ref_meta)

    ref_entry_title = ET.Element('refentrytitle')
    ref_entry_title.text = title.strip()
    ref_meta.append(ref_entry_title)

    man_volnum = ET.Element('manvolnum')
    man_volnum.text = volid
    ref_meta.append(man_volnum)

    if parent is not None:
        parent.append(ref_entry)

    return ref_entry


def _make_refdiv(parent, divnam):
    div = ET.Element("ref%sdiv" % divnam)
    if parent is not None:
        parent.append(div)
    return div

def _make_refnamediv(parent, names, brief=None):
    if brief is not None and len(brief.strip()) > 0:
        div = _make_refdiv(parent, 'name')

        if names.__class__.__name__ == 'str':
            names = [names]

        for n in names:
            el = ET.Element('refname')
            el.text = n.strip()
            div.append(el)

        ref_purpose = ET.Element('refpurpose')
        ref_purpose.text = brief.strip()
        div.append(ref_purpose)

        if parent is not None:
            parent.append(div)
    else:
        div = None
    return div

def _make_refsynopsisdiv(parent, typ, name, params, info=None):
    div = _make_refdiv(parent, 'synopsis')

    if info is not None and len(info.strip()) > 0:
        func_synopsis_info = ET.Element('funcsynopsisinfo')
        func_synopsis_info.text = _escape_text(info.strip())
        div.append(func_synopsis_info)

    _make_funcprototype(div, typ, name, params)

    if parent is not None:
        parent.append(div)
    return div

def _make_refsection(parent, title):
    refsect1 = ET.Element('refsect1')

    ttl = ET.Element('title')
    ttl.text = _escape_text(title.strip())
    refsect1.append(ttl)
    
    if parent is not None:
        parent.append(refsect1)
    return refsect1


def _make_funcprototype(parent, typ, name, params):
    def format_type(typ):
        stripped_type = typ.strip()
        if " " in stripped_type:
            return stripped_type
        return "%s " % stripped_type

    func_prototype = ET.Element('funcprototype')

    func_def = ET.Element('funcdef')
    func_def.text = format_type(_escape_text(typ.strip()))
    func_prototype.append(func_def)

    function = ET.Element('function')
    function.text = _escape_text(name)
    func_def.append(function)

    for p in params:
        paramdef = ET.Element('paramdef')
        paramdef.text = format_type(_escape_text(p['type'].strip()))
        func_prototype.append(paramdef)

        if 'declname' in p and len(p['declname'].strip()) > 0:
            parameter = ET.Element('parameter')
            parameter.text = _escape_text(p['declname'].strip())
            paramdef.append(parameter)

    if parent is not None:
        parent.append(func_prototype)

    return func_prototype

def _make_varname(parent, name):
    varname = ET.Element('varname')
    varname.text = _escape_text(name.strip())
    if parent is not None:
        parent.append(varname)
    return varname

def _make_para(parent, text=None):
    para = ET.Element('para')
    if text is not None:
        para.text = "%s" % text
    if parent is not None:
        parent.append(para)
    return para

def _make_code(parent, code):
    screen = ET.Element('programlisting')
    screen.text = ET.CDATA(code)
    if parent is not None:
        parent.append(screen)
    return screen

def _unmarked_text(elements):
    return _escape_text(_collect_text(elements))

def _collect_text(elements):
    text = ''

    if elements is not None:
        typ = elements.__class__.__name__

        if typ == 'list':
            for entry in elements:
                text += _collect_text(entry)
        elif typ == 'dict':
            for name, value in elements.iteritems():
                if name not in ['param', 'return']:
                    text += (" %s" % value)
        else:
            text += (" %s" % elements)

    return text.strip()

def _add_marked_text(parent, elements):
    added = False
    for el in elements:
        for name, value in el.iteritems():
            if name == 'para':
                para = _make_para(parent, value)
                added = True
    return added

def _make_param_list(parent, descr, render='table'):
    entries = 0
    if render == 'table':
        param_list, tbody = _make_list_table(['1*', '5*'])
        for en in descr:
            if 'param' in en:
                for p in en['param']:
                    if len(set(p) & set(['name', 'descr'])) != 2:
                        continue
                    _add_list_table_row(tbody, p['name'], p['descr'])
                    entries += 1
    elif render == 'varlist':
        param_list = ET.Element('variablelist')
        for en in descr:
            if 'param' in en:
                for p in en['param']:
                    if len(set(p) & set(['name', 'descr'])) != 2:
                        continue
                    name = _make_varname(None, p['name'])
                    item = _add_varlist_item(param_list, name, '')
                    _add_marked_text(item, p['descr'])
                    entries += 1
    if entries > 0:
        if parent is not None:
            parent.append(param_list)
        return param_list
    return None

def _make_variable_list(parent, variables, ratio, render='table'):
    entries = 0
    if render == 'table':
        variable_list, tbody = _make_list_table(['1*', '%d*' % ratio])
        for en in variables:
            if 'brief' in en:
                _add_list_table_row(tbody, en['name'], en['brief'])
                entries += 1
    elif render == 'varlist':
        variable_list = ET.Element('variablelist')
        for en in variables:
            if 'brief' in en:
                name = _make_varname(None, en['name'])
                item = _add_varlist_item(variable_list, name, '')
                _add_marked_text(item, en['brief'])
                entries += 1
    if entries > 0:
        if parent is not None:
            parent.append(variable_list)
        return variable_list
    return None


def _make_list_table(colwidths):
        table = ET.Element('table',
                           {'align'     : 'left',
                            'frame'     : 'none',
                            'colsep'    : '0',
                            'rowsep'    : '0',
                            'rowheader' : 'norowheader'})

        tgroup = ET.Element('tgroup', {'cols':'%s' % len(colwidths)})
        table.append(tgroup)

        for i in colwidths:
            tgroup.append(ET.Element('colspec', {'colwidth':i}))

        tbody = ET.Element('tbody')
        tgroup.append(tbody)

        return table, tbody

def _add_list_table_row(parent, name, descr):
    row = ET.Element('row')
    parent.append(row)
    
    name_entry = ET.Element('entry', {'align':'left', 'valign':'top'})
    _make_varname(name_entry, name)
    row.append(name_entry)

    desc_entry = ET.Element('entry', {'align':'left', 'valign':'top'})
    _add_marked_text(desc_entry, descr)
    _make_para(desc_entry, ' ')
    row.append(desc_entry)


def _add_varlist_item(varlist, key, brief=None, descr=None):
    list_entry = ET.Element('varlistentry')
    varlist.append(list_entry)

    term = ET.Element('term')
    if ET.iselement(key):
        term.append(key)
    else:
        term.text = key.strip()
    list_entry.append(term)

    list_item = ET.Element('listitem')
    if brief is not None:
        if len(brief) > 0:
            list_item.text = "- %s" % brief
    else:
        list_item.text = ":"

    if descr is not None:
        if descr.__class__.__name__ == 'list':
            list_item.extend(descr)
        else:
            list_item.append(descr)

    list_entry.append(list_item)
                        
    return list_item
    
def _escape_text(text):
    global _escape_pattern, _escape_map

    def find_replacement(match):
        pattern = match.group(1)
        if pattern == None:
            return ''
        elif pattern in _escape_map:
            return _escape_map[pattern]
        else:
            return ''

    return re.sub(_escape_pattern, find_replacement, text)

def build_define_container(parent, section_depth):
    section = _make_section(parent, "Preprocessor definitions", section_depth)
    parent.append(section)
    
    if section_depth < 2:
        table, container = _make_list_table(['1*', '2*'])
        section.append(table)
    else:
        container = ET.Element('variablelist')
        section.append(container)
    return container, section_depth + 1

def build_define_entry(parent, define, depth):
    if len(set(['name', 'value', 'brief']) & set(define)) != 3:
        return

    name  = define['name']

    if depth < 3:
        descr = define['brief']
        if 'descr' in define:
            descr += define['descr']
        _add_list_table_row(parent, name, define['brief'])
    else:
        brief = _unmarked_text(define['brief'])
        cdata = "#define %s %s\n" % (name, define['value'])
        para = _make_para(None) 
        code = _make_code(para, cdata)    

        _add_varlist_item(parent, name, brief, para)


def build_enum_entry(parent, enum, depth):
    if 'name' not in enum:
        return
    else:
        name = enum['name']

    if depth < 2:
        if len(set(['brief', 'enumvalues']) & set(enum)) != 2:
            return
        ref_entry = _make_refentry(parent, "Enumeration %s" % name, name, '7')

        _make_refnamediv(ref_entry, name, _unmarked_text(enum['brief']))

        elist = _make_variable_list(None, enum['enumvalues'], 3)
        if elist is not None:
            _make_refsection(ref_entry, 'Values').append(elist)
    else:
        return


def build_struct_entry(parent, struct, depth, kind='struct'):
    if 'name' not in struct:
        return
    else:
        name = struct['name']

    if depth < 2:
        if len(set(['brief', 'code', 'variables']) & set(struct)) != 3:
            return
        title = "%s %s" % (kind.capitalize(), name)
        ref_entry = _make_refentry(parent, title, name, '7')

        _make_refnamediv(ref_entry, name, _unmarked_text(struct['brief']))

        synop_div = _make_refdiv(ref_entry, 'synopsis')

        synopsis = ET.Element('synopsis')
        synopsis.text = ET.CDATA(struct['code'])
        synop_div.append(synopsis)

        vlist = _make_variable_list(None, struct['variables'], 5)
        if vlist is not None:
            _make_para(synop_div, "Where")
            _make_para(synop_div).append(vlist)

        if 'descr' in struct:
            descr_div = _make_refsection(None, 'Description')
            if _add_marked_text(descr_div, struct['descr']):
                ref_entry.append(descr_div)
    else:
        return


def build_union_entry(parent, union, depth):
    build_struct_entry(parent, union, depth, 'union')

def build_function_entry(parent, func, depth):
    if 'name' not in func:
        return
    else:
        name = func['name']

    if depth < 2:
        if len(set(['def', 'type', 'brief']) & set(func)) != 3:
            return

        if 'param' in func:
            params = func['param']
        else:
            params = [{'type':'void', 'declname':''}]

        ref_entry = _make_refentry(parent, "Function %s" % name, name, '3')

        _make_refnamediv(ref_entry, name, _unmarked_text(func['brief']))
        
        synop_div = _make_refsynopsisdiv(ref_entry, func['type'], name, params)

        if 'descr' in func:
            descr = func['descr']
            plist = _make_param_list(None, descr)
            if plist is not None:
                _make_para(synop_div, 'Where')
                _make_para(synop_div).append(plist)

            descr_div = _make_refsection(None, 'Description')
            if _add_marked_text(descr_div, descr):
                ref_entry.append(descr_div)
                
            code = ret = None
            for d in descr:                
                if 'code' in d:
                    code = d['code']
                if 'return' in d:
                    ret = d['return']
            if ret is not None:
                return_div = _make_refsection(ref_entry, 'Return value')
                _add_marked_text(return_div, ret)
            if code is not None:
                example_div = _make_refsection(ref_entry, 'Example')
                _make_code(example_div, d['code'])
    else:
        if len(set(['def', 'args', 'brief']) & set(func)) != 3:
            return

        section = _make_section(parent, "%s()" % name, depth+1)

        synopsis = _make_section(section, "SYNOPSIS", depth+2)
        _make_code(_make_para(synopsis), "%s%s" % (func['def'], func['args']))
        _make_para(synopsis, "%s() %s" % (name, _unmarked_text(func['brief'])))

        if 'descr' not in func:
            return

        valid = False
        descr = _make_section(None, "DESCRIPTION", depth+2)
        plist = _make_param_list(None, func['descr'])
        if plist is not None:
            valid = True
            descr.append(plist)
            if _add_marked_text(descr, func['descr']):
                valid = True
        if valid:
            section.append(descr)

#############################################################################

if __name__ == '__main__':
    _main()
