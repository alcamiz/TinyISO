#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "tni.h"
#define PATH_SIZE 4096

typedef struct {
    char *path;
    int idx;
    tni_iso_t *iso;
} arg_t;

tni_signal_t traverse_cb(tni_record_t *rec, void *raw_arg) {

    tni_response_t ret_val;
    tni_callback_t cb;
    arg_t *args;
    int t_id;

    if (rec == NULL || raw_arg == NULL) {
        return TNI_SIGNAL_ERR;
    }
    args = (arg_t *) raw_arg;

    if (rec->type == REC_NORMAL) {

        if (args->idx + rec->id_length + 2  > PATH_SIZE) {
            return TNI_SIGNAL_ERR;
        }

        memcpy(args->path + args->idx, rec->record_id, rec->id_length);

        if (rec->is_dir) {

            args->idx += rec->id_length + 1;
            args->path[args->idx - 1] = '/';
            args->path[args->idx] = '\0';

            cb.fn = traverse_cb;
            cb.args = raw_arg;

            ret_val = tni_traverse_dir(args->iso, rec, &cb);
            if (ret_val != TNI_OK) {
                return TNI_SIGNAL_ERR;
            }

            args->idx -= rec->id_length + 1;
            args->path[args->idx] = '\0';

        } else {
            args->path[args->idx + rec->id_length] = '\0';
            printf("%s\n", args->path);
        }
    }
    return TNI_SIGNAL_OK;
}

int main(int argc, char *argv[]) {

    tni_response_t ret_val;
    tni_iso_t iso;
    tni_callback_t cb;
    
    arg_t args;
    char *path;

    if (argc != 2) {
        printf("Usage: %s ISO-FILE\n", argv[0]);
    }
    
    ret_val = tni_open_iso(&iso, argv[1], TNI_PARSE_JOLIET, false);
    if (ret_val != TNI_OK) {
        printf("ERROR NUM: %d\n", ret_val);
        return EXIT_FAILURE;
    }

    path = malloc(PATH_SIZE);
    if (cb.args == NULL) {
        return EXIT_FAILURE;
    }
    path[0] = '\0';

    args.path = path;
    args.idx = 0;
    args.iso = &iso;

    cb.fn = traverse_cb;
    cb.args = (void *) &args;

    ret_val = tni_traverse_dir(&iso, iso.root_dir, &cb);
    if (ret_val != TNI_OK) {
        printf("ERROR NUM: %d\n", ret_val);
        free(path);
        return EXIT_FAILURE;
    }
}
