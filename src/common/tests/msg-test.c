#include <murphy/common.h>

int main(int argc, char *argv[])
{
    mrp_msg_t *msg, *decoded;
    char       buf[1024], *tag;
    void      *data, *encoded;
    ssize_t    size;
    int        i, len;
    char       default_argv1[] = \
	"output=one_cb(): #0: 'one'\none_cb(): #0: 'one'\n";
    char      *default_argv[] = { default_argv1, default_argv1 };
    int        default_argc = 2;
    

    if (argc < 2) {
	argc = default_argc;
	argv = default_argv;
    }

    if ((msg = mrp_msg_create(NULL)) != NULL) {
	for (i = 1; i < argc; i++) {
	    tag  = argv[i];
	    data = strchr(tag, '=');
	    
	    if (data != NULL) {
		len = (ptrdiff_t )(data - (void *)tag);
		if (len > (int)sizeof(buf) - 1)
		    len = sizeof(buf) - 1;
		strncpy(buf, tag, len);
		buf[len] = '\0';
		tag = buf;
		data++;
		size = strlen((char *)data) + 1;
	    }
	    else {
		data = NULL;
		size = 0;
	    }

	    if (!mrp_msg_append(msg, tag, data, size)) {
		mrp_log_error("Failed to add field %s='%s' to message.",
			      tag, (char *)data);
		exit(1);
	    }
	}
    }

    mrp_msg_dump(msg, stdout);

    size = mrp_msg_default_encode(msg, &encoded);
    
    if (size < 0) {
	mrp_log_error("Failed to encode message.");
	exit(1);
    }
    
    decoded = mrp_msg_default_decode(encoded, size);

    if (decoded == NULL) {
	mrp_log_error("Failed to decode message.");
	exit(1);
    }

    mrp_msg_dump(decoded, stdout);

    return 0;
}
