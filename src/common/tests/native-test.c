#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/native-types.h>


typedef enum {
    MUSIC,
    MOVIE,
    BOOK,
    PAINTING,
} art_type_t;


typedef struct {
    art_type_t  type;
    char       *artist;
    char       *title;
    uint16_t    year;
    char       *location;
    double      price;
} art_t;


typedef enum {
    LEFT = 0,
    RIGHT,
    BOTH
} hand_t;


typedef enum {
    MALE = 0,
    FEMALE = 1,
} gender_t;


typedef struct {
    const char      *item;
    mrp_list_hook_t  hook;
} item_t;



typedef struct {
    char            *name;
    gender_t         gender;
    int              age;
    char           **languages;
    unsigned int     height;
    float            weight;
    char             nationality[32];
    hand_t           hand;
    bool             glasses;
    art_t           *favourites;
    size_t           nfavourite;
    mrp_list_hook_t  items;
} person_t;


typedef struct {
    person_t *father;
    person_t *mother;
    person_t *children;
} family_t;


art_t paps_favourites[] = {
    {
        BOOK ,
        "Douglas Adams", "Dirk Gently's Holistic Detective Agency",
        1987, "bookshelf", 9.5
    },
    {
        MUSIC,
        "Megadeth", "Sweating Bullets",
        1992, "pocket", 12.5
    },
    {
        MUSIC,
        "Sentenced", "Noose",
        1996, "phone", 12
    },
    {
        MOVIE,
        "Bananas", "Woody Allen",
        1971, "PVR", 20.5
    }
};


char *paps_languages[] = {
    "english", "swedish", "finnish", NULL
};


item_t pap_item_0 = {
    .item = "Pap's list item #1",
    .hook = MRP_LIST_INIT(pap_item_0.hook),
};

item_t pap_item_1 = {
    .item = "Pap's list item #2",
    .hook = MRP_LIST_INIT(pap_item_1.hook),
};

item_t pap_item_2 = {
    .item = "Pap's list item #3",
    .hook = MRP_LIST_INIT(pap_item_2.hook),
};


person_t pap = {
    .name        = "Pap",
    .gender      = MALE,
    .age         = 30,
    .languages   = paps_languages,
    .height      = 180,
    .weight      = 84.5,
    .nationality = "martian",
    .hand        = RIGHT,
    .glasses     = false,
    .favourites  = paps_favourites,
    .nfavourite  = MRP_ARRAY_SIZE(paps_favourites),
    .items       = MRP_LIST_INIT(pap.items),
};


art_t moms_favourites[] = {
    {
        BOOK ,
        "Douglas Adams", "THHGTTG",
        1982, "bookshelf", 11.8
    },
    {
        MUSIC,
        "Megadeth", "Sweating Bullets",
        1992, "pocket", 12.5
    },
    {
        MOVIE,
        "Hottie Chick", "GGW-II",
        1996, "PVR", 0.5
    },
    {
        BOOK ,
        "Douglas Adams", "The Long Dark Tea-Time of the Soul",
        1988, "Kindle Touch", 8.50
    }
};


char *moms_languages[] = {
    "finnish", "english", "swedish", "french", NULL
};

person_t mom = {
    .name        = "Mom",
    .gender      = FEMALE,
    .age         = 28,
    .languages   = moms_languages,
    .height      = 165,
    .weight      = 57.8,
    .nationality = "venusian",
    .hand        = LEFT,
    .glasses     = true,
    .favourites  = moms_favourites,
    .nfavourite  = MRP_ARRAY_SIZE(moms_favourites),
    .items       = MRP_LIST_INIT(mom.items),
};


char *kids_languages[] = {
    "english", "finnish", "swedish", NULL
};

person_t tom_dick_and_harry[] = {
    {
        .name        = "Tom",
        .gender      = MALE,
        .age         = 10,
        .languages   = kids_languages + 1,
        .height      = 135,
        .weight      = 40.5,
        .nationality = "UFO",
        .hand        = BOTH,
        .glasses     = false,
        .favourites  = NULL,
        .nfavourite  = 0,
        .items       = MRP_LIST_INIT(tom_dick_and_harry[0].items),
    },
    {
        .name        = "Dick",
        .gender      = MALE,
        .age         = 12,
        .languages   = kids_languages,
        .height      = 145,
        .weight      = 45.5,
        .nationality = "UFO",
        .hand        = RIGHT,
        .glasses     = true,
        .favourites  = paps_favourites + 1,
        .nfavourite  = MRP_ARRAY_SIZE(paps_favourites) - 2,
        .items       = MRP_LIST_INIT(tom_dick_and_harry[1].items)
    },
    {
        .name        = "Harry",
        .gender      = MALE,
        .age         = 14,
        .languages   = kids_languages + 2,
        .height      = 165,
        .weight      = 60.5,
        .nationality = "UFO",
        .hand        = LEFT,
        .glasses     = false,
        .favourites  = moms_favourites + 1,
        .nfavourite  = MRP_ARRAY_SIZE(moms_favourites) - 2,
        .items       = MRP_LIST_INIT(tom_dick_and_harry[2].items)
    },
    {
        .name        = NULL,
    },
};


family_t family = { &pap, &mom, &tom_dick_and_harry[0] };


int main(int argc, char *argv[])
{
    MRP_NATIVE_TYPE(art_type, art_t,
                    MRP_UINT32(art_t, type    , DEFAULT),
                    MRP_STRING(art_t, artist  , DEFAULT),
                    MRP_STRING(art_t, title   , DEFAULT),
                    MRP_UINT16(art_t, year    , DEFAULT),
                    MRP_STRING(art_t, location, DEFAULT),
                    MRP_DOUBLE(art_t, price   , DEFAULT));
    MRP_NATIVE_TYPE(item_type, item_t,
                    MRP_STRING(item_t, item   , DEFAULT),
                    MRP_HOOK  (item_t, hook            ));
    MRP_NATIVE_TYPE(person_type, person_t,
                    MRP_STRING(person_t, name       , DEFAULT),
                    MRP_UINT32(person_t, gender     , DEFAULT),
                    MRP_INT   (person_t, age        , DEFAULT),
                    MRP_ARRAY (person_t, languages  , DEFAULT, GUARDED,
                               char *, "", .strp = NULL),
                    MRP_UINT  (person_t, height     , DEFAULT),
                    MRP_FLOAT (person_t, weight     , DEFAULT),
                    MRP_STRING(person_t, nationality, INLINED),
                    MRP_UINT32(person_t, hand       , DEFAULT),
                    MRP_BOOL  (person_t, glasses    , DEFAULT),
                    MRP_ARRAY (person_t, favourites , DEFAULT, SIZED,
                               art_t, nfavourite),
                    MRP_SIZET (person_t, nfavourite , DEFAULT),
                    MRP_LIST  (person_t, items      , item_t, hook));

    MRP_NATIVE_TYPE(family_type, family_t,
                    MRP_STRUCT(family_t, father  , DEFAULT, person_t),
                    MRP_STRUCT(family_t, mother  , DEFAULT, person_t),
                    MRP_ARRAY (family_t, children, DEFAULT, GUARDED,
                               person_t, name, .strp = NULL));
    mrp_typemap_t map[5];

    uint32_t  art_type_id, item_type_id, person_type_id, family_type_id;
    void     *ebuf;
    size_t    esize;
    int       fd;
    void     *dbuf;
    family_t *decoded;
    char      dump[16 * 1024];

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_INFO));

    mrp_list_append(&pap.items, &pap_item_0.hook);
    mrp_list_append(&pap.items, &pap_item_1.hook);
    mrp_list_append(&pap.items, &pap_item_2.hook);

    art_type_id = mrp_register_native(&art_type);

    if (art_type_id == MRP_INVALID_TYPE)
        mrp_log_error("Failed to register art_t type.");
    else
        mrp_log_info("Type art_t sucessfully registered.");

    item_type_id = mrp_register_native(&item_type);

    if (item_type_id == MRP_INVALID_TYPE)
        mrp_log_error("Failed to register item_t type.");
    else
        mrp_log_info("Type item_t sucessfully registered.");

    person_type_id = mrp_register_native(&person_type);

    if (person_type_id == MRP_INVALID_TYPE)
        mrp_log_error("Failed to register person_t type.");
    else
        mrp_log_info("Type person_t sucessfully registered.");

    family_type_id = mrp_register_native(&family_type);

    if (family_type_id == MRP_INVALID_TYPE)
        mrp_log_error("Failed to register family_t type.");
    else
        mrp_log_info("Type family_t sucessfully registered.");

    ebuf = NULL;

    map[0] = (mrp_typemap_t)MRP_TYPEMAP(1, art_type_id   );
    map[1] = (mrp_typemap_t)MRP_TYPEMAP(2, item_type_id  );
    map[2] = (mrp_typemap_t)MRP_TYPEMAP(3, person_type_id);
    map[3] = (mrp_typemap_t)MRP_TYPEMAP(4, family_type_id);
    map[4] = (mrp_typemap_t)MRP_TYPEMAP_END;

    if (mrp_encode_native(&family, family_type_id, 0, &ebuf, &esize, map) < 0) {
        mrp_log_error("Failed to encode test data.");
        exit(1);
    }
    else
        mrp_log_info("Test data successfully encoded (%zd bytes).", esize);

    if ((fd = open("type-test.encoded",
                   O_CREAT | O_TRUNC | O_WRONLY, 0644)) >= 0) {
        if (write(fd, ebuf, esize) != (ssize_t)esize)
            mrp_log_error("Failed to write encoded data.");
        close(fd);
    }

    if (mrp_decode_native(&ebuf, &esize, &dbuf, &family_type_id, map) < 0) {
        mrp_log_error("Failed to decode test data.");
        exit(1);
    }
    else
        mrp_log_info("Test data sucessfully decoded.");

    decoded = dbuf;

    if (mrp_print_native(dump, sizeof(dump), decoded, family_type_id) >= 0)
        mrp_log_info("dump of decoded data: %s", dump);
    else
        mrp_log_error("Failed to dump decoded data.");

    mrp_free_native(dbuf, family_type_id);

    return 0;
}
