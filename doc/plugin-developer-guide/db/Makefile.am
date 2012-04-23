TOP_SRCDIR = $(abs_top_srcdir)/src

vpath %.lyx ../lyx

include $(MRP_MAKE_DOCRULES)


FIGURES_SVG = db-layers.svg
FIGURES_PNG = $(FIGURES_SVG:.svg=.png)
FIGURES_PDF = $(FIGURES_SVG:.svg=.pdf)

FIGURES = $(FIGURES_SVG) $(FIGURES_PNG) $(FIGURES_PDF)

LYX_DOCS = murphy-db.lyx
XML_DOCS = $(LYX_DOCS:.lyx=.xml)

MQL_GRAMMAR = mql-grammar.xml
GENERATED_GRAMMARS = $(MQL_GRAMMAR)

TARGETS = $(FIGURES) plugin-developer-guide.xml

all-am: $(TARGETS)

figures: $(FIGURES_SVG)
	for svg in $^ ; do \
	    pdf=`basename($${svg/.svg/.pdf})` ; \
	    png=$${pdf/.pdf/.png} ; \
	    cp $$svg . ; \
	    $(MRP_INKSCAPE) --export-area-drawing --export-pdf=$$pdf $$svg ; \
	    ${MRP_INKSCAPE) --export-area-drawing --export-png=$$png $$svg ; \
	done

$(MQL_GRAMMAR): $(TOP_SRCDIR)/murphy-db/mql/mql-scanner.l \
                $(TOP_SRCDIR)/murphy-db/mql/mql-parser.y
	$(MRP_ABNF) $+ > $@

xml_docs: $(LYX_DOCS)
	for lyx_file in $^ ; do \
		lyxml_file=$${lyx_file/.lyx/.xml} ; \
		xml_file=`basename $$lyxml_file` ; \
		rm -f $$lyxml_file ; \
		$(MRP_LYX) --export docbook-xml $$lyx_file && \
		$(MRP_DBLYXFIX)  $$lyxml_file $$xml_file && \
		rm -f $$lyxml_file ; \
	done


plugin-developer-guide.xml: plugin-developer-guide.lyx \
                            $(GENERATED_GRAMMARS) xml_docs
	rm -f ../lyx/plugin-devloper-guide.xml ; \
	$(MRP_LYX) --export docbook-xml ../lyx/plugin-developer-guide.lyx && \
	$(MRP_DBLYXFIX) ../lyx/plugin-developer-guide.xml $@ && \
	rm -f ../lyx/plugin-devloper-guide.xml

clean-local:
	rm -f $(TARGETS) $(FIGURES) $(GENERATED_GRAMMARS) $(XML_DOCS) \
		../lyx/*.xml
