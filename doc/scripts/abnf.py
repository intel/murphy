#!/usr/bin/env python
# -*- coding: latin-1 -*-

#
# this script produces ABNF description of grammars implemented by flex/bison
# ABNF stands for Augmented Backus-Naur Form as it is specified by the IETF
# RFC 5234
#
#

import os, sys, re

def regexp_to_abnf(string):
    return regexp_parse(string, 0)[0]

def regexp_parse(string, index):
    stack = []
    precedence = 0
    escape = False

    while index < len(string):
        c = string[index]

        insert = False

        if c == "\\":
            escape = True
        else:
            if c.isalnum() or escape:
                if c == "\"":
                    new_string = "DQUOTE"
                elif c == " ":
                    new_string = "SP"
                else:
                    new_string = "\"%s\"" % c
                new_precedence = 100
                escape = False
            elif c == ".":
                new_precedence = 90
                new_string = "VCHAR"
            elif c == "{":
                new_precedence = 100
                if index+1 >= len(string) or string.find("}", index+1) < 0:
                    new_string = "\"{\""
                else:
                    begin = index+1
                    end = string.find("}", begin)
                    new_string = string[begin:end]
                    index = end
            elif c == "(":
                new_precedence = 80
                result, index = regexp_parse(string, index+1)
                if len(result) < 1:
                    index += 1
                    continue
                new_string = "(" + result + ")"
            elif c == ")":
                break 
            elif c == "[":
                new_precedence = 70
                new_string, index = regexp_character_class(string, index)
            elif c == "*":
                new_precedence = 43
                new_string = "*"
                insert = True
            elif c == "+":
                new_precedence = 42
                new_string = "1*"
                insert = True
            elif c == "?":
                new_precedence = 40
                new_string = "0*1"
                insert = True
            elif c == "|":
                new_precedence = 30
                new_string = "/"
            else:
                new_precedence = 100
                if c == "\"":
                    new_string = "DQUOTE"
                elif ord(c) < ord(' '):
                    new_string = "\%x%x" % ord(c)
                else:
                    new_string = "\"" + c + "\""

            if insert:
                last = len(stack) - 1
                if last >= 0:
                    stack.insert(last, (stack[last][0], new_string))
                    stack = regexp_merge(new_precedence, stack)
            else:
                stack = regexp_merge(new_precedence, stack)
                stack.append((new_precedence, new_string))

            precedence = new_precedence
        index += 1

    if len(stack) < 1:
        result = ""
    else:
        result = regexp_merge(-1, stack)[0][1]


    if result.startswith("(") and result.endswith(")"):
        strip = True
        balance = 0
        for c in result[1:-1]:
            if c == "(":
                balance += 1
            elif c == ")":
                balance -= 1
                if balance < 0:
                    strip = False
                    break
        if strip and balance == 0:
            result = result[1:-1]

    return (result, index)

def regexp_character_class(string, index):
    cnt = 0
    result = ""
    backslash = False
    sep = ""

    if string[index+1] == "^":
        index += 1
        ranges = [(32, 126)]
        escape = False
        while index+1 < len(string):
            index += 1
            char = string[index]

            if char == "]":
                break

            if char == "\\":
                escape = True
                continue
            
            c = ord(char)

            if escape:
                if char == "n":
                    c = 10
                if char == "t":
                    c = 9
                escape = False

            for r in ranges:
                if c >= r[0] and c <= r[1]:
                    ranges.remove(r)
                    if c == r[0]:
                        if c != r[1]:
                            ranges.append((r[0]+1, r[1]))
                        break
                    elif c == r[1]:
                        if c != r[0]:
                            ranges.append((r[0], r[1]-1))
                        break
                    else:
                        ranges.append((r[0], c-1))
                        ranges.append((c+1, r[1]))
                        break

        lower = False
        upper = False
        digit = False
        for r in ranges:
            if r[0] <= 65 and r[1] >= 90:
                upper = r
            if r[0] <= 97 and r[1] >= 122:
                lower = r
            if r[0] <= 48 and r[1] >= 57:
                digit = r
        if lower and upper:
            ranges = regexp_range_extract(ranges, 97,122)
            ranges = regexp_range_extract(ranges, 65,90)
            result += sep + "ALPHA"
            sep = " / "
            cnt += 1
        if digit:
            ranges = regexp_range_extract(ranges, 48,57)
            result += sep + "DIGIT"
            sep = " / "
            cnt += 1
        ranges.sort(regexp_range_sort)
        for r in ranges:
            if r[0] == r[1]:
                result += sep + "%x" + "%x" % r[0]
            else:
                result += sep + "%x" + "%x-%x" % r
            sep = " / "
            cnt += 1
    else:
        while index+1 < len(string):
            index += 1
            c = string[index]

            if c == "]":
                break

            if c == "\\":
                if not backslash:
                    backslash = True
                    continue
            else:
                if backslash:
                    backslash = False
                    c = regexp_escape(c)
                elif c == " ":
                    c = "SP"
                elif c == "\"":
                    c = "DQUOTE"
                else:
                    if string[index:index+3] == "0-9":
                        index += 2
                        c = "DIGIT"
                    elif string[index:index+6] == "a-zA-Z" or \
                            string[index:index+6] == "A-Za-z":
                        index += 5
                        c = "ALPHA"
                    elif index < len(string)-2 and string[index+1] == "-" and \
                            ((string[index].isalpha() and \
                              string[index+2].isalpha()) or \
                             (string[index].isdigit() and \
                              string[index+2].isdigit())):
                        index += 2
                        start = ord(c)
                        end = ord(string[index])+1
                        sep2 = ""
                        c = ""

                        for i in range(start, end):
                            c += sep2 +  "\"" + chr(i) + "\""
                            sep2 = " / "
                    else:
                        c = "\"" + c + "\""

            result += sep + c
            sep = " / "
            cnt += 1

    if cnt > 1:
        result = "(" + result + ")"

    return (result, index)

def regexp_escape(char):
    if char == "n":
        return "CRLF"
    elif char == "t":
        return "HTAB"
    elif char == " ":
        return "SP"
    elif char == "\"":
        return "DQUOTE"
    elif char == "\\":
        return "\"\\\""
    elif char == "^":
        return "\"^\""

    return "\"" + char + "\""


def regexp_merge(new_precedence, stack):
    last = len(stack) - 1
    precedence = 0

    if last >= 0:
        for merge in range(last,-1,-1):
            precedence = stack[merge][0]

            if new_precedence >= precedence:
                break

        append = False
        string = ""
        sep = ""

        for i in range(merge,last+1):
            element = stack[i][1]
            string += sep + element
            if element.find("*") == len(element)-1:
                sep = ""
            else:
                sep = " "
            append = True


        for i in range(last,merge-1,-1):
            stack.pop()

        if append:
            stack.append((new_precedence, string))

    return stack

def regexp_range_extract(ranges, l,h):
    for r in ranges:
        if r[0] <= l and r[1] >= h:
            ranges.remove(r)
            if l == r[0] and h < r[1]:
                ranges.append((h+1, r[1]))
            elif l > r[0] and h == r[1]:
                ranges.append((r[0], l-1))
            elif l > r[0] and h < r[1]:
                ranges.append((r[0],l-1))
                ranges.append((h+1,r[1])) 
            break
    return ranges

def regexp_range_sort(a,b):
    if a[0] < b[0]:
        return -1
    elif a[0] > b[0]:
        return +1
    return 0


def component_list(input_list):
    output_list  = ""
    sep = ""
    if len(input_list) > 0:
        components = input_list.split(' ')
        ncomponent = len(components)
        if ncomponent > 0:
            for component in components:
                name = component.strip()
                if len(name) < 1:
                    continue
                output_list += sep
                sep = " "
                if name.startswith("TKN_"):
                    output_list += name[4:]
                else:
                    output_list += name
    return output_list


def rule_list(rule_def): 
    stripped_rule_def = rule_def.strip()
    rlist = ""
    sep = ""
    if len(stripped_rule_def) > 0:
        rules = stripped_rule_def.split('|')
        nrule = len(rules)
        if nrule == 1:
            stripped_rule = rules[0].strip()
            rlist += component_list(stripped_rule)
        elif nrule > 1:
            if len(rules[0].strip()) < 1:
                rlist = "["
                close = "]"
            else:
                rlist = "("
                close = ")"
            for rule in rules:
                stripped_rule = rule.strip()
                if len(stripped_rule) > 0:
                    rlist += sep + component_list(stripped_rule)
                    sep = "|"
            rlist += close
    return rlist


def print_abnf_rules():
    name_len = 0
    prologue = "<![CDATA["
    epilogue = "]]>"
    extra_linefeed = ""

    for rule in abnf:
        name_len = max(len(rule[0]), name_len)

    for rule in abnf:
        line   = rule[0].ljust(name_len) + " ="
        words  = rule[1].split(' ')
        margin = len(line)
        width  = margin

        for word in words:
            wl = len(word) + 1
            if width + wl > line_width:
                line += "\n".ljust(margin+2)
                width = margin
            line += " " + word
            width += wl

        if rule[0].isupper():
            extra_linefeed = ""
        else:
            if len(extra_linefeed) < 1:
                print "\n"
            extra_linefeed = "\n"
        
        print prologue + line + extra_linefeed
        prologue = ""
    print epilogue

def make_abnf_rules(topresults):
    for result in topresults:
        abnf.append( (result, abnf_rule(topresults, result, 0, " ")) )


def abnf_rule(topresults, result, rdepth, gap):
    canonic   = ""
    component = result.strip()
    toplevel  = component in topresults 

    if component in results:
        rule_list = results[component]
        if rule_list.startswith("(") and rule_list.endswith(")"):
            stripped_rule_list = rule_list[1:-1].strip()
            if rdepth < 1:
                close = ""
            else:
                canonic += " ("
                close = ")"
                gap = ""
        elif rule_list.startswith("[") and rule_list.endswith("]"):
            stripped_rule_list = rule_list[1:-1].strip()
            canonic += " ["
            close = "]"
            gap = ""
        else:
            stripped_rule_list = rule_list.strip()
            close = ""

        escaped_rule_list = stripped_rule_list.replace("\"|\"","Ö")
        rules = escaped_rule_list.split("|")
        sep = ""
        for rule in rules:
            components = rule.replace("Ö","\"|\"").split(' ')

            if components[0].strip() == result:
                rept = ")"
            else:
                rept = ""
                canonic += sep
                sep = " /"

            first = True
            for component in components:
                component_name = component.strip()
                if len(component_name) < 1:
                    continue
                if first and component_name == result:
                    canonic += " *("
                    gap = ""
                elif component_name in topresults:
                    canonic += gap + component_name
                    gap = " "
                else:
                    canonic += abnf_rule(topresults, component_name, \
                                         rdepth+1,gap)
                    gap = " "
                first = False
            canonic += rept

        canonic += close
    else:
        canonic += gap + component

    return canonic


lfile       = sys.argv[1]
yfile       = sys.argv[2]
line_width  = 78
inputdir    = os.path.dirname(yfile)
scriptdir   = sys.path[0] 
lname       = os.path.basename(lfile)
yname       = os.path.basename(yfile)
start       = ""
grammar     = False
prologue    = False
comment     = False
toplevel    = False
depth       = 0
result      = ""
result_def  = ""
results     = {}
top_results = []
abnf        = []
pos         = 0
end         = 0



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
                abnf.append( (key, " \"" + value.upper() + "\"") )
            else:
                if value[0] == "\\" and len(value) == 2:
                    results[key] = "\"%s\"" % value[1]
                elif len(value) == 1:
                    results[key] = "\"%s\"" % value[0]
                else:
                    if value.find("[") < 0 and value.find("(") < 0:
                        results[key] = "\"%s\"" % value.replace("/","")
                    else:
                        abnf.append( (key.lower(), " "+regexp_to_abnf(value)) )
                        results[key] = key.lower()

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
                        result_def = ""
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
                                result_def += line[pos: open_brace]
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
                                result_def += line[pos: semicolon]
                                results[result] = rule_list(result_def)
                                if toplevel:
                                    top_results.append(result)
                                result = ""
                                result_def = ""
                                toplevel = False
                            else:
                                result_def += line[pos: -1]
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



make_abnf_rules(top_results)

print "<!-- XML file was automatically generated from %s and from %s -->\n" % \
    (lname, yname)

print "<screen>"
print_abnf_rules()
print "</screen>"




