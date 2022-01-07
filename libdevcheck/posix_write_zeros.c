#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "procedure.h"

struct posix_write_zeros_priv {
    int64_t start_lba;
    int64_t end_lba;
    int64_t lba_to_process;
    int fd;
    void *buf;
    uint64_t blk_index;
};
typedef struct posix_write_zeros_priv PosixWriteZerosPriv;

#define SECTORS_AT_ONCE 256
#define BLK_SIZE (SECTORS_AT_ONCE * 512) // FIXME hardcode

static int SuggestDefaultValue(DC_Dev *dev, DC_OptionSetting *setting) {
    (void)dev;
    if (!strcmp(setting->name, "start_lba")) {
        setting->value = strdup("0");
    } else {
        return 1;
    }
    return 0;
}

static int Open(DC_ProcedureCtx *ctx) {
    int r;
    PosixWriteZerosPriv *priv = ctx->priv;

    // Setting context
    ctx->blk_size = BLK_SIZE;
    priv->end_lba = ctx->dev->capacity / 512;
    priv->lba_to_process = priv->end_lba - priv->start_lba;
    ctx->progress.den = priv->lba_to_process / SECTORS_AT_ONCE;
    if (priv->lba_to_process % SECTORS_AT_ONCE)
        ctx->progress.den++;

    r = posix_memalign(&priv->buf, sysconf(_SC_PAGESIZE), ctx->blk_size);
    if (r)
        goto fail_buf;
    memset(priv->buf, 0, ctx->blk_size);

    priv->fd = open(ctx->dev->dev_path, O_WRONLY | O_DIRECT | O_LARGEFILE | O_NOATIME);
    if (priv->fd == -1) {
        dc_log(DC_LOG_FATAL, "open %s fail\n", ctx->dev->dev_path);
        goto fail_open;
    }
    lseek(priv->fd, 512 * priv->start_lba, SEEK_SET);
    r = ioctl(priv->fd, BLKFLSBUF, NULL);
    if (r == -1)
      dc_log(DC_LOG_WARNING, "Flushing block device buffers failed\n");
    return 0;

fail_open:
    free(priv->buf);
fail_buf:
    return 1;
}

static int Perform(DC_ProcedureCtx *ctx) {
    ssize_t write_ret;
    PosixWriteZerosPriv *priv = ctx->priv;
    size_t sectors_to_write = (priv->lba_to_process < SECTORS_AT_ONCE) ? priv->lba_to_process : SECTORS_AT_ONCE;

    // Updating context
    ctx->report.lba = priv->start_lba + SECTORS_AT_ONCE * priv->blk_index;
    ctx->report.sectors_processed = sectors_to_write;
    ctx->report.blk_status = DC_BlockStatus_eOk;
    priv->blk_index++;

    // Timing
    _dc_proc_time_pre(ctx);

    // Acting
    write_ret = write(priv->fd, priv->buf, sectors_to_write * 512);

    // Error handling
    if (write_ret != (int)sectors_to_write * 512) {
        // fd position is undefined, set it to write to next block
        lseek(priv->fd, 512 * priv->start_lba + ctx->blk_size * priv->blk_index, SEEK_SET);

        // Updating context
        ctx->report.blk_status = DC_BlockStatus_eError;
    }

    // Timing
    _dc_proc_time_post(ctx);

    // Updating context
    ctx->progress.num++;
    priv->lba_to_process -= sectors_to_write;

    return 0;
}

static void Close(DC_ProcedureCtx *ctx) {
    PosixWriteZerosPriv *priv = ctx->priv;
    free(priv->buf);
    close(priv->fd);
}

static DC_ProcedureOption options[] = {
    { "start_lba", "set LBA address to begin from", offsetof(PosixWriteZerosPriv, start_lba), DC_ProcedureOptionType_eInt64 },
    { NULL }
};

DC_Procedure posix_write_zeros = {
    .name = "posix_write_zeros",
    .display_name = "Write zeros",
    .help = "Fills device space with zeros. Uses POSIX write() call, in direct mode",
    .flags = DC_PROC_FLAG_INVASIVE,
    .suggest_default_value = SuggestDefaultValue,
    .open = Open,
    .perform = Perform,
    .close = Close,
    .priv_data_size = sizeof(PosixWriteZerosPriv),
    .options = options,
};

