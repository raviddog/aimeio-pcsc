#include "aimepcsc.h"
#include <stdio.h>

static const uint8_t atr_ios14443_common[] = {0x3B, 0x8F, 0x80, 0x01, 0x80, 0x4F, 0x0C, 0xA0, 0x00, 0x00, 0x03, 0x06};
static const uint8_t cardtype_m1k[] = {0x03, 0x00, 0x01};
static const uint8_t cardtype_felica[] = {0x11, 0x00, 0x3B};

static const uint8_t felica_cmd_readidm[] = {0xFF, 0xCA, 0x00, 0x00, 0x00};

static const uint8_t m1k_cmd_loadkey[] = {0xFF, 0x82, 0x00, 0x00, 0x06, 0x57, 0x43, 0x43, 0x46, 0x76, 0x32};
static const uint8_t m1k_cmd_auth_block2[] = {0xFF, 0x86, 0x00, 0x00, 0x05, 0x01, 0x00, 0x02, 0x61, 0x00};
static const uint8_t m1k_cmd_read_block2[] = {0xFF, 0xB0, 0x00, 0x02, 0x10};

struct aimepcsc_context {
    SCARDCONTEXT hContext;
    LPSTR mszReaders;
    DWORD pcchReaders;

    CHAR last_error[256];
};

#define APDU_SEND(card, cmd, expected_res_len) \
    do { \
        len = sizeof(buf); \
        ret = SCardTransmit(*card, SCARD_PCI_T1, cmd, sizeof(cmd), NULL, buf, &len); \
        if (ret != SCARD_S_SUCCESS) { \
            snprintf(ctx->last_error, sizeof(ctx->last_error), "SCardTransmit failed during " #cmd ": %08lX", (ULONG) ret); \
            return -1; \
        } \
        if (len != expected_res_len || buf[expected_res_len - 2] != 0x90 || buf[expected_res_len - 1] != 0x00) { \
            snprintf(ctx->last_error, sizeof(ctx->last_error), #cmd " failed; res_len=%lu, res_code=%02x%02x", len, buf[2], buf[3]); \
            return 1; \
        } \
    } while (0)

static void convert_felica_to_access_code(struct aime_data *data) {
    FILE *fptr;
    fptr = fopen("aimeio_felicadb.txt", "r");

    char fid[16];
    for(int i = 0; i < 8; i++) {
        sprintf(&fid[i*2], "%02X", data->card_id[i]);
    }
    printf("Scanned felica %s\n", fid);
    
    if(fptr) {
        int found = 0;
        char buf[100];
        while(fgets(buf, 100, fptr)) {
            if(strncmp(buf, fid, 16) == 0) {
                char access_code[10];
                char *buf_ac = &buf[17];
                for(int i = 0; i < 10; i++)
                {
                    sscanf(&buf_ac[i*2], "%02hhx", &access_code[i]);
                }
                printf("Found match, replacing with access code %s\n", buf_ac);
                memcpy(data->card_id, access_code, 10);
                data->card_id_len = 10;
                data->card_type = Mifare;
                break;
            }
        }
    }
    fclose(fptr);
}

static int read_felica_aime(struct aimepcsc_context *ctx, LPSCARDHANDLE card, struct aime_data *data) {
    uint8_t buf[32];
    DWORD len;
    LONG ret;

    /* read card ID */
    APDU_SEND(card, felica_cmd_readidm, 10);

    data->card_id_len = 8;
    memcpy(data->card_id, buf, 8);

    convert_felica_to_access_code(data);

    return 0;
}

static int read_m1k_aime(struct aimepcsc_context *ctx, LPSCARDHANDLE card, struct aime_data *data) {
    uint8_t buf[32];
    DWORD len;
    LONG ret;

    /* load key onto reader */
    APDU_SEND(card, m1k_cmd_loadkey, 2);

    /* authenticate block 2 */
    APDU_SEND(card, m1k_cmd_auth_block2, 2);

    /* read block 2 */
    APDU_SEND(card, m1k_cmd_read_block2, 18);

    data->card_id_len = 10;
    memcpy(data->card_id, buf + 6, 10);

    return 0;
}

struct aimepcsc_context* aimepcsc_create(void) {
    struct aimepcsc_context* ctx;

    ctx = (struct aimepcsc_context*) malloc(sizeof(struct aimepcsc_context));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(struct aimepcsc_context));
    return ctx;
}

void aimepcsc_destroy(struct aimepcsc_context *ctx) {
    free(ctx);
}

int aimepcsc_init(struct aimepcsc_context *ctx) {
    LONG ret;

    ret = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &ctx->hContext);

    if (ret != SCARD_S_SUCCESS) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "SCardEstablishContext failed: %08lX", (ULONG) ret);
        return -1;
    }

    ctx->pcchReaders = SCARD_AUTOALLOCATE;

    ret = SCardListReaders(ctx->hContext, NULL, (LPSTR) &ctx->mszReaders, &ctx->pcchReaders);

    if (ret != SCARD_S_SUCCESS) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "SCardListReaders failed: %08lX", (ULONG) ret);
        goto errout;
    }

    return 0;

errout:
    SCardReleaseContext(ctx->hContext);
    return -1;
}

void aimepcsc_shutdown(struct aimepcsc_context *ctx) {
    if (ctx->mszReaders != NULL) {
        SCardFreeMemory(ctx->hContext, ctx->mszReaders);
    }

    SCardReleaseContext(ctx->hContext);
}

int aimepcsc_poll(struct aimepcsc_context *ctx, struct aime_data *data) {
    SCARDHANDLE hCard;
    SCARD_READERSTATE rs;
    LONG ret;
    LPBYTE pbAttr = NULL;
    DWORD cByte = SCARD_AUTOALLOCATE;
    int retval;

    retval = 1;

    memset(&rs, 0, sizeof(SCARD_READERSTATE));

    rs.szReader = ctx->mszReaders;
    rs.dwCurrentState = SCARD_STATE_UNAWARE;

    /* check if a card is present */
    ret = SCardGetStatusChange(ctx->hContext, 0, &rs, 1);

    if (ret == SCARD_E_TIMEOUT) {
        return 1;
    }

    if (ret != SCARD_S_SUCCESS) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "SCardGetStatusChange failed: %08lX", (ULONG) ret);
        return -1;
    }

    if (rs.dwEventState & SCARD_STATE_EMPTY) {
        return 1;
    }

    if (!(rs.dwEventState & SCARD_STATE_PRESENT)) {
        sprintf(ctx->last_error, "unknown dwCurrentState: %08lX", rs.dwCurrentState);
        return -1;
    }

    /* connect to card */
    ret = SCardConnect(ctx->hContext, rs.szReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, NULL);

    if (ret != SCARD_S_SUCCESS) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "SCardConnect failed: %08lX", (ULONG) ret);
        return -1;
    }

    /* get ATR string */
    ret = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, (LPBYTE) &pbAttr, &cByte);

    if (ret != SCARD_S_SUCCESS) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "SCardGetAttrib failed: %08lX", (ULONG) ret);
        return -1;
    }

    if (cByte != 20) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "invalid ATR length: %lu", cByte);
        goto out;
    }

    /* check ATR */
    if (memcmp(pbAttr, atr_ios14443_common, sizeof(atr_ios14443_common)) != 0) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "invalid card type.");
        goto out;
    }

    /* check card type */
    if (memcmp(pbAttr + sizeof(atr_ios14443_common), cardtype_m1k, sizeof(cardtype_m1k)) == 0) {
        data->card_type = Mifare;
        ret = read_m1k_aime(ctx, &hCard, data);
        if (ret < 0) {
            retval = -1;
            goto out;
        } else if (ret > 0) {
            goto out;
        }
    } else if (memcmp(pbAttr + sizeof(atr_ios14443_common), cardtype_felica, sizeof(cardtype_felica)) == 0) {
        data->card_type = FeliCa;
        ret = read_felica_aime(ctx, &hCard, data);
        if (ret < 0) {
            retval = -1;
            goto out;
        } else if (ret > 0) {
            goto out;
        }
    } else {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "invalid card type.");
        goto out;
    }

    retval = 0;

out:
    SCardFreeMemory(ctx->hContext, pbAttr);
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);

    return retval;
}

const char* aimepcsc_error(struct aimepcsc_context *ctx) {
    return ctx->last_error;
}

const char* aimepcsc_reader_name(struct aimepcsc_context *ctx) {
    return ctx->mszReaders;
}
