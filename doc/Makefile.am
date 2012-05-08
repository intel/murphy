MANDEFS  = plugin-developer-guide.x
MANUALS  = $(MANDEFS:.x=)
TARGETS  =
DOCFILES =

vpath %.xml plugin-developer-guide/db

include $(MRP_MAKE_DOCRULES)


if BUILD_DOCUMENTATION
DOCDIRS = plugin-developer-guide
endif

if BUILD_PDF_DOCUMENTS
TARGETS += pdf_targets
DOCFILES += $(MANDEFS:.x=.pdf)
endif


SUBDIRS = $(DOCDIRS)

doc_DATA = CODING-STYLE # $(DOCFILES)


all-am: $(TARGETS)

pdf_targets: plugin-developer-guide.pdf

plugin-developer-guide.pdf: plugin-developer-guide.xml

clean-local:
	rm -f *.pdf scripts/*~

