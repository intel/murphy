SUBDIRS  = . src doc
doc_DATA = AUTHORS ChangeLog COPYING INSTALL NEWS README

# This is the only way with automake I know of to force 'check-git-hooks'
# to be evaluated before 'all'. If there is a nicer way, I'm all ears...
BUILT_SOURCES = check-git-hooks

###################################
# git hook management
#

install-git-hooks:
	if test -d .git; then                                       \
	    for hook in githooks/???*; do                           \
	        case $$hook in                                      \
	            *.sample|*~|*.swp) continue;;                   \
	        esac;                                               \
	        if test -x $$hook; then                             \
	            echo "Installing git hook $${hook##*/}...";     \
	            cp $$hook .git/hooks;                           \
	            chmod a+x .git/hooks/$${hook##*/};              \
	        fi                                                  \
	    done                                                    \
	fi

check-git-hooks:
	if test -d .git; then                                                \
	    for hook in githooks/???*; do                                    \
	        case $$hook in                                               \
	            *.sample|*~|*.swp) continue;;                            \
	        esac;                                                        \
	        if test -x $$hook -a ! -e .git/hooks/$${hook##*/}; then      \
	            echo "WARNING:";                                         \
	            echo "WARNING: You have an uninstalled git hook $$hook"; \
	            echo "WARNING:";                                         \
	        fi                                                           \
	    done                                                             \
	fi


# cleanup
clean-local:
	rm -f *~
