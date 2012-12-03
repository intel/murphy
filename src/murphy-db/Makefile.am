if HAVE_CHECK
TESTDIR = tests
else
TESTDIR =
endif

SUBDIRS = mdb mqi mql include $(TESTDIR)

pkgconfigdir = $(libdir)/pkgconfig
nodist_pkgconfig_DATA = murphy-db.pc

murphy-db.pc: murphy-db.pc.in
	sed -e 's![@]prefix[@]!$(prefix)!g' \
            -e 's![@]exec_prefix[@]!$(exec_prefix)!g' \
            -e 's![@]includedir[@]!$(includedir)!g' \
            -e 's![@]libdir[@]!$(libdir)!g' \
            -e 's![@]PACKAGE_VERSION[@]!$(PACKAGE_VERSION)!g' \
        $< > $@

generate-linker-scripts:
	$(MAKE) -C mdb $@
	$(MAKE) -C mqi $@
	$(MAKE) -C mql $@

clean-local:
	rm -f *~ murphy-db.pc
