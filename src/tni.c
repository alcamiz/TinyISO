#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iconv.h>

#include "tni.h"

/**** Little-Endian Parsers ****/

static
uint16_t LE_int16(uint8_t *iso_num) {
    return ((uint16_t) (iso_num[0] & 0xff) 
         | ((uint16_t) (iso_num[1] & 0xff) << 8));
}

static
uint32_t LE_int32(uint8_t *iso_num) {
    return ((uint32_t) (LE_int16(iso_num))
         | ((uint32_t) (LE_int16(iso_num + 2))) << 16);
}

static
uint64_t LE_int64(uint8_t *iso_num) {
    return ((uint64_t) (LE_int32(iso_num))
         | ((uint64_t) (LE_int32(iso_num + 4))) << 32);
}


/**** Wrapper Functions ****/

static
tni_response_t handle_iconv(char *from_code, char *to_code,
                            char *from_buff, size_t *from_space,
                            char *to_buff, size_t *to_space) {

    tni_response_t ret_val;
    iconv_t id_transform;
    size_t iconv_ret, real_space;

    id_transform = iconv_open(to_code, from_code);
    if (iconv_ret == -1) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    iconv_ret = iconv(id_transform, &from_buff, from_space,
                        &to_buff, to_space);
    if (iconv_ret < 0) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    if (iconv_close(id_transform) == -1) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    ret_val = TNI_OK;
    exit_normal:
        return ret_val;
}

static
tni_response_t handle_alloc(void **mem, size_t count, size_t size, bool clear) {

    tni_response_t ret_val;
    void *in_mem;

    if (clear) {
        in_mem = calloc(count, size);
    } else {
        in_mem = malloc(count * size);
    }

    if (in_mem == NULL) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    *mem = in_mem;
    ret_val = TNI_OK;

    exit_normal:
        return ret_val;
}

static
tni_response_t handle_fopen(FILE **file, char *filename, char *mode) {

    tni_response_t ret_val;

    *file = fopen(filename, mode);

    if (file == NULL) {
        ret_val = TNI_ERR_FILE;
        goto exit_normal;
    }

    ret_val = TNI_OK;
    exit_normal:
        return ret_val;
}

static
tni_response_t handle_fseek(FILE *file, off_t loc, int whence) {

    tni_response_t ret_val;
    int seek_ret;

    seek_ret = fseeko(file, loc, whence);
    if (seek_ret != 0) {
        ret_val = TNI_ERR_FILE;
        goto exit_normal;
    }

    ret_val = TNI_OK;
    exit_normal:
        return ret_val;
}

static
tni_response_t handle_fread(void *buf, size_t size, size_t count, FILE *file) {

    tni_response_t ret_val;
    size_t read_ret;

    read_ret = fread(buf, size, count, file);
    if (read_ret < 1) {
        ret_val = TNI_ERR_FILE;
        goto exit_normal;
    }

    ret_val = TNI_OK;
    exit_normal:
        return ret_val;
}


/**** Descriptor Parsing ****/

static
bool detect_primary(iso_vol_desc_t *desc) {
    return desc->vol_desc_type[0] == 1;
}

static
bool detect_joliet(iso_vol_desc_t *desc) {
    bool is_svd = desc->vol_desc_type == 2;
}

static
tni_response_t read_desc(iso_vol_desc_t *desc, FILE *file_ptr, off_t pos) {
    
    tni_response_t ret_val;

    ret_val = handle_fseek(file_ptr, pos, SEEK_SET);
    if (ret_val != TNI_OK) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    ret_val = handle_fread((void *) desc, DESC_SIZE, 1, file_ptr);
    if (ret_val != TNI_OK) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    ret_val = TNI_OK;
    exit_normal:
        return ret_val;
}

static
tni_response_t search_desc(iso_vol_desc_t *desc, FILE *file_ptr,
                            type_func_t is_type) {

    tni_response_t ret_val;
    off_t cur_pos;

    cur_pos = SECTOR_SIZE * RESV_SECTORS;

    while (true) {

        ret_val = read_desc(desc, file_ptr, cur_pos);
        if (ret_val != TNI_OK) {
            ret_val = TNI_ERROR;
            goto exit_normal;
        }

        if (is_type(desc)) {
            ret_val = TNI_OK;
            goto exit_normal;
        }

        if (desc->vol_desc_type == SECTOR_TAIL) {
            ret_val = TNI_FAIL;
            goto exit_normal;
        }

        cur_pos += DESC_SIZE;
    }

    exit_normal:
        return ret_val;
}


/**** Record Parsing ****/

static
tni_response_t run_generator(void **output, generator_t *d_gen) {
    return d_gen->generate(output, d_gen->state);
}

static
tni_response_t single_generator(void **output, void *raw_state) {

    tni_response_t ret_val;
    single_state_t *state;

    state = (single_state_t *) raw_state;
    if (!(state->parsed)) {
        *output = (void *) state->root_dir;
        state->parsed = true;
        ret_val = TNI_OK;
    } else {
        ret_val = TNI_FAIL;
    }

    return ret_val;
}

static
tni_response_t record_generator(void **output, void *raw_state) {

    tni_response_t ret_val;
    bool is_rec, iso_valid, final_block;

    record_state_t *state;
    iso_dir_record_t *raw_rec;
    tni_iso_t *iso;
    off_t t_end;

    if (output == NULL || raw_state == NULL) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    state = (record_state_t *) raw_state;
    iso = state->iso;
    final_block = state->block_pos == state->block_end;

    t_end = (final_block)? state->rel_end : iso->block_size;
    if (state->rel_pos < t_end - sizeof(iso_dir_record_t)) {

        raw_rec = (tni_record_t *) (state->block + state->rel_pos);
        is_rec = raw_rec->len_dr[0] != 0;
        iso_valid = state->rel_pos + raw_rec->len_dr[0] <= iso->block_size;

        if (is_rec) {
            if (iso_valid) {
                *output = (void *) raw_rec;
                state->rel_pos += raw_rec->len_dr[0];
                ret_val = TNI_OK;
                goto exit_normal;

            } else {
                ret_val = TNI_ERR_ISO;
                goto exit_normal;
            }

            goto exit_normal;
        }
    }

    if (final_block) {
        ret_val = TNI_FAIL;
    }

    state->block_pos += 1;

    ret_val = handle_fseek(iso->file_ptr,
                            state->block_pos * iso->block_size, SEEK_SET);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    ret_val = handle_fread(state->block, iso->block_size, 1, iso->file_ptr);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    raw_rec = (tni_record_t *) state->block;
    if (raw_rec->len_dr[0] == 0) {
        ret_val = TNI_FAIL;
    }

    if (raw_rec->len_dr > iso->block_size) {
        ret_val = TNI_ERR_ISO;
    }

    state->rel_pos = raw_rec->len_dr[0];
    *output = raw_rec;

    ret_val = TNI_OK;
    exit_normal:
        return ret_val;
}

static
tni_response_t parse_record(tni_record_t *rec, tni_iso_t *iso, generator_t *d_gen) {

    tni_response_t ret_val;
    iso_dir_record_t *raw_rec;
    bool multi_extent;
    tni_extent_t *cur_extent, *t_ext;

    off_t local_start, local_end;
    char *ucs_name, *utf8_name;
    size_t ucs_len, utf8_len;
    char *encoding;

    if (rec == NULL || d_gen == NULL) {
        ret_val = TNI_ERROR;
        goto exit_normal;
    }

    ret_val = run_generator(&raw_rec, d_gen);
    if (ret_val != 0) {
        goto exit_normal;
    }

    rec->total_size = LE_int32(&raw_rec->length[0]);

    rec->is_hidden = (raw_rec->flags[0] & 0x1);
    rec->is_dir = (raw_rec->flags[0] & 0x2);

    ucs_len = (size_t) raw_rec->len_fi[0];
    ucs_name = (char *) (((void *) raw_rec) + sizeof(iso_dir_record_t));

    utf8_len = (ucs_len * 3) / 2;
    ret_val = handle_alloc(&utf8_name, utf8_len + 2, 1, true);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    if (ucs_name[0] <= 1) {

        switch(ucs_name[0]) {
            case '\0':
                utf8_name[0] = '.';
                rec->id_length = 1;
                rec->type = REC_CUR_DIR;
                break;
            case '\1':
                utf8_name[0] = '.';
                utf8_name[1] = '.';
                rec->id_length = 2;
                rec->type = REC_PARENT_DIR;
                break;
        }
    } else {

        if (iso->parse_type == TNI_PARSE_PRIMARY) {
            encoding = "ASCII";
        } else if (iso->parse_type == TNI_PARSE_JOLIET) {
            encoding = "UCS-2BE";
        }

        ret_val = handle_iconv(encoding, "UTF-8",
                                ucs_name, &ucs_len,
                                utf8_name, &utf8_len);
        if (ret_val != TNI_OK) {
            goto exit_id;
        }

        rec->type = REC_NORMAL;
        rec->id_length = ((ucs_len * 3) / 2) - utf8_len;
    }
    rec->record_id = utf8_name;

    ret_val = handle_alloc(&(rec->extent_list), 1,
                                sizeof(tni_extent_t), false);
    if (ret_val != TNI_OK) {
        goto exit_id;
    }
    cur_extent = rec->extent_list;

    cur_extent->lba = LE_int32(&raw_rec->block[0]);
    cur_extent->length = LE_int32(&raw_rec->length[0]);
    cur_extent->link = NULL;

    rec->extent_span.start = cur_extent->lba * iso->block_size;
    rec->extent_span.end = rec->extent_span.start + cur_extent->length;
    rec->extent_num = 1;

    multi_extent = raw_rec->flags[0] & EXTENT_FLAG;
    while (multi_extent) {

        ret_val = run_generator(&raw_rec, d_gen);
        if (ret_val == TNI_FAIL) {
            ret_val = TNI_ERR_ISO;
            goto exit_extent;

        } else if (ret_val != TNI_OK) {
            goto exit_extent;
        }

        ret_val = handle_alloc(&(cur_extent->link), 1,
                                sizeof(tni_extent_t), false);
        if (ret_val != TNI_OK) {
            goto exit_extent;
        }

        cur_extent = cur_extent->link;
        cur_extent->lba = LE_int32(&raw_rec->block[0]);
        cur_extent->length = LE_int32(&raw_rec->length[0]);
        cur_extent->link = NULL;

        local_start = cur_extent->lba * iso->block_size;
        local_end = local_start + cur_extent->length;

        rec->extent_span.start = MIN(rec->extent_span.start, local_start);
        rec->extent_span.end = MAX(rec->extent_span.end, local_end);

        rec->extent_num += 1;
        rec->total_size += cur_extent->length;

        multi_extent = raw_rec->flags[0] & EXTENT_FLAG;
    }

    ret_val = TNI_OK;
    goto exit_normal;

    exit_extent:
        cur_extent = rec->extent_list;
        while(cur_extent != NULL) {
            t_ext = cur_extent->link;
            free(cur_extent);
            cur_extent = t_ext;
        }
    exit_id:
        free(utf8_name);
    exit_normal:
        return ret_val;
}

static
void free_record(tni_record_t *rec) {

    tni_extent_t *t_ext;
    while (rec->extent_list != NULL) {
        t_ext = rec->extent_list->link;
        free(rec->extent_list);
        rec->extent_list = t_ext;
    }
    free(rec->record_id);
}


/**** API Functions ****/

tni_response_t tni_open_iso(tni_iso_t *iso, char *path, tni_parse_t parse_type,
                            bool is_header) {

    tni_response_t ret_val;
    FILE *iso_file;
    type_func_t t_func;
    iso_vol_desc_t desc;
    bool desc_success;

    single_state_t root_state;
    generator_t d_gen;

    if (iso == NULL || path == NULL) {
        ret_val = TNI_ERR_ARGS;
        goto exit_normal;
    }

    ret_val = handle_fopen(&iso_file, path, "r");
    if (ret_val != TNI_OK) {
        goto exit_normal; 
    }

    switch(parse_type) {

        case TNI_PARSE_PRIMARY:
            t_func = *detect_primary;
            break;
        case TNI_PARSE_JOLIET:
            t_func = *detect_joliet;
            break;
        default:
            break;
    }

    ret_val = search_desc(&desc, iso_file, t_func);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    iso->lba_count = LE_int32(&(desc.vol_space_size));
    iso->block_size = LE_int16(&(desc.block_size));
    iso->parse_type = parse_type;
    iso->is_header = is_header;
    iso->file_ptr = iso_file;

    ret_val = handle_alloc(&(iso->root_dir), 1, 
                            sizeof(iso_dir_record_t), false);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    root_state.root_dir = &(desc.root_dir_record);
    root_state.parsed = false;

    d_gen.generate = single_generator;
    d_gen.state = (void *) &root_state;

    ret_val = parse_record(iso->root_dir, iso, &d_gen);
    if (ret_val != TNI_OK) {
        goto exit_root;
    }

    ret_val = TNI_OK;
    goto exit_normal;

    exit_root:
        free(iso->root_dir);
    exit_normal:
        return ret_val;
}

tni_response_t tni_read_block(void *block, tni_iso_t *iso, uint32_t lba) {

    tni_response_t ret_val;

    ret_val = handle_fseek(iso->file_ptr, lba * iso->block_size, SEEK_SET);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    ret_val = handle_fread(block, iso->block_size, 1, iso->file_ptr);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    ret_val = TNI_OK;
    exit_normal:
        return ret_val;
}

tni_response_t tni_traverse_dir(tni_iso_t *iso, tni_record_t *dir, imn_callback_t *cb) {

    tni_response_t ret_val;
    tni_signal_t signal;

    record_state_t state;
    generator_t gen;

    tni_record_t cur_rec;
    tni_extent_t *cur_extent;
    off_t local_start, local_end;

    if (iso == NULL || dir == NULL) {
        ret_val = TNI_ERR_ARGS;
        goto exit_normal;
    }

    if (!(dir->is_dir)) {
        ret_val = TNI_ERR_DIR;
        goto exit_normal;
    }

    gen.generate = record_generator;
    gen.state = &state;

    state.iso = iso;
    ret_val = handle_alloc(&(state.block), 1, iso->block_size, false);
    if (ret_val != TNI_OK) {
        goto exit_normal;
    }

    cur_extent = dir->extent_list;
    while (cur_extent != NULL) {

        local_start = state.block_pos * iso->block_size;
        local_end = local_start + cur_extent->length;

        state.block_pos = cur_extent->lba;
        state.block_end = local_end / iso->block_size;

        state.rel_pos = 0;
        state.rel_end = local_end % iso->block_size;

        ret_val = tni_read_block(state.block, iso, cur_extent->lba);
        if (ret_val != TNI_OK) {
            goto exit_block;
        }

        while (true) {
            ret_val = parse_record(&cur_rec, iso, &gen);
            if (ret_val == TNI_FAIL) {
                break;
            }
            if (ret_val != TNI_OK) {
                break;
            }

            signal = cb->fn(&cur_rec, cb->args);
            if (signal == TNI_SIGNAL_STOP) {
                ret_val = TNI_OK;
                goto exit_record;
            }
            if (signal == TNI_SIGNAL_ERR) {
                ret_val = TNI_ERR_CB;
                goto exit_record;
            }

            free_record(&cur_rec);
        }
        cur_extent = cur_extent->link;
    }

    ret_val = TNI_OK;
    goto exit_normal;

    exit_record:
        free_record(&cur_rec);
    exit_block:
        free(state.block);
    exit_normal:
        return ret_val;
}
