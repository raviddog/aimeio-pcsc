#pragma once

#include <windows.h>
#include <winioctl.h>
#include <winscard.h>

#include <stdint.h>

/* opaque context */
struct aimepcsc_context;

/* aime card types */
enum AIME_CARDTYPE {
    Mifare = 0x01,
    FeliCa = 0x02,
};

/* aime card data */
struct aime_data {
    /* AIME_CARDTYPE */
    uint8_t card_type;

    /* Card ID */
    uint8_t card_id[32];

    /* Card ID length */
    uint8_t card_id_len;
};

/* create new context for aimepcsc
 * @return context on success, NULL on failure
 */
struct aimepcsc_context* aimepcsc_create(void);

/* destroy context for aimepcsc */
void aimepcsc_destroy(struct aimepcsc_context *ctx);

/* setup reader (first detected reader will be used)
 * @param ctx context
 * @return 0 on success, -1 on failure
 */
int aimepcsc_init(struct aimepcsc_context *ctx);

/* shutdown reader */
void aimepcsc_shutdown(struct aimepcsc_context *ctx);

/* poll for card
 * @param ctx context
 * @param data data to be filled
 * @return 0 on success, -1 on failure, 1 on no card
 */
int aimepcsc_poll(struct aimepcsc_context *ctx, struct aime_data *data);

/* get last error
 * @param ctx context
 * @return error string
 */
const char* aimepcsc_error(struct aimepcsc_context *ctx);

/* get reader name
 * @param ctx context
 * @return reader name
 */
const char* aimepcsc_reader_name(struct aimepcsc_context *ctx);