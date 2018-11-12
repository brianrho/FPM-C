#include "fpm.h"
#include <string.h>

#if defined(FPM_ENABLE_DEBUG)
    #define FPM_DEFAULT_STREAM          Serial

    #define FPM_DEBUG_PRINT(x)          FPM_DEFAULT_STREAM.print(x)
    #define FPM_DEBUG_PRINTLN(x)        FPM_DEFAULT_STREAM.println(x)
    #define FPM_DEBUG_DEC(x)            FPM_DEFAULT_STREAM.print(x)
    #define FPM_DEBUG_DECLN(x)          FPM_DEFAULT_STREAM.println(x)
    #define FPM_DEBUG_HEX(x)            FPM_DEFAULT_STREAM.print(x, HEX)
    #define FPM_DEBUG_HEXLN(x)          FPM_DEFAULT_STREAM.println(x, HEX) 
#else
    #define FPM_DEBUG_PRINT(x)
    #define FPM_DEBUG_PRINTLN(x)
    #define FPM_DEBUG_DEC(x)          
    #define FPM_DEBUG_DECLN(x)
    #define FPM_DEBUG_HEX(x)
    #define FPM_DEBUG_HEXLN(x)
#endif

static void write_packet(FPM * fpm, uint8_t packettype, uint8_t * packet, uint16_t len);
static int16_t get_reply(FPM * fpm, uint8_t * replyBuf, uint16_t buflen, uint8_t * pktid, fpm_uart_write_func out_stream);
static int16_t read_ack_get_response(FPM * fpm, uint8_t * rc);

const uint16_t fpm_packet_lengths[] = {32, 64, 128, 256};
static fpm_millis_func millis_func;
static fpm_delay_func delay_func;

typedef enum {
    FPM_STATE_READ_HEADER,
    FPM_STATE_READ_ADDRESS,
    FPM_STATE_READ_PID,
    FPM_STATE_READ_LENGTH,
    FPM_STATE_READ_CONTENTS,
    FPM_STATE_READ_CHECKSUM
} FPM_State;

uint8_t fpm_begin(FPM * fpm, fpm_millis_func _millis_func, fpm_delay_func _delay_func) {
    millis_func = _millis_func;
    delay_func = _delay_func;
    
    delay_func(1000);            // 500 ms at least according to datasheet
    
    fpm->buffer[0] = FPM_VERIFYPASSWORD;
    fpm->buffer[1] = (fpm->password >> 24) & 0xff; fpm->buffer[2] = (fpm->password >> 16) & 0xff;
    fpm->buffer[3] = (fpm->password >> 8) & 0xff; fpm->buffer[4] = fpm->password & 0xff;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 5);
    
    uint8_t confirm_code = 0;
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0 || confirm_code != FPM_OK)
        return 0;

    if (fpm_read_params(fpm, NULL) != FPM_OK)
        return 0;
    
    return 1;
}

int16_t fpm_set_password(FPM * fpm, uint32_t pwd) {
    fpm->buffer[0] = FPM_SETPASSWORD;
    fpm->buffer[1] = (pwd >> 24) & 0xff; fpm->buffer[2] = (pwd >> 16) & 0xff;
    fpm->buffer[3] = (pwd >> 8) & 0xff; fpm->buffer[4] = pwd & 0xff;
    
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 5);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}

int16_t fpm_get_image(FPM * fpm) {
    fpm->buffer[0] = FPM_GETIMAGE;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}

// for ZFM60 modules
int16_t fpm_get_imageNL(FPM * fpm) {
    fpm->buffer[0] = FPM_GETIMAGE_NOLIGHT;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}

// for ZFM60 modules
int16_t fpm_led_on(FPM * fpm) {
    fpm->buffer[0] = FPM_LEDON;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}

// for ZFM60 modules
int16_t fpm_led_off(FPM * fpm) {
    fpm->buffer[0] = FPM_LEDOFF;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}

/* tested on R551 modules,
   standby current measured at 10uA, UART and LEDs turned off,
   no other documentation available */
int16_t fpm_standby(FPM * fpm) {
    fpm->buffer[0] = FPM_STANDBY;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}

int16_t fpm_image2Tz(FPM * fpm, uint8_t slot) {
    fpm->buffer[0] = FPM_IMAGE2TZ; 
    fpm->buffer[1] = slot;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 2);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}


int16_t fpm_create_model(FPM * fpm) {
    fpm->buffer[0] = FPM_REGMODEL;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}


int16_t fpm_store_model(FPM * fpm, uint16_t id, uint8_t slot) {
    fpm->buffer[0] = FPM_STORE;
    fpm->buffer[1] = slot;
    fpm->buffer[2] = id >> 8; fpm->buffer[3] = id & 0xFF;
    
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 4);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}
    
//read a fingerprint template from flash into Char Buffer 1
int16_t fpm_load_model(FPM * fpm, uint16_t id, uint8_t slot) {
    fpm->buffer[0] = FPM_LOAD;
    fpm->buffer[1] = slot;
    fpm->buffer[2] = id >> 8; fpm->buffer[3] = id & 0xFF;
    
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 4);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);
    
    if (rc < 0)
        return rc;
    
    return confirm_code;
}


int16_t fpm_set_param(FPM * fpm, uint8_t param, uint8_t value) {
	fpm->buffer[0] = FPM_SETSYSPARAM;
    fpm->buffer[1] = param; fpm->buffer[2] = value;
    
	write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 3);
    uint8_t confirm_code = 0;
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0)
        return len;
    
    if (confirm_code != FPM_OK)
        return confirm_code;
    
    delay_func(100);
    fpm_read_params(fpm, NULL);
    return confirm_code;
}

static void reverse_bytes(void *start, uint16_t size) {
    uint8_t *lo = (uint8_t *)start;
    uint8_t *hi = (uint8_t *)start + size - 1;
    uint8_t swap;
    while (lo < hi) {
        swap = *lo;
        *lo++ = *hi;
        *hi-- = swap;
    }
}

int16_t fpm_read_params(FPM * fpm, FPM_System_Params * user_params) {
    fpm->buffer[0] = FPM_READSYSPARAM;
    
	write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0)
        return len;
    
    if (confirm_code != FPM_OK)
        return confirm_code;
    
    if (len != 16)
        return FPM_READ_ERROR;
    
    memcpy(&fpm->sys_params, &fpm->buffer[1], 16);
    reverse_bytes(&fpm->sys_params.status_reg, 2);
    reverse_bytes(&fpm->sys_params.system_id, 2);
    reverse_bytes(&fpm->sys_params.capacity, 2);
    reverse_bytes(&fpm->sys_params.security_level, 2);
    reverse_bytes(&fpm->sys_params.device_addr, 4);
    reverse_bytes(&fpm->sys_params.packet_len, 2);
    reverse_bytes(&fpm->sys_params.baud_rate, 2);
    
    if (user_params != NULL)
        memcpy(user_params, &fpm->sys_params, 16);
    
    return confirm_code;
}

// NEW: download fingerprint image to pc
int16_t fpm_down_image(FPM * fpm) {
	fpm->buffer[0] = FPM_IMGUPLOAD;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);

    if (rc < 0)
        return rc;

    return confirm_code;
}

/* extract template/img data from packets and return 1 if successful */
uint8_t fpm_read_raw(FPM * fpm, uint8_t outType, void * out, uint8_t * read_complete, uint16_t * read_len) {
    fpm_uart_write_func out_stream;
    uint8_t * outBuf;
    
    if (outType == FPM_OUTPUT_TO_BUFFER)
        outBuf = (uint8_t *)out;
    else if (outType == FPM_OUTPUT_TO_STREAM)
        out_stream = (fpm_uart_write_func)out;
    else
        return 0;
    
    uint16_t chunk_sz = fpm_packet_lengths[fpm->sys_params.packet_len];
    uint8_t pid;
    int16_t len;
    
    if (outType == FPM_OUTPUT_TO_BUFFER)
        len = get_reply(fpm, outBuf, *read_len, &pid, NULL);
    else if (outType == FPM_OUTPUT_TO_STREAM)
        len = get_reply(fpm, NULL, 0, &pid, out_stream);
    
    /* check the length */
    if (len != chunk_sz) {
        FPM_DEBUG_PRINT("Read data failed: ");
        FPM_DEBUG_PRINTLN(len);
        return 0;
    }
    
    *read_complete = 0;
    
    if (pid == FPM_DATAPACKET || pid == FPM_ENDDATAPACKET) {
        if (outType == FPM_OUTPUT_TO_BUFFER)
            *read_len = len;
        if (pid == FPM_ENDDATAPACKET)
            *read_complete = 1;
        return 1;
    }
    
    return 0;
}

void fpm_write_raw(FPM * fpm, uint8_t * data, uint16_t len) {
    uint16_t written = 0;
    uint16_t chunk_sz = fpm_packet_lengths[fpm->sys_params.packet_len];
    
    while (len > chunk_sz) {
        write_packet(fpm, FPM_DATAPACKET, &data[written], chunk_sz);
        written += chunk_sz;
        len -= chunk_sz;
    }
    write_packet(fpm, FPM_ENDDATAPACKET, &data[written], len);
}

//transfer a fingerprint template from Char Buffer 1 to host computer
int16_t fpm_get_model(FPM * fpm, uint8_t slot) {
    fpm->buffer[0] = FPM_UPLOAD;
    fpm->buffer[1] = slot;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 2);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);

    if (rc < 0)
        return rc;

    return confirm_code;
}

int16_t fpm_upload_model(FPM * fpm) {
    fpm->buffer[0] = FPM_DOWNCHAR;
    fpm->buffer[1] = 0x01;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 2);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);

    if (rc < 0)
        return rc;

    return confirm_code;
}
    
int16_t fpm_delete_model(FPM * fpm, uint16_t id, uint16_t how_many) {
    fpm->buffer[0] = FPM_DELETE;
    fpm->buffer[1] = id >> 8; fpm->buffer[2] = id & 0xFF;
    fpm->buffer[3] = how_many >> 8; fpm->buffer[4] = how_many & 0xFF;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 5);
    
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);

    if (rc < 0)
        return rc;

    return confirm_code;
}

int16_t fpm_empty_database(FPM * fpm) {
    fpm->buffer[0] = FPM_EMPTYDATABASE;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    int16_t rc = read_ack_get_response(fpm, &confirm_code);

    if (rc < 0)
        return rc;

    return confirm_code;
}

int16_t fpm_search_database(FPM * fpm, uint16_t * finger_id, uint16_t * score, uint8_t slot) {
    // high speed search of slot #1 starting at page 0 to 'capacity'
    #if defined(FPM_R551_MODULE)
    fpm->buffer[0] = FPM_SEARCH;
    #else
    fpm->buffer[0] = FPM_HISPEEDSEARCH;
    #endif
    fpm->buffer[1] = slot;
    fpm->buffer[2] = 0x00; fpm->buffer[3] = 0x00;
    fpm->buffer[4] = (uint8_t)(fpm->sys_params.capacity >> 8);
    fpm->buffer[5] = (uint8_t)(fpm->sys_params.capacity & 0xFF);
    
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 6);
    uint8_t confirm_code = 0;
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0)
        return len;
    
    if (confirm_code != FPM_OK)
        return confirm_code;
    
    if (len != 4)
        return FPM_READ_ERROR;

    *finger_id = fpm->buffer[1];
    *finger_id <<= 8;
    *finger_id |= fpm->buffer[2];

    *score = fpm->buffer[3];
    *score <<= 8;
    *score |= fpm->buffer[4];

    return confirm_code;
}

int16_t fpm_match_template_pair(FPM * fpm, uint16_t * score) {
    fpm->buffer[0] = FPM_PAIRMATCH;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0)
        return len;
    
    if (confirm_code != FPM_OK)
        return confirm_code;
    
    if (len != 2)
        return FPM_READ_ERROR;
    
    *score = fpm->buffer[1]; 
    *score <<= 8;
    *score |= fpm->buffer[2];

    return confirm_code;
}

int16_t fpm_get_template_count(FPM * fpm, uint16_t * template_cnt) {
    fpm->buffer[0] = FPM_TEMPLATECOUNT;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0)
        return len;
    
    if (confirm_code != FPM_OK)
        return confirm_code;
    
    if (len != 2)
        return FPM_READ_ERROR;
    
    *template_cnt = fpm->buffer[1];
    *template_cnt <<= 8;
    *template_cnt |= fpm->buffer[2];

    return confirm_code;
}

int16_t fpm_get_free_index(FPM * fpm, uint8_t page, int16_t * id) {
    fpm->buffer[0] = FPM_READTEMPLATEINDEX; 
    fpm->buffer[1] = page;
    
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 2);
    uint8_t confirm_code = 0;
    
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0)
        return len;
    
    if (confirm_code != FPM_OK)
        return confirm_code;
    
    for (int group_idx = 0; group_idx < len; group_idx++) {
        uint8_t group = fpm->buffer[1 + group_idx];
        if (group == 0xff)        /* if group is all occupied */
            continue;
        
        for (uint8_t bit_mask = 0x01, fid = 0; bit_mask != 0; bit_mask <<= 1, fid++) {
            if ((bit_mask & group) == 0) {
                #if defined(FPM_R551_MODULE)
                if (page == 0 && group_idx == 0 && fid == 0)     /* Skip LSb of first group */
                    continue;
                *id = (FPM_TEMPLATES_PER_PAGE * page) + (group_idx * 8) + fid - 1;      /* all IDs are off by one */
                #else
                *id = (FPM_TEMPLATES_PER_PAGE * page) + (group_idx * 8) + fid;
                #endif
                return confirm_code;
            }
        }
    }
    
    *id = FPM_NOFREEINDEX;  // no free space found
    return confirm_code;
}

int16_t fpm_get_random_number(FPM * fpm, uint32_t * number) {
    fpm->buffer[0] = FPM_GETRANDOM;
    write_packet(fpm, FPM_COMMANDPACKET, fpm->buffer, 1);
    uint8_t confirm_code = 0;
    
    int16_t len = read_ack_get_response(fpm, &confirm_code);
    
    if (len < 0)
        return len;
    
    if (confirm_code != FPM_OK)
        return confirm_code;
    
    if (len != 4)
        return FPM_READ_ERROR;
    
    *number = fpm->buffer[1]; 
    *number <<= 8;
    *number |= fpm->buffer[2];
    *number <<= 8;
    *number |= fpm->buffer[3];
    *number <<= 8;
    *number |= fpm->buffer[4];

    return confirm_code;
}

static void write_packet(FPM * fpm, uint8_t packettype, uint8_t * packet, uint16_t len) {
    len += 2;
    
    uint8_t preamble[] = {(uint8_t)(FPM_STARTCODE >> 8), (uint8_t)FPM_STARTCODE,
                          (uint8_t)(fpm->address >> 24), (uint8_t)(fpm->address >> 16),
                          (uint8_t)(fpm->address >> 8), (uint8_t)(fpm->address),
                          (uint8_t)packettype, (uint8_t)(len >> 8), (uint8_t)(len) };
    
    fpm->write_func(preamble, sizeof(preamble));
  
    uint16_t sum = (len >> 8) + (len & 0xFF) + packettype;
    for (uint8_t i = 0; i < len - 2; i++) {
        fpm->write_func(&packet[i], 1);
        sum += packet[i];
    }
    
    /* assume little-endian mcu */
    fpm->write_func((uint8_t *)(&sum) + 1, 1);
    fpm->write_func((uint8_t *)&sum, 1);
}

static int16_t get_reply(FPM * fpm, uint8_t * replyBuf, uint16_t buflen, 
                        uint8_t * pktid, fpm_uart_write_func out_stream) {
                            
    FPM_State state = FPM_STATE_READ_HEADER;
    
    uint16_t header = 0;
    uint8_t pid = 0;
    uint16_t length = 0;
    uint16_t chksum = 0;
    uint16_t remn = 0;
    
    uint32_t last_read = millis_func();
    
    while ((uint32_t)(millis_func() - last_read) < FPM_DEFAULT_TIMEOUT) {        
        switch (state) {
            case FPM_STATE_READ_HEADER: {
                if (fpm->avail_func() == 0)
                    continue;
                
                last_read = millis_func();
                uint8_t byte;
                fpm->read_func(&byte, 1);
                
                header <<= 8; header |= byte;
                if (header != FPM_STARTCODE)
                    break;
                
                state = FPM_STATE_READ_ADDRESS;
                header = 0;
                
                FPM_DEBUG_PRINTLN("\r\n[+]Got header");
                break;
            }
            case FPM_STATE_READ_ADDRESS: {
                if (fpm->avail_func() < 4)
                    continue;
                
                last_read = millis_func();
                fpm->read_func(fpm->buffer, 4);
                uint32_t addr = fpm->buffer[0]; addr <<= 8; 
                addr |= fpm->buffer[1]; addr <<= 8;
                addr |= fpm->buffer[2]; addr <<= 8;
                addr |= fpm->buffer[3];
                
                if (addr != fpm->address) {
                    state = FPM_STATE_READ_HEADER;
                    FPM_DEBUG_PRINTLN("[+]Wrong fpm->address");
                    break;
                }
                
                state = FPM_STATE_READ_PID;
                FPM_DEBUG_PRINT("[+]fpm->address: 0x"); FPM_DEBUG_HEXLN(fpm->address);
                
                break;
            }
            case FPM_STATE_READ_PID:
                if (fpm->avail_func() == 0)
                    continue;
                
                last_read = millis_func();
                fpm->read_func(&pid, 1);
                chksum = pid;
                *pktid = pid;
                
                state = FPM_STATE_READ_LENGTH;
                FPM_DEBUG_PRINT("[+]PID: 0x"); FPM_DEBUG_HEXLN(pid);
                
                break;
            case FPM_STATE_READ_LENGTH: {
                if (fpm->avail_func() < 2)
                    continue;
                
                last_read = millis_func();
                fpm->read_func(fpm->buffer, 2);
                length = fpm->buffer[0]; length <<= 8;
                length |= fpm->buffer[1];
                
                if (length > FPM_MAX_PACKET_LEN + 2 || (out_stream == NULL && length > buflen + 2)) {
                    state = FPM_STATE_READ_HEADER;
                    FPM_DEBUG_PRINTLN("[+]Packet too long");
                    continue;
                }
                
                /* num of bytes left to read */
                remn = length;
                
                chksum += fpm->buffer[0]; chksum += fpm->buffer[1];
                state = FPM_STATE_READ_CONTENTS;
                FPM_DEBUG_PRINT("[+]Length: "); FPM_DEBUG_DECLN(length - 2);
                break;
            }
            case FPM_STATE_READ_CONTENTS: {
                if (remn <= 2) {
                    state = FPM_STATE_READ_CHECKSUM;
                    break;
                }
                
                if (fpm->avail_func() == 0)
                    continue;
                
                last_read = millis_func();
                
                /* we now have to stop using 'fpm->buffer' since
                 * we may be storing data in it now */
                uint8_t byte;
                fpm->read_func(&byte, 1);
                if (out_stream != NULL) {
                    out_stream(&byte, 1);
                }
                else {
                    *replyBuf++ = byte;
                    chksum += byte;
                }
                
                FPM_DEBUG_HEX(byte); FPM_DEBUG_PRINT(" ");
                remn--;
                break;
            }
            case FPM_STATE_READ_CHECKSUM: {
                if (fpm->avail_func() < 2)
                    continue;
                
                last_read = millis_func();
                uint8_t temp[2];
                fpm->read_func(temp, 2);
                uint16_t to_check = temp[0]; to_check <<= 8;
                to_check |= temp[1];
                
                if (out_stream == NULL && to_check != chksum) {
                    state = FPM_STATE_READ_HEADER;
                    FPM_DEBUG_PRINTLN("\r\n[+]Wrong chksum");
                    continue;
                }
                
                FPM_DEBUG_PRINTLN("\r\n[+]Read complete");
                /* without chksum */
                return length - 2;
            }
        }
    }
    
    FPM_DEBUG_PRINTLN();
    return FPM_TIMEOUT;
}

/* read standard ACK-reply into library fpm->buffer and
 * return packet length and confirmation code */
static int16_t read_ack_get_response(FPM * fpm, uint8_t * rc) {
    uint8_t pktid = 0;
    int16_t len = get_reply(fpm, fpm->buffer, FPM_BUFFER_SZ, &pktid, NULL);
    
    /* most likely timed out */
    if (len < 0)
        return len;
    
    /* wrong pkt id */
    if (pktid != FPM_ACKPACKET)
        return FPM_READ_ERROR;
    
    *rc = fpm->buffer[0];
    
    /* minus confirmation code */
    return --len;
}
