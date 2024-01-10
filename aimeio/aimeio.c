#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "aimeio/aimeio.h"
#include "aimepcsc/aimepcsc.h"

static struct aimepcsc_context *ctx;
static struct aime_data data;

uint16_t aime_io_get_api_version(void)
{
    return 0x0100;
}

HRESULT aime_io_init(void)
{
    int ret;
    FILE* fp;

    ret = AllocConsole();

    // someone might already allocated a console - seeing this on fufubot's segatools
    if (ret != 0) {
        // only when we allocate a console, we need to redirect stdout
        freopen_s(&fp, "CONOUT$", "w", stdout);
    }

    ctx = aimepcsc_create();
    if (!ctx) {
        return E_OUTOFMEMORY;
    }

    ret = aimepcsc_init(ctx);

    if (ret != 0) {
        printf("aimeio-pcsc: failed to initialize: %s\n", aimepcsc_error(ctx));
        aimepcsc_destroy(ctx);
        ctx = NULL;
        return E_FAIL;
    }

    printf("aimeio-pcsc: initialized with reader: %s\n", aimepcsc_reader_name(ctx));

    return S_OK;
}

HRESULT aime_io_nfc_poll(uint8_t unit_no)
{
    int ret;
    HRESULT hr;

    if (unit_no != 0) {
        return S_OK;
    }

    memset(&data, 0, sizeof(data));

    ret = aimepcsc_poll(ctx, &data);

    if (ret < 0) {
        printf("aimeio-pcsc: poll failed: %s\n", aimepcsc_error(ctx));
    }

    return S_OK;
}

HRESULT aime_io_nfc_get_aime_id(
        uint8_t unit_no,
        uint8_t *luid,
        size_t luid_size)
{
    assert(luid != NULL);

    if (unit_no != 0 || data.card_type != Mifare) {
        return S_FALSE;
    }

    assert(luid_size == data.card_id_len);

    memcpy(luid, data.card_id, luid_size);

    return S_OK;
}

HRESULT aime_io_nfc_get_felica_id(uint8_t unit_no, uint64_t *IDm)
{
    uint64_t val;
    size_t i;

    assert(IDm != NULL);

    if (unit_no != 0 || data.card_type != FeliCa) {
        return S_FALSE;
    }

    val = 0;

    for (i = 0 ; i < 8 ; i++) {
        val = (val << 8) | data.card_id[i];
    }

    *IDm = val;

    return S_OK;
}

void aime_io_led_set_color(uint8_t unit_no, uint8_t r, uint8_t g, uint8_t b)
{}
