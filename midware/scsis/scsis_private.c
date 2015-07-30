/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2015, Alexey Kramarenko
    All rights reserved.
*/

#include "scsis_private.h"
#include "../../userspace/stdio.h"
#include "../../userspace/stdlib.h"
#include "sys_config.h"
#include <string.h>

void scsis_error_init(SCSIS* scsis)
{
    rb_init(&scsis->rb_error, SCSI_SENSE_DEPTH);
}

void scsis_error(SCSIS* scsis, uint8_t key_sense, uint16_t ascq)
{
    unsigned int idx;
    if (rb_is_full(&scsis->rb_error))
        rb_get(&scsis->rb_error);
    idx = rb_put(&scsis->rb_error);
    scsis->errors[idx].key_sense = key_sense;
    scsis->errors[idx].ascq = ascq;
#if (SCSI_DEBUG_ERRORS)
    printf("SCSI error: sense key: %02xh, ASC: %02xh, ASQ: %02xh\n", key_sense, ascq >> 8, ascq & 0xff);
#endif //SCSI_DEBUG_ERRORS
}

void scsis_error_get(SCSIS* scsis, SCSIS_ERROR *err)
{
    unsigned int idx;
    if (rb_is_empty(&scsis->rb_error))
    {
        err->key_sense = SENSE_KEY_NO_SENSE;
        err->ascq = ASCQ_NO_ADDITIONAL_SENSE_INFORMATION;
    }
    else
    {
        idx = rb_get(&scsis->rb_error);
        err->key_sense = scsis->errors[idx].key_sense;
        err->ascq = scsis->errors[idx].ascq;
    }
}

void scsis_host_request(SCSIS* scsis, SCSIS_REQUEST request)
{
    scsis->cb_host(scsis->param, scsis->io, request);
}

void scsis_storage_request(SCSIS* scsis, SCSIS_REQUEST request)
{
    scsis->cb_storage(scsis->param, scsis->io, request);
}

void scsis_fatal(SCSIS* scsis)
{
    scsis_reset(scsis);
    scsis_host_request(scsis, SCSIS_REQUEST_INTERNAL_ERROR);
}

void scsis_fail(SCSIS* scsis, uint8_t key_sense, uint16_t ascq)
{
    scsis_error(scsis, key_sense, ascq);
    scsis->state = SCSIS_STATE_IDLE;
    scsis_host_request(scsis, SCSIS_REQUEST_FAIL);
}

void scsis_pass(SCSIS* scsis)
{
    scsis->state = SCSIS_STATE_IDLE;
    scsis_host_request(scsis, SCSIS_REQUEST_PASS);
}

bool scsis_get_storage_descriptor(SCSIS* scsis)
{
    if (scsis->storage)
        return true;

    switch (scsis->state)
    {
    case SCSIS_STATE_STORAGE_DESCRIPTOR_REQUEST:
        if (scsis->io->data_size < sizeof(void*))
        {
#if (SCSI_DEBUG_ERRORS)
            printf("SCSI: invalid storage descriptor response\n");
#endif //SCSI_DEBUG_ERRORS
            scsis_fatal(scsis);
            return false;
        }
        scsis->storage = malloc(scsis->io->data_size);
        if (scsis->storage == NULL)
        {
#if (SCSI_DEBUG_ERRORS)
            printf("SCSI: out of memory\n");
#endif //SCSI_DEBUG_ERRORS
            scsis_fatal(scsis);
            return false;
        }
        memcpy(scsis->storage, io_data(scsis->io), scsis->io->data_size);
        if (*(scsis->storage) == NULL)
            *(scsis->storage) = ((void*)(scsis->storage)) + sizeof(void*);
        scsis->state = SCSIS_STATE_IDLE;
        break;
    case SCSIS_STATE_IDLE:
        scsis->state = SCSIS_STATE_STORAGE_DESCRIPTOR_REQUEST;
        scsis_storage_request(scsis, SCSIS_REQUEST_GET_STORAGE_DESCRIPTOR);
        return false;
    default:
#if (SCSI_DEBUG_ERRORS)
        printf("SCSI: invalid state on descriptor request: %d\n", scsis->state);
#endif //SCSI_DEBUG_ERRORS
        scsis_fatal(scsis);
        return false;
    }
    return true;
}

bool scsis_get_media_descriptor(SCSIS* scsis)
{
    if (scsis->media)
        return true;

    switch (scsis->state)
    {
    case SCSIS_STATE_MEDIA_DESCRIPTOR_REQUEST:
        if (scsis->io->data_size < sizeof(void*))
        {
#if (SCSI_DEBUG_ERRORS)
            printf("SCSI: invalid media descriptor response\n");
#endif //SCSI_DEBUG_ERRORS
            scsis_fatal(scsis);
            return false;
        }
        scsis->media = malloc(scsis->io->data_size);
        if (scsis->media == NULL)
        {
#if (SCSI_DEBUG_ERRORS)
            printf("SCSI: out of memory\n");
#endif //SCSI_DEBUG_ERRORS
            scsis_fatal(scsis);
            return false;
        }
        memcpy(scsis->media, io_data(scsis->io), scsis->io->data_size);
        if (*(scsis->media) == NULL)
            *(scsis->media) = ((void*)(scsis->media)) + sizeof(void*);
        scsis->state = SCSIS_STATE_IDLE;
        break;
    case SCSIS_STATE_IDLE:
        scsis->state = SCSIS_STATE_MEDIA_DESCRIPTOR_REQUEST;
        scsis_storage_request(scsis, SCSIS_REQUEST_GET_MEDIA_DESCRIPTOR);
        return false;
    default:
#if (SCSI_DEBUG_ERRORS)
        printf("SCSI: invalid state on descriptor request: %d\n", scsis->state);
#endif //SCSI_DEBUG_ERRORS
        scsis_fatal(scsis);
        return false;
    }
    return true;
}

bool scsis_get_media(SCSIS* scsis)
{
    if (!scsis_get_media_descriptor(scsis))
        return false;
    if (scsis->media == NULL)
    {
        scsis_fail(scsis, SENSE_KEY_NOT_READY, ASCQ_MEDIUM_NOT_PRESENT);
        return false;
    }
    return true;
}