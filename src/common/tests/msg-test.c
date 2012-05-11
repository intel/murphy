#include <murphy/common.h>

#include <murphy/common/msg.h>
#include <murphy/common/msg.c>

#define TYPE(type, name) [MRP_MSG_FIELD_##type] = name
const char *types[] = {
    TYPE(INVALID, "invalid"),
    TYPE(STRING , "string" ),
    TYPE(BOOL   , "bool"   ),
    TYPE(SINT8  , "sint8"  ),
    TYPE(UINT8  , "uint8"  ),
    TYPE(SINT16 , "sint16" ),
    TYPE(UINT16 , "uint16" ),
    TYPE(SINT32 , "sint32" ),
    TYPE(UINT32 , "uint32" ),
    TYPE(SINT64 , "sint64" ),
    TYPE(UINT64 , "uint64" ),
    TYPE(DOUBLE , "double" ),
    TYPE(BLOB   , "blob"   ),
    NULL,
};
#undef TYPE


uint16_t get_type(const char **types, const char *name)
{
    const char **t;
    
    for (t = types; *t != NULL; t++) {
	if (!strcmp(*t, name))
	    return (uint16_t)(t - types);
    }
    
    return MRP_MSG_FIELD_INVALID;
}


int main(int argc, char *argv[])
{
    mrp_msg_t *msg, *decoded;
    void      *encoded;
    ssize_t    size;
    uint16_t   tag, type, prev_tag;
    uint8_t    u8;
    int8_t     s8;
    uint16_t   u16;
    int16_t    s16;
    uint32_t   u32;
    int32_t    s32;
    uint64_t   u64;
    int64_t    s64;
    double     dbl;
    bool       bln;
    char      *val, *end;
    int        i, ok;

    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_DEBUG));
    mrp_log_set_target(MRP_LOG_TO_STDOUT);
        
    if ((msg = mrp_msg_create(MRP_MSG_FIELD_INVALID)) == NULL) {
	mrp_log_error("Failed to create new message.");
	exit(1);
    }
    
    prev_tag = 0;
    i        = 1;
    while (i < argc) {
	
	if ('0' <= *argv[i] && *argv[i] <= '9') {
	    if (argc <= i + 2) {
		mrp_log_error("Missing field type or value.");
		exit(1);
	    }

	    tag = prev_tag = (uint16_t)strtoul(argv[i++], &end, 0);
	    if (end && *end) {
		mrp_log_error("Invalid field tag '%s'.", argv[i]);
		exit(1);
	    }
	}
	else {
	    if (argc <= i + 1) {
		mrp_log_error("Missing field type or value.");
		exit(1);
	    }

	    tag = ++prev_tag;
	}

	type = get_type(types, argv[i++]);
	val  = argv[i++];
	
	if (type == MRP_MSG_FIELD_INVALID) {
	    mrp_log_error("Invalid field type '%s'.", argv[i + 1]);
	    exit(1);
	}

	switch (type) {
	case MRP_MSG_FIELD_STRING:
	    ok = mrp_msg_append(msg, tag, type, val);
	    break;

	case MRP_MSG_FIELD_BOOL:
	    if (!strcasecmp(val, "true"))
		bln = TRUE;
	    else if (!strcasecmp(val, "false"))
		bln = FALSE;
	    else {
		mrp_log_error("Invalid boolean value '%s'.", val);
		exit(1);
	    }
	    ok = mrp_msg_append(msg, tag, type, bln);
	    break;

#define HANDLE_INT(_bits, _uget, _sget)					\
	case MRP_MSG_FIELD_UINT##_bits:				        \
	    u##_bits = (uint##_bits##_t)strtoul(val, &end, 0);		\
	    if (end && *end) {						\
		mrp_log_error("Invalid uint%d value '%s'.", _bits, val); \
		exit(1);						\
	    }								\
	    ok = mrp_msg_append(msg, tag, type, u##_bits);		\
	    break;							\
	case MRP_MSG_FIELD_SINT##_bits:					\
	    s##_bits = (int##_bits##_t)strtol(val, &end, 0);		\
	    if (end && *end) {						\
		mrp_log_error("Invalid sint%d value '%s'.", _bits, val); \
		exit(1);						\
	    }								\
	    ok = mrp_msg_append(msg, tag, type, s##_bits);		\
	    break
	    
	    HANDLE_INT(8 , strtol , strtoul);
	    HANDLE_INT(16, strtol , strtoul);
	    HANDLE_INT(32, strtol , strtoul);
	    HANDLE_INT(64, strtoll, strtoull);
	    
	case MRP_MSG_FIELD_DOUBLE:
	    dbl = strtod(val, &end);
	    if (end && *end) {
		mrp_log_error("Invalid double value '%s'.", val);
		exit(1);
	    }
	    ok = mrp_msg_append(msg, tag, type, dbl);
	    break;
	    
	default:
	    mrp_log_error("Invalid (or unimplemented) type 0x%x (%s).",
			  type, argv[i + 1]);
	    ok = FALSE;
	}

	if (!ok) {
	    mrp_log_error("Failed to add field to message.");
	    exit(1);
	}
    }

    mrp_msg_dump(msg, stdout);

    size = mrp_msg_default_encode(msg, &encoded);
    if (size <= 0) {
	mrp_log_error("Failed to encode message with default encoder.");
	exit(1);
    }

    mrp_log_info("encoded message size: %d", (int)size);

    decoded = mrp_msg_default_decode(encoded, size);
    if (decoded == NULL) {
	mrp_log_error("Failed to decode message with default decoder.");
	exit(1);
    }

    mrp_msg_dump(decoded, stdout);

    mrp_msg_unref(msg);
    mrp_msg_unref(decoded);
    
    return 0;
}
