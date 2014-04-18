#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/types.h>

typedef struct artist_s     artist_t;
typedef struct member_s     member_t;
typedef struct album_s      album_t;
typedef struct track_info_s track_info_t;
typedef struct track_s      track_t;

struct artist_s {
    char        name[64];
    const char  genre[32];
    uint16_t    established;
    const char *country;
    bool        disbanded;
    member_t   *members;
    size_t      nmember;
    album_t    *albums;
};

struct member_s {
    const char  *name;
    uint16_t     birth;
    bool         female;
    const char  *nationality;
    char       **instruments;
#if 0
    char        *flexi[];
#endif
};

struct track_info_s {
    uint16_t   length;
    char     **authors;
    bool       instrumental;
};

struct track_s {
    const char   *title;
    track_info_t  info;
};

struct album_s {
    const char *title;
    uint16_t    year;
    const char *label;
    uint8_t     format;
    track_t    *tracks;
    int         ntrack;
#if 0
    track_t     flexible[0];
#endif
};

typedef enum {
    FORMAT_UNKNOWN = 0x00,
    FORMAT_EP      = 0x01,
    FORMAT_LP      = 0x02,
    FORMAT_CASETTE = 0x04,
    FORMAT_CD      = 0x08,
    FORMAT_MP3     = 0x10,
    FORMAT_AAC     = 0x20,
    FORMAT_FLAC    = 0x40,
} format_t;

#define LENGTH(min, sec) ((min) * 60 + (sec))

typedef union  vehicle_u vehicle_t;
typedef struct car_s     car_t;
typedef struct bus_s     bus_t;
typedef struct train_s   train_t;
typedef struct plane_s   plane_t;
typedef struct deco_s    deco_t;
typedef struct bike_s    bike_t;
typedef struct trike_s   trike_t;
typedef struct light_s   light_t;

typedef enum {
    VEHICLE_UNKNOWN,
    VEHICLE_CAR,
    VEHICLE_BUS,
    VEHICLE_TRAIN,
    VEHICLE_PLANE,
    VEHICLE_BIKE,
} vehicle_type_t;

typedef enum {
    FUEL_UNKNOWN = 0,
    FUEL_GASOLINE,
    FUEL_DIESEL,
    FUEL_ELECTRIC,
    FUEL_HUMAN,
    FUEL_HAKAPONTTO
} fuel_t;

struct car_s {
    vehicle_type_t  type;
    const char     *vendor;
    const char     *model;
    int16_t         year;
    fuel_t          fuel;
    int             power;
    int16_t         speed;
    uint8_t         doors;
    uint8_t         seats;
};

struct bus_s {
    vehicle_type_t  type;
    const char     *vendor;
    const char     *model;
    fuel_t          fuel;
    int             range;
    int16_t         seats;
};

struct train_s {
    vehicle_type_t  type;
    const char     *vendor;
    const char     *model;
    fuel_t          fuel;
    int8_t          vagons;
    int16_t         seats;
    int8_t          doors;
    int8_t          cabins;
    int16_t         beds;
};

struct plane_s {
    vehicle_type_t  type;
    const char     *vendor;
    const char     *model;
    uint16_t        wingspan;
    uint8_t         engines;
    int32_t         range;
    int16_t         seats;
    int8_t          crew;
    int32_t         cargo;
};

struct bike_s {
    vehicle_type_t  type;
    const char     *model;
    double          weight;
    uint8_t         gears;
};

union vehicle_u {
    vehicle_type_t type;
    car_t          car;
    bus_t          bus;
    train_t        train;
    plane_t        plane;
};

typedef enum {
    LIGHT_UNKNOWN,
    LIGHT_BIKE,
    LIGHT_TRIKE,
} light_type_t;

struct deco_s {
    uint8_t     metal_color;
    int         nsticker;
    const char *stickers[0];
};

struct trike_s {
    const char *vendor;
    const char *owner;
    uint32_t    color;
    deco_t      deco;
};

struct light_s {
    light_type_t type;
    int          price;
    union {
        bike_t   bike;
        trike_t  trike;
    };
};


static void print_member(const char *prefix, member_t *m)
{
    int i;
    const char *sep;

    printf("%s %s %s (%d, %s):\n", prefix,
           m->female ? "Mrs." : "Mr.", m->name, m->birth, m->nationality);
    printf("%s  instruments: ", prefix);
    for (i = 0, sep = ""; m->instruments[i] != NULL; i++, sep = ", ")
        printf("%s%s", sep, m->instruments[i]);
    printf("\n");
}


static const char *format_string(uint8_t format)
{
    switch (format) {
    case FORMAT_EP:      return "EP";
    case FORMAT_LP:      return "LP";
    case FORMAT_CASETTE: return "C-casette";
    case FORMAT_CD:      return "CD";
    case FORMAT_MP3:     return "MP3";
    case FORMAT_AAC:     return "AAC";
    case FORMAT_FLAC:    return "FLAC";
    default:             return "<unknown format>";
    }
}


static void print_track(const char *prefix, track_t *t)
{
    int   i;
    char *a, *sep;

    printf("%s%s (%d:%2.2d%s, ", prefix, t->title,
           t->info.length / 60, t->info.length % 60,
           t->info.instrumental ? ", instrumental" : "");
    for (i = 0, sep = ""; (a = t->info.authors[i]) != NULL; i++, sep = ", ")
        printf("%s%s", sep, a);
    printf(")\n");
}

static void print_album(const char *prefix, album_t *a)
{
    int i;

    printf("%s%s (%s, label %s, %d, %d tracks):\n", prefix,
           a->title, format_string(a->format), a->label, a->year, a->ntrack);

    for (i = 0; i < a->ntrack; i++)
        print_track("      ", a->tracks + i);
}

static void print_artist(artist_t *a)
{
    uint32_t i;

    printf("artist %s (%d, %s, %s%s):\n", a->name, a->established, a->country,
           a->genre, a->disbanded ? ", disbanded" : "");
    printf("  members:\n");
#if 0
    for (i = 0; i < a->nmember; i++)
        print_member("    ", a->members + i);
#else
    for (i = 0; a->members[i].name != NULL; i++)
        print_member("    ", a->members + i);
#endif
    printf("  albums:\n");
    for (i = 0; a->albums[i].title != NULL; i++)
        print_album("    ", a->albums + i);
}


static const char *fuel_type(fuel_t f)
{
    switch (f) {
    case FUEL_GASOLINE:   return "gasoline";
    case FUEL_DIESEL:     return "diesel";
    case FUEL_ELECTRIC:   return "electric";
    case FUEL_HUMAN:      return "human";
    case FUEL_HAKAPONTTO: return "hakapontto";
    default:              return "<unknown fuel type>";
    }
}


static void print_car(car_t *c)
{
    printf("car (%s %s, year %d):\n", c->vendor, c->model, c->year);
    printf("  fuel: %s\n", fuel_type(c->fuel));
    printf("  power: %d hp\n", c->power);
    printf("  speed: %d km/h\n", c->speed);
    printf("  doors: %d\n", c->doors);
    printf("  seats: %d\n", c->seats);
}


static void print_bus(bus_t *b)
{
    printf("bus (%s %s):\n", b->vendor, b->model);
    printf("  fuel: %s\n", fuel_type(b->fuel));
    printf("  range: %d\n", b->range);
    printf("  seats: %u\n", b->seats);
}


static void print_train(train_t *t)
{
    printf("train (%s %s):\n", t->vendor, t->model);
    printf("  fuel: %s\n", fuel_type(t->fuel));
    printf("  vagons: %d\n", t->vagons);
    printf("  seats: %u\n", t->seats);
    printf("  doors: %d\n", t->doors);
    printf("  cabins: %d\n", t->cabins);
    printf("  beds: %d\n", t->beds);
}


static void print_plane(plane_t *p)
{
    printf("plane (%s %s):\n", p->vendor, p->model);
    printf("  wingspan: %u\n", p->wingspan);
    printf("  engines: %u\n", p->engines);
    printf("  range: %u\n", p->range);
    printf("  seats: %u\n", p->seats);
    printf("  crew: %d\n", p->crew);
    printf("  cargo: %d\n", p->cargo);
}


static void print_vehicle(vehicle_t *v)
{
    switch (v->type) {
    case VEHICLE_CAR:   print_car(&v->car);     break;
    case VEHICLE_BUS:   print_bus(&v->bus);     break;
    case VEHICLE_TRAIN: print_train(&v->train); break;
    case VEHICLE_PLANE: print_plane(&v->plane); break;
#if 0
    case VEHICLE_BIKE:  print_bike(&v->bike);   break;
#endif
    default:
        printf("<vehicle of unknown type 0x%x>\n", v->type);
    }
}


static void print_bike(bike_t *b)
{
    printf("bike (%s):\n", b->model);
    printf("  weight: %f\n", b->weight);
    printf("  gears: %d\n", b->gears);
}


static void print_trike(trike_t *t)
{
    int i;

    printf("trike (%s, %s):\n", t->owner, t->vendor);
    printf("  color: 0x%8.8x (%s)\n", t->color,
           t->deco.metal_color ? "metal color" : "matt color");
    for (i = 0; i < t->deco.nsticker; i++)
        printf("  #%d sticker: %s\n", i, t->deco.stickers[i]);
}


static void print_light(light_t *l)
{
    printf("light price: %d\n", l->price);
    switch (l->type) {
    case LIGHT_BIKE:  print_bike(&l->bike);   break;
    case LIGHT_TRIKE: print_trike(&l->trike); break;
    default:          printf("<unknown light_t 0x%x>\n", l->type);
    }
}

int main(int argc, char *argv[])
{
#define G  "David Gilmour"
#define W  "Roger Waters"
#define M  "Nick Mason"
#define RW "Richard Wright"
#define B  "Syd Barrett"
#define C  "Clare Torry"

    static char *gwwm[] = { G, W, RW, M,    NULL };
    static char *gww[]  = { G, W, RW,       NULL };
    static char *gw[]   = { G, W,           NULL };
    static char *gm[]   = { G,        M,    NULL };
    static char *ww[]   = {    W, RW,       NULL };
    static char *wc[]   = {       RW,    C, NULL };
    static char *w[]    = {    W,           NULL };
    static char *g[]    = { G,              NULL };
    static track_t pf_dsotm_tracks[] = {
        { "Speak To Me", { LENGTH(1, 30),  gww, true  } },
        { "Breathe"    , { LENGTH(2, 43),  gww, false } },
        { "On The Run" , { LENGTH(3, 36),   gw, true  } },
        { "Time"       , { LENGTH(7,  1), gwwm, false } },
        { "The Great Gig "
          "In The Sky" , { LENGTH(4, 36),   wc, false } },
        { "Money"      , { LENGTH(6, 22),   gw, false } },
        { "Us And Them", { LENGTH(7, 46),   ww, false } },
        { "Any Colour "
          "You Like"   , { LENGTH(3, 25),   gm, true  } },
        {"Brain Damage", { LENGTH(3, 48),    w, false } },
        {"Eclipse"     , { LENGTH(2,  3),    w, false } }
    };
    static track_t pf_soycd_tracks[] = {
        { "Shine On You Crazy Diamond, I-V"  , { LENGTH(13, 38), gww, false } },
        { "Welcome To The Machine"           , { LENGTH(7 , 30), gw , false } },
        { "Have A Cigar"                     , { LENGTH(5 , 24), w  , false } },
        { "Wish You Were Here"               , { LENGTH(5 , 17), g  , false } },
        { "Shine On You Crazy Diamond, VI-IX", { LENGTH(12, 29), gww, false } }
    };


    static album_t pf_albums[] = {
        {
            .title  = "Dark Side Of The Moon",
            .year   = 1973,
            .label  = "Harvest, Capitol",
            .format = FORMAT_LP,
            .tracks = pf_dsotm_tracks,
            .ntrack = MRP_ARRAY_SIZE(pf_dsotm_tracks),
        },
        {
            .title  = "Wish You Were Here",
            .year   = 1975,
            .label  = "Harvest, Columbia/CBS",
            .format = FORMAT_LP,
            .tracks = pf_soycd_tracks,
            .ntrack = MRP_ARRAY_SIZE(pf_soycd_tracks),
        },
        { NULL, 0, NULL, 0, NULL, 0 }
    };

#define IV "vocals"
#define IG "guitars"
#define IB "bass guitar"
#define IP "percussion"
#define IK "keyboards"
#define I3 "VCS3"
#define IT "tape effects"

    static char *vg3[]  = { IV, IG, I3    , NULL };
    static char *pt[]   = { IP, IT        , NULL };
    static char *kv3[]  = { IK, IV, I3    , NULL };
    static char *bv3t[] = { IB, IV, I3, IT, NULL };
    static member_t pf_members[] = {
        { "David Gilmour" , 0, false, "UK", vg3  },
        { "Nick Mason"    , 0, false, "UK", pt   },
        { "Richard Wright", 0, false, "UK", kv3  },
        { "Roger Waters"  , 0, false, "UK", bv3t },
#if 1
        { NULL            , 0, false, ""  , NULL },
#endif
    };

    static artist_t pink_floyd = {
        "Pink Floyd", "Progressive Rock", 1965, "UK", true,
        pf_members, MRP_ARRAY_SIZE(pf_members), pf_albums
    };

    static vehicle_t cessna = {
        .plane = {
            .type     = VEHICLE_PLANE,
            .vendor   = "Cessna",
            .model    = "172 Skyhawk",
            .wingspan = 650,
            .engines  = 1,
            .range    = 500,
            .seats    = 4,
            .crew     = 0,
            .cargo    = 50
        }
    };

    static light_t light_bike = {
        .type  = LIGHT_BIKE,
        .price = 123,
        .bike = {
            .type   = VEHICLE_BIKE,
            .model  = "Light Bike",
            .weight = 11.5,
            .gears  = 3,
        },
    };

    static struct {
        light_t     light;
        const char *stickers[5];
    } light_trike = {
        .light = {
            .type = LIGHT_TRIKE,
            .price = 15,
            .trike = {
                .vendor = "Nalle Puh",
                .owner  = "Marci",
                .color  = 0xff0000,
                .deco = {
                    .metal_color = 1,
                    .nsticker = 5,
                },
            },
        },
        .stickers = { "Nalle Puh", "Tiikeri", "Ihaa", "Nasu", "Kani" },
    };

    mrp_type_id_t artist_id;
    MRP_DEFINE_TYPE(artist_type, artist_t,
                    MRP_SCALAR(artist_t, char *  , name               ),
                    MRP_SCALAR(artist_t, char *  , genre              ),
                    MRP_SCALAR(artist_t, uint16_t, established        ),
                    MRP_SCALAR(artist_t, char *  , country            ),
                    MRP_SCALAR(artist_t, bool    , disbanded          ),
#if 0
                    MRP_ARRAY (artist_t, member_t, members,
                               SIZED, nmember                         ),
#else
                    MRP_ARRAY (artist_t, member_t, members,
                               GUARD, name, { .strp = NULL }          ),

#endif
                    MRP_SCALAR(artist_t, size_t  , nmember            ),
                    MRP_ARRAY (artist_t, album_t , albums,
                               GUARD, title, { .strp = NULL }         ));

    mrp_type_id_t member_id;
    MRP_DEFINE_TYPE(member_type, member_t,
                    MRP_SCALAR(member_t, char *  , name           ),
                    MRP_SCALAR(member_t, uint16_t, birth          ),
                    MRP_SCALAR(member_t, bool    , female         ),
                    MRP_SCALAR(member_t, char *  , nationality    ),
                    MRP_ARRAY (member_t, char *  , instruments,
                               GUARD, -, { .strp = NULL }         ),
#if 0
                    MRP_FLEXI (member_t, char *  , flexi,
                               -, { .strp = NULL })
#endif
                    );

    mrp_type_id_t album_id;
    MRP_DEFINE_TYPE(album_type, album_t,
                    MRP_SCALAR(album_t, char *  , title  ),
                    MRP_SCALAR(album_t, uint16_t, year   ),
                    MRP_SCALAR(album_t, char *  , label  ),
                    MRP_SCALAR(album_t, uint8_t , format ),
                    MRP_ARRAY (album_t, track_t , tracks,
                               SIZED, ntrack             ),
                    MRP_SCALAR(album_t, int     , ntrack ),
#if 0
                    MRP_ARRAY (album_t, track_t , flexible,
                               GUARD, title, { .strp = NULL })
#endif
                    );

    mrp_type_id_t track_id;
    MRP_DEFINE_TYPE(track_type, track_t,
                    MRP_SCALAR(track_t, char *      , title),
                    MRP_SCALAR(track_t, track_info_t, info));

    mrp_type_id_t track_info_id;
    MRP_DEFINE_TYPE(track_info_type, track_info_t,
                    MRP_SCALAR(track_info_t, uint16_t, length      ),
                    MRP_ARRAY (track_info_t, char *  , authors,
                               GUARD, -, { .strp = NULL }          ),
                    MRP_SCALAR(track_info_t, bool    , instrumental));

    mrp_type_id_t vehicle_id;
    MRP_DEFINE_TYPE(vehicle_type, vehicle_t,
                    MRP_UNION (vehicle_t, vehicle_type_t, type , KEY  ),
                    MRP_UNION (vehicle_t, car_t         , car  , MEMBER,
                               type, { .si = VEHICLE_CAR   }          ),
                    MRP_UNION (vehicle_t, bus_t         , bus  , MEMBER,
                               type, { .si = VEHICLE_BUS   }          ),
                    MRP_UNION (vehicle_t, train_t       , train, MEMBER,
                               type, { .si = VEHICLE_TRAIN }          ),
                    MRP_UNION (vehicle_t, plane_t       , plane, MEMBER,
                               type, { .si = VEHICLE_PLANE }          ));

    mrp_type_id_t car_id;
    MRP_DEFINE_TYPE(car_type, car_t,
                    MRP_SCALAR(car_t, vehicle_type_t, type  ),
                    MRP_SCALAR(car_t, char *        , vendor),
                    MRP_SCALAR(car_t, char *        , model ),
                    MRP_SCALAR(car_t, int16_t       , year  ),
                    MRP_SCALAR(car_t, fuel_t        , fuel  ),
                    MRP_SCALAR(car_t, int           , power ),
                    MRP_SCALAR(car_t, int16_t       , speed ),
                    MRP_SCALAR(car_t, uint8_t       , doors ),
                    MRP_SCALAR(car_t, uint8_t       , seats ));

    mrp_type_id_t bus_id;
    MRP_DEFINE_TYPE(bus_type, bus_t,
                    MRP_SCALAR(bus_t, vehicle_type_t, type  ),
                    MRP_SCALAR(bus_t, char *  , vendor),
                    MRP_SCALAR(bus_t, char *  , model ),
                    MRP_SCALAR(bus_t, fuel_t  , fuel  ),
                    MRP_SCALAR(bus_t, int     , range ),
                    MRP_SCALAR(bus_t, int16_t , seats ));

    mrp_type_id_t train_id;
    MRP_DEFINE_TYPE(train_type, train_t,
                    MRP_SCALAR(train_t, vehicle_type_t, type  ),
                    MRP_SCALAR(train_t, char *  , vendor),
                    MRP_SCALAR(train_t, char *  , model ),
                    MRP_SCALAR(train_t, int8_t  , vagons),
                    MRP_SCALAR(train_t, int16_t , seats ),
                    MRP_SCALAR(train_t, int8_t  , doors ),
                    MRP_SCALAR(train_t, int8_t  , cabins),
                    MRP_SCALAR(train_t, int16_t , beds  ));

    mrp_type_id_t plane_id;
    MRP_DEFINE_TYPE(plane_type, plane_t,
                    MRP_SCALAR(plane_t, vehicle_type_t, type    ),
                    MRP_SCALAR(plane_t, char *        , vendor  ),
                    MRP_SCALAR(plane_t, char *        , model   ),
                    MRP_SCALAR(plane_t, uint16_t      , wingspan),
                    MRP_SCALAR(plane_t, uint8_t       , engines ),
                    MRP_SCALAR(plane_t, int32_t       , range   ),
                    MRP_SCALAR(plane_t, int16_t       , seats   ),
                    MRP_SCALAR(plane_t, int8_t        , crew    ),
                    MRP_SCALAR(plane_t, int32_t       , cargo   ));

    mrp_type_id_t bike_id;
    MRP_DEFINE_TYPE(bike_type, bike_t,
                    MRP_SCALAR(bike_t, vehicle_type_t, type  ),
                    MRP_SCALAR(bike_t, char *        , model ),
                    MRP_SCALAR(bike_t, double        , weight),
                    MRP_SCALAR(bike_t, uint8_t       , gears ));

    mrp_type_id_t light_id;
    MRP_DEFINE_TYPE(light_type, light_t,
                    MRP_SCALAR(light_t, light_type_t, type        ),
                    MRP_UNION (light_t, bike_t      , bike ,
                               MEMBER, type, { .si = LIGHT_BIKE  }),
                    MRP_UNION (light_t, trike_t     , trike,
                               MEMBER, type, { .si = LIGHT_TRIKE }),
                    MRP_SCALAR(light_t, int         , price       ));

    mrp_type_id_t trike_id;
    MRP_DEFINE_TYPE(trike_type, trike_t,
                    MRP_SCALAR(trike_t, char *  , vendor),
                    MRP_SCALAR(trike_t, char *  , owner ),
                    MRP_SCALAR(trike_t, uint32_t, color ),
                    MRP_SCALAR(trike_t, deco_t  , deco  ));

    mrp_type_id_t deco_id;
    MRP_DEFINE_TYPE(deco_type, deco_t,
                    MRP_SCALAR(deco_t, uint8_t, metal_color),
                    MRP_SCALAR(deco_t, int    , nsticker   ),
                    MRP_ARRAY (deco_t, char  *, stickers,
                               SIZED,  nsticker            ));

    mrp_type_map_t idmap[64], *map;
    int            nid;

    void   *enc, *dec;
    size_t  esize;
    char    dump[16 * 1024];
    int     i;

    MRP_UNUSED(argc);
    MRP_UNUSED(argv);

    mrp_log_enable(true);
    mrp_log_set_mask(mrp_log_parse_levels("info,error,warning"));
    mrp_debug_enable(true);

    mrp_debug_set_config("@types.c");
    for (i = 1; i < argc; i++)
        mrp_debug_set_config(argv[i]);

#define MAP(id) do {                            \
        idmap[nid].native = id;                 \
        idmap[nid].mapped = 30 + nid;           \
        nid++;                                  \
    } while (0)

    nid = 0;

    artist_id     = mrp_declare_type("artist_t");     MAP(artist_id);
    member_id     = mrp_declare_type("member_t");     MAP(member_id);
    album_id      = mrp_declare_type("album_t");      MAP(album_id);
    track_info_id = mrp_declare_type("track_info_t"); MAP(track_info_id);
    track_id      = mrp_declare_type("track_t");      MAP(track_id);

    if (mrp_register_type(&artist_type) < 0) {
        mrp_log_error("Failed to register type artist_t.");
        exit(1);
    }

    if (mrp_register_type(&member_type) < 0) {
        mrp_log_error("Failed to register type member_t.");
        exit(1);
    }

    if (mrp_register_type(&album_type) < 0) {
        mrp_log_error("Failed to register type album_t.");
        exit(1);
    }

    if (mrp_register_type(&track_type) < 0) {
        mrp_log_error("Failed to register type track_t.");
        exit(1);
    }

    if (mrp_register_type(&track_info_type) < 0) {
        mrp_log_error("Failed to register type track_info_t.");
        exit(1);
    }

    mrp_declare_enum("vehicle_type_t");
    mrp_declare_enum("light_type_t");
    mrp_declare_enum("fuel_t");

    vehicle_id = mrp_declare_type("vehicle_t"); MAP(vehicle_id);
    car_id     = mrp_declare_type("car_t");     MAP(car_id);
    bus_id     = mrp_declare_type("bus_t");     MAP(bus_id);
    train_id   = mrp_declare_type("train_t");   MAP(train_id);
    plane_id   = mrp_declare_type("plane_t");   MAP(plane_id);
    deco_id    = mrp_declare_type("deco_t");    MAP(deco_id);
    trike_id   = mrp_declare_type("trike_t");   MAP(trike_id);
    bike_id    = mrp_declare_type("bike_t");    MAP(bike_id);
    light_id   = mrp_declare_type("light_t");   MAP(light_id);

    idmap[nid].native = MRP_TYPE_INVALID;
    idmap[nid].mapped = MRP_TYPE_INVALID;
    map = idmap;

    if (mrp_register_type(&vehicle_type) < 0) {
        mrp_log_error("Failed to register type vehicle_t.");
        exit(1);
    }

    if (mrp_register_type(&car_type) < 0) {
        mrp_log_error("Failed to register type car_t.");
        exit(1);
    }

    if (mrp_register_type(&bus_type) < 0) {
        mrp_log_error("Failed to register type bus_t.");
        exit(1);
    }

    if (mrp_register_type(&train_type) < 0) {
        mrp_log_error("Failed to register type train_t.");
        exit(1);
    }

    if (mrp_register_type(&plane_type) < 0) {
        mrp_log_error("Failed to register type plane_t.");
        exit(1);
    }

    if (mrp_register_type(&bike_type) < 0) {
        mrp_log_error("Failed to register type bike_t.");
        exit(1);
    }

    if (mrp_register_type(&light_type) < 0) {
        mrp_log_error("Failed to register type light_t.");
        exit(1);
    }

    if (mrp_register_type(&trike_type) < 0) {
        mrp_log_error("Failed to register type trike_t.");
        exit(1);
    }

    if (mrp_register_type(&deco_type) < 0) {
        mrp_log_error("Failed to register type deco_t.");
        exit(1);
    }

    if (mrp_print_type_defs(dump, sizeof(dump)) < (ssize_t)sizeof(dump))
        printf("type table:\n%s", dump);

    print_artist(&pink_floyd);

    if (mrp_encode_type(artist_id, &pink_floyd, &enc, &esize, map, 0) < 0) {
        printf("failed to encode Pink Floyd\n");
        exit(1);
    }
    else {
        int fd;

        printf("Pink Floyd successfully encoded (%zu bytes)\n", esize);

        if ((fd = open("pink-floyd.encoded", O_WRONLY|O_CREAT, 0644)) < 0) {
        pf_ioerr:
            fprintf(stderr, "failed to write encoded data to file\n");
            exit(1);
        }

        if (write(fd, enc, esize) < 0)
            goto pf_ioerr;
        else
            close(fd);

        if (mrp_decode_type(&artist_id, &dec, enc, esize, map) < 0) {
            printf("failed to decode Pink Floyd\n");
            exit(1);
        }
        else {
            printf("successfully re-decoded Pink Floyd\n");
            print_artist((artist_t *)dec);
            mrp_free_type(light_id, dec);
        }

        mrp_free(enc);
    }

    if (mrp_encode_type(vehicle_id, &cessna, &enc, &esize, map, 0) < 0) {
        printf("failed to encode cessna\n");
        exit(1);
    }
    else {
        int fd;

        printf("cessna successfully encoded (%zu bytes)\n", esize);

        if ((fd = open("cessna.encoded", O_WRONLY|O_CREAT, 0644)) < 0) {
        csn_ioerr:
            fprintf(stderr, "failed to write encoded data to file\n");
            exit(1);
        }

        if (write(fd, enc, esize) < 0)
            goto csn_ioerr;
        else
            close(fd);

        if (mrp_decode_type(&vehicle_id, &dec, enc, esize, map) < 0) {
            printf("failed to decode cessna\n");
            exit(1);
        }
        else {
            printf("successfully re-decoded cessna\n");
            print_vehicle((vehicle_t *)dec);
            mrp_free_type(light_id, dec);
        }

        mrp_free(enc);
    }


    if (mrp_encode_type(light_id, &light_bike, &enc, &esize, map, 0) < 0) {
        printf("failed to encode light_bike\n");
        exit(1);
    }
    else {
        int fd;

        printf("light_bike successfully encoded (%zu bytes)\n", esize);

        if ((fd = open("light-bike.encoded", O_WRONLY|O_CREAT, 0644)) < 0) {
        lbk_ioerr:
            fprintf(stderr, "failed to write encoded data to file\n");
            exit(1);
        }

        if (write(fd, enc, esize) < 0)
            goto lbk_ioerr;
        else
            close(fd);

        if (mrp_decode_type(&light_id, &dec, enc, esize, map) < 0) {
            printf("failed to decode light_bike\n");
            exit(1);
        }
        else {
            printf("successfully re-decoded light_bike\n");
            print_light((light_t *)dec);
            mrp_free_type(light_id, dec);
        }

        mrp_free(enc);
    }

    if (mrp_encode_type(light_id, &light_trike, &enc, &esize, map, 0) < 0) {
        printf("failed to encode light_trike\n");
        exit(1);
    }
    else {
        int fd;

        printf("light_trike successfully encoded (%zu bytes)\n", esize);

        if ((fd = open("light-trike.encoded", O_WRONLY|O_CREAT, 0644)) < 0) {
        ltk_ioerr:
            fprintf(stderr, "failed to write encoded data to file\n");
            exit(1);
        }

        if (write(fd, enc, esize) < 0)
            goto ltk_ioerr;
        else
            close(fd);

        if (mrp_decode_type(&light_id, &dec, enc, esize, map) < 0) {
            printf("failed to decode light_trike\n");
            exit(1);
        }
        else {
            printf("successfully re-decoded light_trike\n");
            print_light((light_t *)dec);
            mrp_free_type(light_id, dec);
        }

        mrp_free(enc);
    }

    return 0;
}
