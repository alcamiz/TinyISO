#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#ifndef TINY_ISO_H
#define TINY_ISO_H

#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

#define BP(a,b) [(b) - (a) + 1]
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define DESC_SIZE 2048
#define SECTOR_SIZE 2048
#define RESV_SECTORS 16
#define SECTOR_TAIL 255
#define EXTENT_FLAG 0x80

/**** Internal Responses ****/

typedef enum {

    TNI_OK,
    TNI_FAIL,
    TNI_ERROR,

    TNI_ERR_ARGS,
    TNI_ERR_MEM,
    TNI_ERR_FILE,
    TNI_ERR_ISO,
    TNI_ERR_DIR,
    TNI_ERR_CB,

} tni_response_t;


/**** Raw ISO-9660 Structs ****/

typedef struct {
    uint8_t vol_desc_type            BP(1, 1);
    uint8_t std_identifier           BP(2, 6);
    uint8_t vol_desc_version         BP(7, 7);
    uint8_t vol_flags                BP(8, 8);
    uint8_t system_id                BP(9, 40);
    uint8_t volume_id                BP(41, 72);
    uint8_t unused1                  BP(73, 80);
    uint8_t vol_space_size           BP(81, 88);
    uint8_t esc_sequences            BP(89, 120);
    uint8_t vol_set_size             BP(121, 124);
    uint8_t vol_seq_number           BP(125, 128);
    uint8_t block_size               BP(129, 132);
    uint8_t path_table_size          BP(133, 140);
    uint8_t l_path_table_pos         BP(141, 144);
    uint8_t opt_l_path_table_pos     BP(145, 148);
    uint8_t m_path_table_pos         BP(149, 152);
    uint8_t opt_m_path_table_pos     BP(153, 156);
    uint8_t root_dir_record          BP(157, 190);
    uint8_t vol_set_id               BP(191, 318);
    uint8_t publisher_id             BP(319, 446);
    uint8_t data_prep_id             BP(447, 574);
    uint8_t application_id           BP(575, 702);
    uint8_t copyright_file_id        BP(703, 739);
    uint8_t abstract_file_id         BP(740, 776);
    uint8_t bibliographic_file_id    BP(777, 813);
    uint8_t vol_creation_time        BP(814, 830);
    uint8_t vol_modification_time    BP(831, 847);
    uint8_t vol_expiration_time      BP(848, 864);
    uint8_t vol_effective_time       BP(865, 881);
    uint8_t file_structure_version   BP(882, 882);
    uint8_t reserved1                BP(883, 883);
    uint8_t app_use                  BP(884, 1395);
    uint8_t reserved2                BP(1396, 2048);

} iso_vol_desc_t;

typedef struct {
    uint8_t len_dr                   BP(1, 1);
    uint8_t len_xa                   BP(2, 2);
    uint8_t block                    BP(3, 10);
    uint8_t length                   BP(11, 18);
    uint8_t recording_time           BP(19, 25);
    uint8_t flags                    BP(26, 26);
    uint8_t file_unit_size           BP(27, 27);
    uint8_t interleave_gap_size      BP(28, 28);
    uint8_t vol_seq_number           BP(29, 32);
    uint8_t len_fi                   BP(33, 33);

} iso_dir_record_t;


/**** Internal Structs ****/

typedef struct {

    off_t start;
    off_t end;

} range_t;

typedef struct linked_path_s {

    uint32_t id_length;
    char *record_id;
    struct linked_path_s *link;

} path_t;

typedef struct {

    size_t length;
    char *string;

} string_t;


/**** Generator Structs ****/

typedef struct {

    iso_dir_record_t *root_dir;
    bool parsed;

} single_state_t;

typedef struct {

    tni_iso_t *iso;

    off_t block_pos, block_end;
    off_t rel_pos, rel_end;

    void *block;

} record_state_t;

typedef struct {

    gen_func_t generate;
    void *state;

} generator_t;


/**** Internal Function Types ****/

typedef bool (*type_func_t)(iso_vol_desc_t *);
typedef tni_response_t (*gen_func_t)(void **, void *);


/**** API Structures ****/

typedef enum {

    TNI_SIGNAL_OK,
    TNI_SIGNAL_STOP,
    TNI_SIGNAL_ERR,

} tni_signal_t;

typedef struct {

    tni_signal_t (*fn)(tni_record_t *, void *);
    void *args;

} tni_callback_t;

typedef enum {

    TNI_PARSE_PRIMARY,
    TNI_PARSE_JOLIET,

} tni_parse_t;

typedef struct tni_extent_s {

    uint32_t lba;
    uint32_t length;
    struct tni_extent_s *link;

} tni_extent_t;

typedef enum {

    REC_NORMAL,
    REC_CUR_DIR,
    REC_PARENT_DIR,

} tni_record_type_t;

typedef struct tni_record_s {

    off_t total_size;

    tni_record_type_t type;
    bool is_hidden;
    bool is_dir;

    range_t extent_span;
    uint32_t extent_num;
    tni_extent_t *extent_list;

    uint32_t id_length;
    char *record_id;

} tni_record_t;

typedef struct {

    uint32_t lba_count;
    uint16_t block_size;
    tni_parse_t parse_type;

    bool is_header;
	FILE *file_ptr;
    tni_record_t *root_dir;

} tni_iso_t;


/**** API Functions ****/

tni_response_t tni_open_iso(tni_iso_t *iso, char *path, tni_parse_t parse_type, bool is_header);
tni_response_t tni_read_file(void *buf, tni_iso_t *iso, tni_record_t *rec, off_t rel_pos, size_t size);
tni_response_t tni_read_block(void *block, tni_iso_t *iso, uint32_t lba);
tni_response_t tni_traverse_dir(tni_iso_t *iso, tni_record_t *dir, tni_callback_t *cb);

#endif
