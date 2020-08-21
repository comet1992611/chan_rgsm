#include "rgsm_dfu.h"
#include "atomics.h"
#include "rgsm_utilities.h"
#include "rgsm_timers.h"

#define DFU_BLOCK_SIZE  120

#define ERROR_DFU(fmt, ...) ast_log(AST_LOG_ERROR, fmt, ## __VA_ARGS__)
#define DEBUG_DFU(fmt, ...) ast_log(AST_LOG_DEBUG, fmt, ## __VA_ARGS__)

typedef enum dfu_format {
    DFU_FORMAT_RAW = 0,
    DFU_FORMAT_ENCODED = 1,
} dfu_format_t;

typedef enum dfu_mcu_id {
    MCU_XMEGA0  = 0,
    MCU_BF      = 16
} dfu_mcu_id_t;

#define MCU_ID_MIN  MCU_XMEGA0
#define MCU_ID_MAX  MCU_BF

#define MCU_ID_IS_XMEGA(mcu_id) ((mcu_id) < MCU_BF)
#define MCU_ID_IS_BF(mcu_id) ((mcu_id) == MCU_BF)
#define MCU_ID_IS_VALID(mcu_id) ((mcu_id) == MCU_XMEGA0 || (mcu_id) == MCU_BF)

#define MANIFEST_SIZE    2

typedef enum dfu_section {
    SECTION_BOOT    = 0,
    SECTION_APP     = 1,
    SECTION_BACKUP  = 2,
    SECTION_NEWAPP  = 3
} dfu_section_t;

typedef enum dfu_cmd {
    CMD_MAKE_BACKUP     = 0,
    CMD_RESTORE_BACKUP  = 1,
    CMD_UPLOAD_BLOCK    = 2,
    CMD_FLASH           = 3,
    CMD_CHECK_SECTION   = 4,
    CMD_SECTION_VERSION = 5,
    CMD_RUN_SECTION     = 6,
    CMD_CURRENT_MODE    = 7
} dfu_cmd_t;

typedef struct dfu_manifest_record {
    dfu_mcu_id_t   mcu_id;
    dfu_format_t   format;
    char           img_path[512];
    int            upgraded:1;
    int            skipped:1;
    int            rolledback:1;
    int            backedup:1;
} dfu_manifest_record_t;

//hardcoded manifest size
typedef dfu_manifest_record_t* dfu_jobs_t[MANIFEST_SIZE];

atomic_t    is_dfu;


AST_MUTEX_DEFINE_STATIC(dfu_mtx);
static int clifd;
static dfu_jobs_t dfu_jobs = {NULL, NULL};


//statics
static void _cmd_rewrite_section(struct ggw8_device *dev, int mcu_id, uint8_t cmd, uint8_t *op_status)
{
    int bytes_received;
    uint8_t req_buf[2];
    uint8_t resp_buf[3];

    req_buf[0] = cmd;
    req_buf[1] = mcu_id;

    *op_status = DFU_STAT_COMMERROR;

    if (ggw8_dfu_request(dev, req_buf, sizeof(req_buf), resp_buf, sizeof(resp_buf), &bytes_received) == 0) {
        *op_status = resp_buf[2];
    }
}

static void _cmd_run_section(struct ggw8_device *dev, int mcu_id, dfu_section_t section, uint8_t *op_status)
{
    int bytes_received;
    uint8_t req_buf[3];
    uint8_t resp_buf[3];

    req_buf[0] = CMD_RUN_SECTION;
    req_buf[1] = mcu_id;
    req_buf[2] = section;

    *op_status = DFU_STAT_COMMERROR;

    if (ggw8_dfu_request(dev, req_buf, sizeof(req_buf), resp_buf, sizeof(resp_buf), &bytes_received) == 0) {
        *op_status = resp_buf[2];
    }
}

#define GGW8_MANIFEST_NAME  "ggw8-dfu.manifest"

//return true in success othewise false
//fw_dir should not be blank
static int parse_dfu_manifest(const char *fw_dir, dfu_jobs_t jobs)
{
    FILE *f;
    char dir[512];
    char file_path[512];
    int mcu_id;
    int format;
    char img_file[32];
    int i, job_indx;
    int ret = 0;
    struct dfu_manifest_record *job;

    //canonize directory
    strncpy(dir, fw_dir, sizeof(dir));
    if (fw_dir[strlen(fw_dir)-1] != '/') {
        strcat(dir, "/");
    }


    sprintf(file_path, "%s%s", dir, GGW8_MANIFEST_NAME);
    ast_cli(clifd, "rgsm: Parse a manifest: \"%s\"\n", file_path);

    f = fopen(file_path, "r");
    if (!f) {
        ast_cli(clifd, "  Manifest not found\n");
        return 0;
    }

    job_indx = 0;
    while (job_indx < MANIFEST_SIZE) {
        i = fscanf(f, "%d%*[=]%d%*[ ]%s", &mcu_id, &format, img_file);
        if (ferror(f)) {
            ast_cli(clifd, "  File reading error\n");
            goto exit_;
        } else if (i != 3) {
            if ((feof(f))) break;
            ast_cli(clifd, "  Manifest record malformed: argc=%d\n", i);
            goto exit_;
        }

        if (!MCU_ID_IS_VALID(mcu_id)) {
            ast_cli(clifd, "  Invalid mcu_id=%d\n", mcu_id);
            goto exit_;
        }

        if ((format != DFU_FORMAT_RAW) && (format != DFU_FORMAT_ENCODED)) {
            ast_cli(clifd, "  Invalid image format=%d specified\n", format);
            goto exit_;
        }

        ast_cli(clifd, "  Manifest record read: mcu_id=%d, format=%d, img_file=\"%s\"\n", mcu_id, format, img_file);
        sprintf(file_path, "%s%s", dir, img_file);

        //allocate job
        job = (struct dfu_manifest_record*)calloc(1, sizeof(struct dfu_manifest_record));
        job->mcu_id = (dfu_mcu_id_t)mcu_id;
        job->format = (dfu_format_t)format;
        strncpy(job->img_path, file_path, sizeof(job->img_path));
        jobs[job_indx] = job;
        job_indx++;
    }

    ret = 1;
    ast_cli(clifd, "  Manifest Ok: records=%d\n", job_indx);
exit_:
    fclose(f);
    return ret;
}

static char tmp_buf[64];

static const char* mcu_id_str(int mcu_id)
{
    if (mcu_id < MCU_BF) {
        sprintf(tmp_buf, "XMEGA_%d", mcu_id);
        return tmp_buf;
    } else {
        return "BF";
    }
}

//! start of dfu commands
//app->backup
static void dfu_make_backup(struct ggw8_device *dev, int mcu_id, uint8_t *op_status)
{
    _cmd_rewrite_section(dev, mcu_id, CMD_MAKE_BACKUP, op_status);
}

static void dfu_current_section(struct ggw8_device *dev, int mcu_id, uint8_t *op_status, uint8_t *section)
{
    int bytes_received;
    uint8_t req_buf[2];
    uint8_t resp_buf[4];

    req_buf[0] = CMD_CURRENT_MODE;
    req_buf[1] = mcu_id;

    *op_status = DFU_STAT_COMMERROR;

    if (ggw8_dfu_request(dev, req_buf, sizeof(req_buf), resp_buf, sizeof(resp_buf), &bytes_received) == 0) {
        *op_status = resp_buf[2];
        *section = resp_buf[3];
        if ((*section != (uint8_t)SECTION_BOOT) && (*section != (uint8_t)SECTION_APP)) {
            *section = -1;
        }
        DEBUG_DFU("Current section=%d, status=%d\n", *section, *op_status);
    }
}

static void dfu_switch_to_boot(struct ggw8_device **dev, int mcu_id, uint8_t *op_status)
{
    //int cnt;
    //uint8_t cur_sect;

    //BF must be already in DFU
    if (mcu_id == MCU_BF) {
        *op_status = DFU_STAT_SUCCESS;
        return;
    }

    _cmd_run_section(*dev, mcu_id, SECTION_BOOT, op_status);
}

static void dfu_switch_to_app(struct ggw8_device **dev, int mcu_id, uint8_t *op_status)
{
    //int cnt;
    //uint8_t cur_sect;

    if (mcu_id == MCU_BF) {
        *op_status = DFU_STAT_SUCCESS;
        return;
    }

    _cmd_run_section(*dev, mcu_id, SECTION_APP, op_status);
}

//backup->app
static void dfu_restore_backup(struct ggw8_device *dev, int mcu_id, uint8_t *op_status)
{
    _cmd_rewrite_section(dev, mcu_id, CMD_RESTORE_BACKUP, op_status);
}

//newapp->app
static void dfu_flash(struct ggw8_device *dev, int mcu_id, uint8_t *op_status)
{
    _cmd_rewrite_section(dev, mcu_id, CMD_FLASH, op_status);
}

static void dfu_upload_block(struct ggw8_device *dev, int mcu_id, uint32_t block_no, uint8_t format, uint8_t *block,
                             int block_size, uint8_t *op_status)
{
    int rc;
    int bytes_received;
    //cmd(1)+mcu_id(1)+block_no(4)+format(1)
    int header_size = 3+sizeof(uint32_t);
    unsigned char *req_buf = (uint8_t*)malloc(header_size+block_size);
    unsigned char resp_buf[3];

    *op_status = DFU_STAT_COMMERROR;

    *req_buf = CMD_UPLOAD_BLOCK;
    *(req_buf+1) = mcu_id;
    *(uint32_t*)(req_buf+2) = block_no;
    *(req_buf+6) = format;

    memcpy(req_buf+header_size, block, block_size);
    rc = ggw8_dfu_request(dev, req_buf, header_size+block_size, resp_buf, 3, &bytes_received);
    free(req_buf);

    if (rc == 0) {
        *op_status = resp_buf[2];
    }
}

static void dfu_check_sect(struct ggw8_device *dev, int mcu_id, dfu_section_t section, uint8_t *op_status)
{
    int bytes_received;
    unsigned char req_buf[3];
    unsigned char resp_buf[3];

    *op_status = DFU_STAT_COMMERROR;

    req_buf[0] = CMD_CHECK_SECTION;
    req_buf[1] = mcu_id;
    req_buf[2] = section;

    if (ggw8_dfu_request(dev, req_buf, sizeof(req_buf), resp_buf, sizeof(resp_buf), &bytes_received) == 0) {
        *op_status = resp_buf[2];
    }
}

static void dfu_sect_version(struct ggw8_device *dev, int mcu_id, dfu_section_t section, uint16_t *sect_version, uint8_t *op_status)
{
    int bytes_received;
    unsigned char req_buf[3];
    unsigned char resp_buf[3+sizeof(uint16_t)];

    req_buf[0] = CMD_SECTION_VERSION;
    req_buf[1] = mcu_id;
    req_buf[2] = section;

    *sect_version = 0;
    *op_status = DFU_STAT_COMMERROR;

    if (ggw8_dfu_request(dev, req_buf, sizeof(req_buf), resp_buf, sizeof(resp_buf), &bytes_received) == 0) {
        *op_status = resp_buf[2];
        *sect_version = *(uint16_t*)(resp_buf+3);
    }
}
//! end of dfu commands

//! helpers to program MCU
static void upgrade_mcu_fw(struct ggw8_device *dev, dfu_manifest_record_t *job)
{
    uint8_t op_status;
    uint8_t buf[DFU_BLOCK_SIZE];
    size_t bytes_read;
    long img_size;
    uint16_t version;
    long total_bytes_written = 0;
    int block_no = 0;
    FILE *f;
    dfu_section_t curr_section;

    ast_cli(clifd, "  Start DFU mcu=%s: img_file=\"%s\"\n", mcu_id_str(job->mcu_id), job->img_path);

    f = fopen(job->img_path, "r");
    if (!f) {
        ast_cli(clifd, "    Skip DFU mcu=%s: img_file=\"%s\" not found\n", mcu_id_str(job->mcu_id), job->img_path);
        job->skipped = 1;
        goto exit_;
    }

    // obtain file size:
    fseek(f, 0, SEEK_END);
    img_size = ftell(f);
    rewind(f);

    if (!img_size) {
        ast_cli(clifd, "    Skip DFU mcu=%s: img_file is empty\n", mcu_id_str(job->mcu_id));
        job->skipped = 1;
        goto exit_;
    }
    ast_cli(clifd, "    Image size=%ld bytes\n", img_size);

    //step#1: switch to boot section
    ast_cli(clifd, "    #1 Goto BOOTLOADER...\n");
    dfu_switch_to_boot(&dev, job->mcu_id, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Error switching mcu=%s to boot mode: status=0x%.2x\n", mcu_id_str(job->mcu_id), op_status);
        goto exit_;
    }

    curr_section = 0;
    dfu_current_section(dev, job->mcu_id, &op_status, (uint8_t*)&curr_section);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Error retrieving current section: status=0x%.2x\n", op_status);
    } else {
        ast_cli(clifd, "      Current section=%d\n", curr_section);
    }


    if (MCU_ID_IS_BF(job->mcu_id)) {
        dfu_sect_version(dev, job->mcu_id, curr_section, &version, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ast_cli(clifd, "      Error retrieving version for currect section: status=0x%.2x\n", op_status);
        } else {
            ast_cli(clifd, "      Passed: current section version=0x%.4x\n", version);
        }
    } else {
        dfu_sect_version(dev, job->mcu_id, SECTION_APP, &version, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ast_cli(clifd, "      Error retrieving version for app section: status=0x%.2x\n", op_status);
        } else {
            ast_cli(clifd, "      Passed: current APP FW version=0x%.4x\n", version);
        }
    }

    ast_cli(clifd, "    #2 Checking APP section...\n");
    if (MCU_ID_IS_BF(job->mcu_id)) {
        ast_cli(clifd, "      Skipped for BF\n");
    } else {
        dfu_check_sect(dev, job->mcu_id, SECTION_APP, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ast_cli(clifd, "      Could not verify APP section: status=%d. Skipping backup\n", op_status);
            goto upload_;
        }
        ast_cli(clifd, "      Passed: status=OK\n");
    }

    ast_cli(clifd, "    #3 Make backup of current version...\n");
    if (MCU_ID_IS_BF(job->mcu_id) && (curr_section != SECTION_APP)) {
        ast_cli(clifd, "      Skipped for BF because current section is not APP\n");
    } else {
        dfu_make_backup(dev, job->mcu_id, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ast_cli(clifd, "      Could not make backup: status=0x%.2x\n", op_status);
            goto app_mode_;
        }
        ast_cli(clifd, "      Passed\n");
    }

    ast_cli(clifd, "    #4 Checking BACKUP section...\n");
    if (MCU_ID_IS_BF(job->mcu_id)) {
        ast_cli(clifd, "      Skipped for BF\n");
    } else {
        dfu_check_sect(dev, job->mcu_id, SECTION_BACKUP, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ast_cli(clifd, "      Could not verify BACKUP section: status=%d\n", op_status);
            goto app_mode_;
        }
        job->backedup = 1;
        ast_cli(clifd, "      Passed: status=OK\n");
    }

upload_:
    //!for xmega new image placed in NEWAPP section, for DF new image placed in APP section
    ast_cli(clifd, "    #5 Uploading image...\n");
    while (1) {
        bytes_read = fread(buf, 1, sizeof(buf), f);
        if (bytes_read != sizeof(buf)) {
            if (ferror(f)) {
                ERROR_DFU("      Error reading image file: errno=%d\n", errno);
                goto rollback_;
            } else if (bytes_read == 0) {
                //zero length block, don't transmit it to device
                break;
            }
        }

        dfu_upload_block(dev, job->mcu_id, block_no, job->format, buf, bytes_read, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ast_cli(clifd, "      Error uploading block to device: block_no=%d, bytes=%d, status=0x%.2x\n", block_no, bytes_read, op_status);
            goto app_mode_;
        }

        DEBUG_DFU("Block uploaded: block_no=%d, bytes=%d\n", block_no, bytes_read);
        block_no++;
        total_bytes_written += bytes_read;
    }
    ast_cli(clifd, "      Passed: uploaded bytes=%ld\n", total_bytes_written);

    //step#3: verify newapp section
    ast_cli(clifd, "    #6 Checking section with new image...\n");
    if (MCU_ID_IS_BF(job->mcu_id)) {
        //will be verified by device at flash time
        ast_cli(clifd, "      Skipped for BF\n");
    } else {
        dfu_check_sect(dev, job->mcu_id, SECTION_NEWAPP, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ast_cli(clifd, "      Could not verify section with new image: status=%d\n", op_status);
            //no need to do rollback because APP section still valid for xmega
            goto app_mode_;
        }
        dfu_sect_version(dev, job->mcu_id, SECTION_NEWAPP, &version, &op_status);
        ast_cli(clifd, "      Passed: status=OK, version=0x%.4x\n", version);
    }

    //step#7: rewrite APP from NEWAPP, valid only for xmega
    ast_cli(clifd, "    #7 Flashing APP section...\n");
    dfu_flash(dev, job->mcu_id, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        //we have backup copy, so fw may be restored
        ast_cli(clifd, "      Could not write APP section: status=%d\n", op_status);
        goto rollback_;
    }
    ast_cli(clifd, "      Passed: status=OK\n");

    //step#8: verify app section
    ast_cli(clifd, "    #8 Checking APP section...\n");
    if (MCU_ID_IS_BF(job->mcu_id)) {
        //will be verified by device at flash time
        ast_cli(clifd, "      Skipped for BF\n");
    } else {
        dfu_check_sect(dev, job->mcu_id, SECTION_APP, &op_status);
        if (op_status != DFU_STAT_SUCCESS) {
            ERROR_DFU("    Could not verify APP section: status=%d\n", op_status);
            goto rollback_;
        }
        ast_cli(clifd, "      Passed: status=OK\n");
    }

    //if all steps above complete then result is true
    job->upgraded = 1;
    //finish dfu by exiting boot mode
    goto app_mode_;

rollback_:
    if (!job->backedup) {
        ast_cli(clifd, "    Skipping Rollback: backup not present\n");
        goto app_mode_;
    }

    ast_cli(clifd, "    Rolling back to previous version...\n");
    dfu_restore_backup(dev, job->mcu_id, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Could not restore previous version: status=%d\n", op_status);
        goto app_mode_;
    }
    job->rolledback = 1;
    ast_cli(clifd, "      Passed: status=OK\n");

app_mode_:
    ast_cli(clifd, "    Exiting BOOTLOADER...\n");
    dfu_switch_to_app(&dev, job->mcu_id, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Error switching mcu=%s to application mode: status=0x%.2x\n", mcu_id_str(job->mcu_id), op_status);
        goto exit_;
    }
    ast_cli(clifd, "      Passed: status=OK\n");
exit_:
    if (f) fclose(f);
    ast_cli(clifd, "  Complete DFU mcu=%s: status=%s\n", mcu_id_str(job->mcu_id), job->skipped ? "SKIPPED" : (job->upgraded ? "OK" : "ERROR"));
}

void rollback_mcu_fw(struct ggw8_device *dev, dfu_manifest_record_t *job)
{
    uint8_t op_status;


    ast_cli(clifd, "  Rolling back FW mcu=%s\n",  mcu_id_str(job->mcu_id));
    if (!job->upgraded || job->rolledback || !job->backedup) {
        ast_cli(clifd, "    Nothing to rollback for mcu=%s\n",  mcu_id_str(job->mcu_id));
        return;
    }

    //step#1: switch to boot section
    ast_cli(clifd, "    #1 - Goto BOOTLOADER\n");
    dfu_switch_to_boot(&dev, job->mcu_id, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Error switching mcu=%s to boot mode: status=0x%.2x\n", mcu_id_str(job->mcu_id), op_status);
        goto exit_;
    }
    ast_cli(clifd, "      Passed\n");
        //step#2: verify backup section
    ast_cli(clifd, "    #2 Checking BACKUP section...\n");
    dfu_check_sect(dev, job->mcu_id, SECTION_BACKUP, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Could not verify BACKUP section: status=%d\n", op_status);
        goto app_mode_;
    }
    ast_cli(clifd, "      Passed: status=OK\n");

    //step#3
    ast_cli(clifd, "    #3 Rolling back to previous version...\n");
    dfu_restore_backup(dev, job->mcu_id, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Could not restore previous version: status=%d\n", op_status);
        goto app_mode_;
    }
    ast_cli(clifd, "      Passed: status=OK\n");

    //step#4: verify app section
    ast_cli(clifd, "    #4 Checking APP section...\n");
    dfu_check_sect(dev, job->mcu_id, SECTION_APP, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "      Could not verify APP section: status=%d\n", op_status);
        goto app_mode_;
    }
    job->rolledback = 1;
    ast_cli(clifd, "      Passed: status=OK\n");

app_mode_:
    ast_cli(clifd, "    Exiting BOOTLOADER\n");
    dfu_switch_to_app(&dev, job->mcu_id, &op_status);
    if (op_status != DFU_STAT_SUCCESS) {
        ast_cli(clifd, "    Error switching mcu=%s to application mode: status=0x%.2x\n", mcu_id_str(job->mcu_id), op_status);
        goto exit_;
    }

exit_:
    ast_cli(clifd, "  Rolled back DFU mcu=%s: status=%s\n", mcu_id_str(job->mcu_id), job->rolledback ? "OK" : "ERROR");
}


int rgsm_gw_dfu(struct gateway **gw, const char *manifest, int ast_clifd)
{
    int i, gw_uid;
    struct gateway *gw_tmp = *gw;
    struct gateway *gw_dfu;
    dfu_manifest_record_t *job;
    int is_rollback = 0;
    int is_upgraded = 0;
    struct rgsm_timer timer;
    struct timeval tv;

    //should be singletone
    if (ast_mutex_trylock(&dfu_mtx)) {
        ast_cli(ast_clifd, "rgsm: A concurrent DFU is in progress. Your request declined");
        return -1;
    }

    atomic_set(&is_dfu, 1);
    clifd = ast_clifd;

    ast_cli(clifd, "rgsm: GGW-8 FW upgrade started\n");

    //check manifest
    if (!parse_dfu_manifest(manifest, dfu_jobs)) {
        ast_cli(clifd, "rgsm:  !!! Skip DFU: manifest error !!!\n");
        goto exit_;
    }

    ast_cli(clifd, "rgsm: Restarting GGW-8 and wait until it appears in DFU mode...\n");

    gw_dfu = NULL;
    gw_uid = gw_tmp->uid;

    //restart device in dfu mode, *gw_tmp becomes invalid after device restart
    ggw8_restart_device(gw_tmp->ggw8_device);

    //5 seconds to shutdown device
    tv_set(tv, 5, 0);
    rgsm_timer_set(timer, tv);
    while (!is_rgsm_timer_fired(timer)) {
        us_sleep(1000000);
    }

    //20 seconds to wait device in DFU mode
    tv_set(tv, 20, 0);
    rgsm_timer_set(timer, tv);

_check:
    if (is_rgsm_timer_fired(timer)) {
        ast_cli(clifd, "rgsm: DFU mode timedout. Please reload channel drived\n");
        goto exit_;
    }
    //wait gateway with the same id
	ast_mutex_lock(&rgsm_lock);
    AST_LIST_TRAVERSE (&gateways, gw_tmp, link)
    {
        if (gw_tmp->uid == gw_uid) {
            gw_dfu = gw_tmp;
            break;
        }
	}
	ast_mutex_unlock(&rgsm_lock);

    if (!gw_dfu) {
        us_sleep(1000000);
        goto _check;
    }
    goto exec_jobs_;

dev_rollback_:
    is_rollback = 1;
    is_upgraded = 0;
    ast_cli(clifd, "rgsm: Rolling back FW\n");

exec_jobs_:
    for (i=0; i < MANIFEST_SIZE; i++) {
        //
        job = dfu_jobs[i];
        if (!job) continue;

        //job.mcu_id type already checked
        if (!is_rollback) {
            upgrade_mcu_fw(gw_dfu->ggw8_device, job);
            if (job->skipped) continue;
            if (!job->upgraded) goto dev_rollback_;
            is_upgraded = 1;
        } else {
            rollback_mcu_fw(gw_dfu->ggw8_device, job);
        }
    }

exit_:

    ast_cli(clifd, "rgsm: GGW-8 FW upgrade complete: status=\"%s\"\n", is_upgraded ? "UPGRADED" : "NOT UPGRADED");

    //cleanup
    for (i = 0; i < sizeof(dfu_jobs)/sizeof(void*); i++) {
        if (dfu_jobs[i]) {
            free(dfu_jobs[i]);
            dfu_jobs[i] = NULL;
        }
    }

    atomic_set(&is_dfu, 0);
    *gw = gw_dfu;
    ast_mutex_unlock(&dfu_mtx);

    if (gw_dfu) {
        ggw8_device_ctl(gw_dfu->ggw8_device, GGW8_CTL_RESET);
        ggw8_restart_device(gw_dfu->ggw8_device);
    }

    if (is_upgraded) {
        ast_cli(clifd, "rgsm: Restarting GGW-8 to apply firmware changes...\n");
        ast_cli(clifd, "rgsm: IF IT WON'T BE REDISCOVERED AUTOMATICALLY WITHIN %d SECONDS, PLEASE REBOOT A BOX\n", GGW8_DISCOVERY_PERIOD);
    }

    return 0;
}

