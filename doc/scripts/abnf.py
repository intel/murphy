#!/usr/bin/env python
# -*- coding: latin-1 -*-

#
# this script produces ABNF description of grammars implemented by flex/bison
# ABNF stands for Augmented Backus-Naur Form as it is specified by the IETF
# RFC 5234
#
#

import os, sys, re

def canonic_optlist(olist):
    canonic  = ""
    sep = ""
    if len(olist) > 0:
        members = olist.split(' ')
        nmember = len(members)
        if nmember > 0:
            for member in members:
                tag = member.strip()
                if len(tag) < 1:
                    continue
                canonic += sep
                sep = " "
                if tag.startswith("TKN_"):
                    canonic += tag[4:]
                else:
                    canonic += tag
    return canonic


def canonic_complist(clist): 
    stripped = clist.strip()
    canonic  = ""
    sep = ""
    if len(stripped) > 0:
        options = stripped.split('|')
        noption = len(options)
        if noption == 1:
            stripped_option = options[0].strip()
            canonic += canonic_optlist(stripped_option)
        elif noption > 1:
            if len(options[0].strip()) < 1:
                canonic = "["
                close   = "]"
            else:
                canonic = "("
                close   = ")"
            for option in options:
                stripped_option = option.strip()
                if len(stripped_option) > 0:
                    canonic += sep + canonic_optlist(stripped_option)
                    sep = "|"
            canonic += close
    return canonic


def print_canonic_rules(toprules):
    print terminals + "\n"
    for rule in toprules:
        print canonic_rule(toprules, rule, 0, " ") + "\n"


def canonic_rule(toprules, name, rdepth, gap):
    canonic = ""
    stripped_name = name.strip()
    toplevel = name in toprules 

    if stripped_name in rules:
        rule = rules[stripped_name]
        if toplevel:
            canonic += "<" + name + "> ="
        if rule.startswith("(") and rule.endswith(")"):
            stripped_rule = rule[1:-1].strip()
            if rdepth < 1:
                close = ""
            else:
                canonic += " ("
                close = ")"
                gap = ""
        elif rule.startswith("[") and rule.endswith("]"):
            stripped_rule = rule[1:-1].strip()
            canonic += " ["
            close = "]"
            gap = ""
        else:
            stripped_rule = rule.strip()
            close = ""

        escaped_rule = stripped_rule.replace("\"|\"","Ö")
        members = escaped_rule.split("|")
        sep = ""
        for member in members:
            tags = member.replace("Ö","\"|\"").split(' ')

            if tags[0].strip() == name:
                rept = ")"
            else:
                rept = ""
                canonic += sep
                sep = " /"

            first = True
            for tag in tags:
                tagname = tag.strip()
                if len(tagname) < 1:
                    continue
                if first and tagname == name:
                    canonic += " *("
                    gap = ""
                elif tagname in toprules:
                    canonic += gap + "<" + tagname + ">"
                    gap = " "
                else:
                    canonic += canonic_rule(toprules, tagname, rdepth+1, gap)
                    gap = " "
                first = False
            canonic += rept

        canonic += close
    else:
        canonic += gap + stripped_name

    return canonic


lfile      = sys.argv[1]
yfile      = sys.argv[2]
inputdir   = os.path.dirname(yfile)
scriptdir  = sys.path[0] 
start      = ""
grammar    = False
prologue   = False
comment    = False
toplevel   = False
depth      = 0
result     = ""
components = ""
terminals  = ""
rules      = {}
top_rules  = []
pos        = 0
end        = 0



if not inputdir == "" and not inputdir.endswith("/"):
    inputdir = "%s/" % inputdir

if not scriptdir == "" and not scriptdir.endswith("/"):
    scriptdir = "%s/" % scriptdir


with open(lfile, "r") as l:
    skip = False
    for line in l.xreadlines():
        if skip:
            if line.startswith("%}"):
                skip = False
        else:
            if line.startswith("%{"):
                skip = True
            elif line.startswith("%%"):
                break
            elif line.startswith("/*"):
                continue
            
            stripped_line = line[:-1].strip()

            if len(stripped_line) < 3:
                continue

            sp  = stripped_line.find(" ")
            tab = stripped_line.find("\t")
            
            if sp > 0 and (tab < 0 or tab > sp):
                delim = sp
            elif tab > 0 and (sp < 0 or sp > tab):
                delim = tab
            else:
                continue

            key   = stripped_line[:delim].strip()
            value = stripped_line[delim:].strip()

            if key[0] == "%" or len(value) < 1:
                continue

            if value.isalpha():
                terminals += key + " = \"" + value.upper() + "\"\n"
            else:
                if value[0] == "\\" and len(value) == 2:
                    rules[key] = "\"%s\"" % value[1]
                elif len(value) == 1:
                    rules[key] = "\"%s\"" % value[0]
                else:
                    if value.find("[") < 0 and value.find("(") < 0:
                        rules[key] = "\"%s\"" % value.replace("/","")
                    else:
                        terminals += "<" + key.lower() + "> = " + value + "\n"
                        rules[key] = "<" + key.lower() + ">"

    l.close()


with open(yfile, "r") as y:
    for line in y.xreadlines():
        if grammar:
            if line.startswith("%%"):
                break
            elif line.startswith("/*#") and line.endswith("#*/\n"):
                key = line[3:-4].strip()
                if key == "toplevel":
                    toplevel = True
            else:
                pos = 0
                end = len(line)
                if depth == 0:
                    colon = line.find(":")
                    slash_star = line.find("/*")
                    if colon > 0 and (slash_star < 0 or slash_star > colon):
                        result = line[:colon]
                        components = ""
                        pos = colon + 1
                while pos < end-1:
                    if comment:
                        star_slash = line.find("*/", pos)
                        if star_slash < 0:
                            break
                        pos = star_slash + 2
                        comment = False
                    else:
                        slash_star  = line.find("/*", pos)
                        open_brace  = line.find("{" , pos)
                        close_brace = line.find("}" , pos)
                        if slash_star >= 0 and \
                           (open_brace  < 0 or slash_star < open_brace) and \
                           (close_brace < 0 or slash_star < close_brace):
                           
                           comment = True
                           pos = slash_star + 2
                           continue
                        if open_brace >= 0 and \
                           (close_brace < 0 or open_brace < close_brace) and \
                           (slash_star  < 0 or open_brace < slash_star ):

                            if depth == 0:
                                components += line[pos: open_brace]
                            depth += 1
                            pos = open_brace + 1
                            continue
                        if close_brace >= 0 and \
                           (open_brace < 0 or close_brace < open_brace) and \
                           (slash_star < 0 or close_brace < slash_star):
                            depth -= 1
                            pos = close_brace + 1
                            continue
                        if depth == 0:
                            semicolon = line.find(";", pos)
                            if semicolon >= pos:
                                components += line[pos: semicolon]
                                rules[result] = canonic_complist(components)
                                if toplevel:
                                    top_rules.append(result)
                                result = ""
                                components = ""
                                toplevel = False
                            else:
                                components += line[pos: -1]
                        break
        else:
            if prologue:
                if line.startswith("%}"):
                    prologue = False
            elif line.startswith("%{"):
                prologue = True
            elif line.startswith("%start"):
                start = line[6:].strip()
            elif line.startswith("%%"):
                grammar = True
    y.close()



print_canonic_rules(top_rules)



