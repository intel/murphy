ifneq ($(strip $(MAKECMDGOALS)),)
%:
	$(MAKE) -C .. $(MAKECMDGOALS)
else
all:
	$(MAKE) -C .. all
endif

%.crt:
	cert="$@"; 							    \
	make -f /etc/ssl/certs/Makefile $@ && 				    \
	mv $${cert%.crt}.key $${cert%.crt}.key.protected &&		    \
	openssl rsa -in $${cert%.crt}.key.protected -out $${cert%.crt}.key
