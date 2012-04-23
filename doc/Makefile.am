MANDEFS  = plugin-developer-guide.x
MANUALS  = $(MANDEFS:.x=)
TARGETS  =
DOCFILES =

if BUILD_DOCUMENTATION
DOCDIRS = plugin-developer-guide/db
endif

if BUILD_PDF_DOCUMENTS
TARGETS += pdf_targets
DOCFILES += $(MANDEFS:.x=.pdf)
endif

SUBDIRS = $(DOCDIRS)

doc_DATA = CODING-STYLE $(DOCFILES)


all-am: $(TARGETS)

pdf_targets:
	for name in $(MANUALS) ; do \
		xml=$${name}/db/$${name}.xml ; \
		pdf=$${name}.pdf ; \
		$(MRP_DBLATEX) --pdf --style=db2latex -P figure.title.top=0 \
			-o $${pdf} $${xml} ; \
	done

clean-local:
	rm -f *.pdf
